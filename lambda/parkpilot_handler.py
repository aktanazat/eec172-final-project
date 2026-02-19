import json
import boto3

IOT_DATA = boto3.client("iot-data", region_name="us-east-2")
THING_NAME = "akge_cc3200_board"


def _as_int(v, default=0):
    try:
        return int(v)
    except Exception:
        return default


def _plan(front, right, left, rear):
    return {
        "source": "lambda",
        "steps": [
            {"cmd": "FWD", "speed": 20, "steer": 30, "duration_ms": 1200},
            {"cmd": "REV", "speed": 22, "steer": -45, "duration_ms": 1700},
            {"cmd": "REV", "speed": 15, "steer": 0, "duration_ms": 1000},
            {"cmd": "STOP", "speed": 0, "steer": 0, "duration_ms": 300},
        ],
        "inputs": {
            "front": front,
            "right": right,
            "left": left,
            "rear": rear,
        },
    }


def lambda_handler(event, context):
    shadow_resp = IOT_DATA.get_thing_shadow(thingName=THING_NAME)
    shadow_doc = json.loads(shadow_resp["payload"].read())

    reported = shadow_doc.get("state", {}).get("reported", {})
    front = _as_int(reported.get("front", 0))
    right = _as_int(reported.get("right", 0))
    left = _as_int(reported.get("left", 0))
    rear = _as_int(reported.get("rear", 0))

    guidance = _plan(front, right, left, rear)

    update_doc = {
        "state": {
            "reported": {
                "var": "PARK_GUIDED",
                "park_guidance": guidance,
                "park_guidance_ready": True,
            },
            "desired": {
                "var": "PARK_GUIDED"
            }
        }
    }

    IOT_DATA.update_thing_shadow(
        thingName=THING_NAME,
        payload=json.dumps(update_doc).encode("utf-8"),
    )

    return {
        "statusCode": 200,
        "body": json.dumps({"ok": True, "guidance": guidance}),
    }
