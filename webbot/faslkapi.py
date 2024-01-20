import time
from http import HTTPStatus

from flask import Flask, Response, jsonify

from webbot import Webbot, cfg

app = Flask(__name__)

bot: Webbot = None

corp_json: str = ""
server_time_json: str = ""
license_json: str = ""

testing: bool = True if cfg["Lucy.testing"] == "1" else False


def load_file(filename) -> str:
    with open(filename, "r") as file:
        return file.read()


def load_test_data() -> None:
    global corp_json
    global server_time_json
    global license_json

    corp_json = load_file("webbot/corp.json")
    server_time_json = load_file("webbot/servertime.json")
    license_json = load_file("webbot/license.json")


def test_personality() -> Response:
    return Response(corp_json, status=HTTPStatus.OK, mimetype="application/json")


def test_license() -> Response:
    return Response(license_json, status=HTTPStatus.OK, mimetype="application/json")


def test_servertime() -> Response:
    time.sleep(3)
    current_time = time.time()
    res = {}
    res["Body"] = int(current_time)
    return jsonify(res)


def do_request(request) -> Response:
    try:
        res = request()
        if res:
            return Response(res.text, status=HTTPStatus.OK, mimetype="application/json")
        else:
            return Response(status=HTTPStatus.NO_CONTENT)
    except:
        return Response(status=HTTPStatus.INTERNAL_SERVER_ERROR)


def personality_impl() -> Response:
    return do_request(lambda: bot.get_corp_info())


def license_impl() -> Response:
    return do_request(lambda: bot.get_licenses())


def server_time_impl() -> Response:
    return do_request(lambda: bot.get_servertime())


@app.route(f'/{cfg["Lucy.personality_endpoint"]}')
def personality() -> Response:
    return personality_impl()


@app.route(f'/{cfg["Lucy.license_endpoint"]}')
def license() -> Response:
    return license_impl()


@app.route(f'/{cfg["Lucy.sync_time_endpoint"]}')
def servertime() -> Response:
    return server_time_impl()


if __name__ == "__main__":
    if not testing:
        bot = Webbot()
        bot.login()
        bot.join_gameworld()
    else:
        load_test_data()
        personality_impl = test_personality
        server_time_impl = test_servertime
        license_impl = test_license

    app.run(debug=False, port=cfg["Lucy.api_port"], use_reloader=False)
