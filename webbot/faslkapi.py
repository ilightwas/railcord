import time
from http import HTTPStatus

from flask import Flask, Response, jsonify

import webbot

app = Flask(__name__)

bot: webbot.Webbot = None
corp_json: str = ""
server_time_json: str = ""
testing: bool = False


@app.route(webbot.webbotconfig["Lucy.personality_endpoint"])
def corporation():
    if testing:
        return corp_json
    try:
        res = bot.get_corp_info()
        if res:
            return Response(res.json(), status=HTTPStatus.OK, mimetype="application/json")
        else:
            return Response(status=HTTPStatus.NO_CONTENT)
    except:
        return Response(status=HTTPStatus.INTERNAL_SERVER_ERROR)


@app.route(webbot.webbotconfig["Lucy.sync_time_endpoint"])
def servertime():
    if testing:
        time.sleep(3)
        current_time = time.time()
        res = {}
        res["Body"] = int(current_time)
        return jsonify(res)

    try:
        res = bot.get_servertime()
        if res:
            return Response(res.json(), status=HTTPStatus.OK, mimetype="application/json")
        else:
            return Response(status=HTTPStatus.NO_CONTENT)
    except:
        return Response(status=HTTPStatus.INTERNAL_SERVER_ERROR)


def load() -> None:
    with open("corp.json", "r") as file:
        global corp_json
        corp_json = file.read()

    with open("servertime.json", "r") as file:
        global server_time_json
        server_time_json = file.read()


if __name__ == "__main__":
    if not testing:
        bot = webbot.Webbot()
        bot.login()
        bot.join_gameworld()
    else:
        load()

    app.run(debug=False, port=webbot.webbotconfig["Lucy.api_port"], use_reloader=False)
