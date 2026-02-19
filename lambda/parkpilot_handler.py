import json
import os
import time
import boto3

IOT_DATA = boto3.client("iot-data", region_name="us-east-2")
S3 = boto3.client("s3", region_name="us-east-2")
THING_NAME = "akge_cc3200_board"
S3_BUCKET = os.environ.get("PARKPILOT_S3_BUCKET", "")


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
    s3_key = ""
    s3_url = ""

    if S3_BUCKET:
        s3_key = f"guidance/{THING_NAME}/{int(time.time())}.json"
        S3.put_object(
            Bucket=S3_BUCKET,
            Key=s3_key,
            Body=json.dumps(guidance).encode("utf-8"),
            ContentType="application/json",
        )
        s3_url = S3.generate_presigned_url(
            "get_object",
            Params={"Bucket": S3_BUCKET, "Key": s3_key},
            ExpiresIn=300,
        )

    update_doc = {
        "state": {
            "reported": {
                "var": "PARK_GUIDED",
                "park_guidance": guidance,
                "park_guidance_s3_key": s3_key,
                "park_guidance_url": s3_url,
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
