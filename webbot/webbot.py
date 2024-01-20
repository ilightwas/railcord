from concurrent.futures import ThreadPoolExecutor
import hashlib
import logging
import pathlib
import sys
import time
from configparser import ConfigParser
from functools import partial
from typing import List

import httpx
from flask import jsonify
from selenium import webdriver
from selenium.common.exceptions import (
    NoSuchElementException,
    TimeoutException,
    WebDriverException,
)
from selenium.webdriver.chrome.service import Service as ChromeService
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.by import By
from selenium.webdriver.remote.webdriver import WebDriver
from selenium.webdriver.remote.webelement import WebElement
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.wait import WebDriverWait
from webdriver_manager.chrome import ChromeDriverManager

DRIVER_IMP_WAIT = 3
DEFAULT_WEBDRIVER_WAIT = 10
DRIVER_USERDATA_DIR = "userdata"
DEFAULT_CONFIG_PATH = "lucy.ini"
COOKIE_ALIVE_INTERVAL = 60 * 20
GAMEWORLD_LOAD_WAIT = 10


def setup_logger() -> logging.Logger:
    logger = logging.getLogger("webbot")
    logger.setLevel(logging.INFO)

    console_handler = logging.StreamHandler(stream=sys.stdout)
    console_handler.setLevel(logging.INFO)

    file_handler = logging.FileHandler("webbot.log")
    file_handler.setLevel(logging.INFO)

    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
    formatter.datefmt = "%Y-%m-%d %H:%M:%S"
    console_handler.setFormatter(formatter)
    file_handler.setFormatter(formatter)

    logger.addHandler(console_handler)
    logger.addHandler(file_handler)
    return logger


class WebbotConfig:
    def __init__(self, config_file_path: str = DEFAULT_CONFIG_PATH):
        self.config_parser: ConfigParser = ConfigParser()
        if not self.config_parser.read(config_file_path):
            raise Exception("Config file could not be loaded")

    def __getitem__(self, setting_name: str):
        section, name = setting_name.split(".")
        return self.config_parser[section][name]


logger = setup_logger()
cfg: WebbotConfig = WebbotConfig()


class Webbot:
    def __init__(self):
        self.cookies: List[dict] = None
        self.current_world_url: str = None
        self.valid_session: bool = False
        self.alive_count: int = 0

        chrome_options = webdriver.ChromeOptions()

        if cfg["Webdriver.headless"] == str(1):
            chrome_options.add_argument("--headless")
            chrome_options.add_argument("--no-sandbox")
            chrome_options.add_argument("--disable-notifications")
            chrome_options.add_argument("--window-size=1920,1080")

        proxy = cfg["Webdriver.proxy"]
        if proxy:
            chrome_options.add_argument(f"--proxy-server={proxy}")

        spoofed_ua = cfg["Webdriver.spoofed_ua"]
        if spoofed_ua:
            chrome_options.add_argument(f"--user-agent={spoofed_ua}")

        datadir = pathlib.Path().absolute().joinpath(DRIVER_USERDATA_DIR)
        chrome_options.add_argument(f"--user-data-dir={datadir}")

        profile = cfg["Webdriver.profile"]
        if profile:
            chrome_options.add_argument(f"--profile-directory={profile}")
        else:
            chrome_options.add_argument("--profile-directory=default")

        try:
            self.driver: WebDriver = webdriver.Chrome(
                service=ChromeService(ChromeDriverManager().install()),
                options=chrome_options,
            )
            self.driver.implicitly_wait(DRIVER_IMP_WAIT)
        except WebDriverException as e:
            logger.warning(f"Error: {e}")
            self.driver.quit()
            raise

        self.client: httpx.Client = httpx.Client()
        self.client.headers.update({"User-Agent": cfg["Webdriver.spoofed_ua"]})
        self.executor: ThreadPoolExecutor = ThreadPoolExecutor()

    def check_for_login_prompt(self) -> List[WebElement]:
        wait = WebDriverWait(self.driver, DEFAULT_WEBDRIVER_WAIT)
        iframes = []
        try:
            iframe = wait.until(
                EC.presence_of_element_located(
                    (By.CLASS_NAME, cfg["Login.popup_form_class"])
                )
            ).find_element(By.TAG_NAME, "iframe")
            iframes.append(iframe)
            self.driver.switch_to.frame(iframe)
            iframe = wait.until(EC.presence_of_element_located((By.TAG_NAME, "iframe")))
            iframes.append(iframe)
            self.driver.switch_to.frame(iframe)
            form = wait.until(
                EC.visibility_of_element_located((By.ID, cfg["Login.form_id"]))
            )

            logger.info("Located login prompt iframes")
            self.driver.switch_to.default_content()
            return iframes
        except:
            logger.info("No login prompt found")
            return None

    def do_login(self, login_iframes: List[WebElement]) -> None:
        wait = WebDriverWait(self.driver, DEFAULT_WEBDRIVER_WAIT)
        for iframe in login_iframes:
            self.driver.switch_to.frame(iframe)

        form = wait.until(
            EC.visibility_of_element_located((By.ID, cfg["Login.form_id"]))
        )

        email_field = wait.until(
            EC.element_to_be_clickable((By.NAME, cfg["Login.email_name"]))
        )

        password_field = wait.until(
            EC.element_to_be_clickable((By.NAME, cfg["Login.password_name"]))
        )

        login_submit = wait.until(
            EC.element_to_be_clickable((By.NAME, cfg["Login.submit_name"]))
        )

        ActionChains(self.driver).pause(1).send_keys_to_element(
            email_field, cfg["Login.email"]
        ).send_keys_to_element(password_field, cfg["Login.password"]).pause(
            1
        ).move_to_element(
            login_submit
        ).click().perform()
        self.driver.switch_to.default_content()

    def is_logged(self, go_to_lobby: bool = False) -> bool:
        if go_to_lobby:
            self.driver.get(cfg["Gameworld.url"])

        wait = WebDriverWait(self.driver, DEFAULT_WEBDRIVER_WAIT)
        try:
            wait.until(
                EC.presence_of_element_located((By.ID, cfg["Login.logged_in_id"]))
            )
            return True
        except:
            return False

    def login(self) -> None:
        wait = WebDriverWait(self.driver, DEFAULT_WEBDRIVER_WAIT)

        if self.is_logged(go_to_lobby=True):
            logger.info("Already logged in")
            return
        else:
            logger.info("Logging in..")

        iframes = self.check_for_login_prompt()
        if iframes:
            self.do_login(iframes)
            if self.is_logged():
                logger.info("Login successful")
                return
        try:
            no_cookie_btn = wait.until(
                EC.visibility_of_element_located(
                    (By.ID, cfg["Login.no_cookies_btn_id"])
                )
            )
            logger.info("Clicking no cookies button")
            no_cookie_btn.click()
        except TimeoutException:
            logger.info("No cookies prompt")

        try:
            wait.until(
                EC.visibility_of_element_located((By.ID, cfg["Login.btn_id"]))
            ).click()

            iframes = self.check_for_login_prompt()
            if not iframes:
                logger.info("No login prompt found")
                return

            self.do_login(iframes)
            if self.is_logged():
                logger.info("Login successful")
        except:
            logger.warning("Some element failed to be found, login failed")

    def join_gameworld(self, world_name: str = None) -> None:
        start_time = time.time()
        if not world_name:
            world_name = cfg["Gameworld.join"]

        self.driver.get(cfg["Gameworld.url"])
        worlds = self.get_game_worlds()
        for world in worlds:
            if world_name in world:
                logger.info(f"World {world} selected")
                ActionChains(self.driver).move_to_element(worlds[world]).pause(
                    1
                ).click().perform()
                login_iframes = self.check_for_login_prompt()
                if login_iframes:
                    logger.info("Joining gameworld stopped by login..")
                    try:
                        self.do_login(login_iframes)
                        logger.info("Login successfull (at joining world)")
                    except:
                        logger.warning("Login try at joining world failed")
                        raise

                time.sleep(GAMEWORLD_LOAD_WAIT)
                logger.info(f"Updating session state for gameworld: {world_name}")
                self.update_session_state()
                logger.info(
                    f"Time taken to join gameworld {time.time() - start_time:.2f}"
                )
                return

        logger.warning("Gameworld not found")

    def update_session_state(self) -> None:
        self.update_cookies()

        self.current_world_url = self.driver.current_url
        logger.info(f"Set current world url to {self.current_world_url}")

        self.driver.get(cfg["Gameworld.url"])

        self.valid_session = True
        self.alive_count = 0
        self.executor.submit(partial(self.keep_cookies_alive, self.current_world_url))

    def update_cookies(self) -> None:
        self.cookies = self.driver.get_cookies()
        self.client.cookies = self.convert_cookies()
        with open("lastcookies.txt", "w") as file:
            file.write(str(self.cookies))

    def get_game_worlds(self) -> dict:
        logger.info("Obtaining the gameworld list")
        wait = WebDriverWait(self.driver, DEFAULT_WEBDRIVER_WAIT)
        gm_worlds = {}

        try:
            gm_cards = wait.until(
                EC.visibility_of_all_elements_located(
                    (By.CLASS_NAME, cfg["Gameworld.gm_card_class"])
                )
            )

            for e in gm_cards:
                world_name = e.find_element(
                    By.CLASS_NAME, cfg["Gameworld.gm_title_class"]
                )
                logger.info(f"world name: {world_name.text}")
                play_btn = e.find_element(
                    By.CLASS_NAME, cfg["Gameworld.gm_play_class"]
                ).find_element(By.CLASS_NAME, "button")

                gm_worlds[world_name.text.lower()] = play_btn
        except TimeoutException:
            logger.warning("Could not find the gameworld cards")

        return gm_worlds

    def keep_cookies_alive(self, current_url: str) -> None:
        time.sleep(COOKIE_ALIVE_INTERVAL)
        if self.valid_session and current_url == self.current_world_url:
            self.alive_count = self.alive_count + 1
            logger.info(
                f"Keeping cookies alive {self.alive_count}x ({(self.alive_count * COOKIE_ALIVE_INTERVAL) / 60}min)"
            )
            self.driver.get(current_url)
            time.sleep(GAMEWORLD_LOAD_WAIT * 6)
            self.driver.get(cfg["Gameworld.url"])
            self.executor.submit(partial(self.keep_cookies_alive, current_url))
        else:
            logger.warning(
                f"Session not valid. valid={self.valid},current_url={current_url},self={self.current_url}"
            )

    def convert_cookies(self) -> httpx.Cookies:
        httpx_cookies = httpx.Cookies()

        for cookie in self.cookies:
            httpx_cookies.set(
                name=cookie["name"],
                value=cookie["value"],
                domain=cookie["domain"],
                path=cookie["path"],
            )

        return httpx_cookies

    def make_request(
        self, endpoint: str, payload: dict, tryagain: bool = True
    ) -> httpx.Response:
        try:
            response = self.client.post(endpoint, json=payload)
            if response.status_code != 200:
                raise Exception(
                    f"Something wrong happened, status code {response.status_code}"
                )

            if response.json()["Errorcode"] == 0:
                return response
            else:
                raise Exception("Request done, but server returned error")

        except Exception as e:
            self.valid_session = False
            if tryagain:
                logger.warning(f"Request error: {e}")
                logger.warning("Trying again...")
                self.join_gameworld()
                return self.make_request(endpoint, payload, False)

            logger.warning(f"Tried again, no luck: {e}")
            return None

    def get_corp_info(self) -> httpx.Response:
        corp_id = cfg["Gameworld.corp"]
        payload = dict()
        payload["client"] = "1"
        payload["checksum"] = "-1"
        payload["parameters"] = [corp_id]
        payload["hash"] = hashlib.md5(f'["{corp_id}"]'.encode()).hexdigest()

        request_url = self.current_world_url.split("web")[0] + cfg["Endpoint.corp"]
        return self.make_request(request_url, payload)

    def get_servertime(self) -> httpx.Response:
        payload = {
            "client": 1,
            "checksum": -1,
            "parameters": [],
        }
        payload["hash"] = hashlib.md5("[]".encode()).hexdigest()
        request_url = (
            self.current_world_url.split("web")[0] + cfg["Endpoint.server_time"]
        )

        return self.make_request(request_url, payload)

    def get_licenses(self) -> httpx.Response:
        payload = dict()
        payload["client"] = "1"
        payload["checksum"] = "-1"
        payload["parameters"] = []
        payload["hash"] = hashlib.md5("[]".encode()).hexdigest()

        request_url = self.current_world_url.split("web")[0] + cfg["Endpoint.license"]
        return self.make_request(request_url, payload)
