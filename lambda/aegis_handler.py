import json
import os
import time
import random
import boto3

REGION = os.environ.get("AWS_REGION", "us-east-2")
THING_NAME = os.environ.get("AEGIS_THING_NAME", "akge_cc3200_board")
S3_BUCKET = os.environ.get("AEGIS_S3_BUCKET", "")

IOT_DATA = boto3.client("iot-data", region_name=REGION)
S3 = boto3.client("s3", region_name=REGION)


def _as_int(value, default=0):
    try:
        return int(value)
    except Exception:
        return default


def _read_shadow():
    resp = IOT_DATA.get_thing_shadow(thingName=THING_NAME)
    return json.loads(resp["payload"].read())


def _write_shadow(update_doc):
    IOT_DATA.update_thing_shadow(
        thingName=THING_NAME,
        payload=json.dumps(update_doc).encode("utf-8"),
    )


def _make_mission(difficulty, seed):
    rng = random.Random(seed)
    wave_count = max(3, min(8, 2 + difficulty))
    waves = []

    for idx in range(wave_count):
        waves.append(
            {
                "wave": idx + 1,
                "duration_ms": rng.randint(4500, 9000),
                "pressure": rng.randint(1 + difficulty, 4 + (difficulty * 2)),
                "anomaly_bias": rng.choice(["dist", "light", "tilt", "mixed"]),
                "target_sectors": sorted(rng.sample(range(16), k=rng.randint(2, 5))),
            }
        )

    return {
        "game": "AEGIS-172",
        "version": 1,
        "seed": seed,
        "difficulty": difficulty,
        "waves": waves,
    }


def _score_round(reported, event):
    defender_score = _as_int(reported.get("defender_score", reported.get("defender", 0)))
    attacker_score = _as_int(reported.get("attacker_score", reported.get("attacker", 0)))
    latency_ms = _as_int(reported.get("avg_latency_ms", 0))
    drops = _as_int(reported.get("telemetry_drops", 0))

    if isinstance(event, dict):
        defender_score = _as_int(event.get("defender_score", defender_score))
        attacker_score = _as_int(event.get("attacker_score", attacker_score))
        latency_ms = _as_int(event.get("avg_latency_ms", latency_ms))
        drops = _as_int(event.get("telemetry_drops", drops))

    penalty = (latency_ms // 50) + drops
    adjusted_defender = max(0, defender_score - penalty)
    adjusted_attacker = attacker_score

    winner = "DEFENDER" if adjusted_defender >= adjusted_attacker else "ATTACKER"

    return {
        "winner": winner,
        "defender_raw": defender_score,
        "attacker_raw": attacker_score,
        "defender_adjusted": adjusted_defender,
        "attacker_adjusted": adjusted_attacker,
        "latency_ms": latency_ms,
        "telemetry_drops": drops,
        "penalty": penalty,
    }


def _save_json_s3(prefix, body):
    if not S3_BUCKET:
        return "", ""

    key = f"{prefix}/{THING_NAME}/{int(time.time())}.json"
    S3.put_object(
        Bucket=S3_BUCKET,
        Key=key,
        Body=json.dumps(body).encode("utf-8"),
        ContentType="application/json",
    )
    url = S3.generate_presigned_url(
        "get_object",
        Params={"Bucket": S3_BUCKET, "Key": key},
        ExpiresIn=600,
    )
    return key, url


def _load_leaderboard():
    if not S3_BUCKET:
        return {"game": "AEGIS-172", "updated_at": int(time.time()), "runs": []}

    try:
        resp = S3.get_object(Bucket=S3_BUCKET, Key=f"leaderboard/{THING_NAME}/latest.json")
        return json.loads(resp["Body"].read())
    except Exception:
        return {"game": "AEGIS-172", "updated_at": int(time.time()), "runs": []}


def _save_leaderboard(entry):
    board = _load_leaderboard()
    runs = board.get("runs", [])
    runs.insert(0, entry)
    board["runs"] = runs[:10]
    board["updated_at"] = int(time.time())

    if S3_BUCKET:
        S3.put_object(
            Bucket=S3_BUCKET,
            Key=f"leaderboard/{THING_NAME}/latest.json",
            Body=json.dumps(board).encode("utf-8"),
            ContentType="application/json",
        )

    return board


def _extract_cmd(event, shadow_doc):
    if isinstance(event, dict):
        cmd = event.get("cmd")
        if cmd:
            return str(cmd)

        state = event.get("state", {})
        if isinstance(state, dict):
            desired = state.get("desired", {})
            reported = state.get("reported", {})
            if isinstance(desired, dict) and desired.get("cmd"):
                return str(desired.get("cmd"))
            if isinstance(reported, dict) and reported.get("round_done") is True:
                return "ROUND_DONE"
            if isinstance(reported, dict) and reported.get("cmd"):
                return str(reported.get("cmd"))

    desired_shadow = shadow_doc.get("state", {}).get("desired", {})
    if isinstance(desired_shadow, dict) and desired_shadow.get("cmd"):
        return str(desired_shadow.get("cmd"))

    reported_shadow = shadow_doc.get("state", {}).get("reported", {})
    if isinstance(reported_shadow, dict) and reported_shadow.get("round_done") is True:
        return "ROUND_DONE"

    return ""


def lambda_handler(event, context):
    shadow_doc = _read_shadow()
    reported = shadow_doc.get("state", {}).get("reported", {})

    cmd = _extract_cmd(event, shadow_doc).upper()

    if cmd in ("MISSION_REQUEST", "AEGIS_MISSION_REQUEST"):
        difficulty = _as_int(reported.get("difficulty", reported.get("mission_level", 2)), 2)
        seed = _as_int(reported.get("seed", int(time.time())), int(time.time()))

        mission = _make_mission(difficulty, seed)
        s3_key, s3_url = _save_json_s3("missions", mission)

        update_doc = {
            "state": {
                "reported": {
                    "cmd": "MISSION_READY",
                    "mission_ready": True,
                    "mission_level": difficulty,
                    "mission": mission,
                    "mission_s3_key": s3_key,
                    "mission_url": s3_url,
                    "mission_generated_at": int(time.time()),
                },
                "desired": {
                    "cmd": "MISSION_READY"
                },
            }
        }
        _write_shadow(update_doc)

        return {
            "statusCode": 200,
            "body": json.dumps({"ok": True, "phase": "mission", "mission_key": s3_key}),
        }

    if cmd in ("ROUND_DONE", "AEGIS_ROUND_DONE"):
        round_result = _score_round(reported, event if isinstance(event, dict) else {})
        replay_doc = {
            "game": "AEGIS-172",
            "timestamp": int(time.time()),
            "result": round_result,
            "reported_snapshot": reported,
        }
        s3_key, s3_url = _save_json_s3("replays", replay_doc)
        leaderboard = _save_leaderboard(
            {
                "timestamp": replay_doc["timestamp"],
                "winner": round_result["winner"],
                "defender_adjusted": round_result["defender_adjusted"],
                "attacker_adjusted": round_result["attacker_adjusted"],
                "mission_level": _as_int(reported.get("mission_level", reported.get("difficulty", 2)), 2),
                "replay_s3_key": s3_key,
            }
        )

        update_doc = {
            "state": {
                "reported": {
                    "cmd": "ROUND_JUDGED",
                    "round_done": False,
                    "round_result": round_result,
                    "replay_s3_key": s3_key,
                    "replay_url": s3_url,
                    "leaderboard": leaderboard,
                    "last_judged_at": int(time.time()),
                },
                "desired": {
                    "cmd": "ROUND_JUDGED"
                },
            }
        }
        _write_shadow(update_doc)

        return {
            "statusCode": 200,
            "body": json.dumps({"ok": True, "phase": "judge", "winner": round_result["winner"]}),
        }

    return {
        "statusCode": 200,
        "body": json.dumps(
            {
                "ok": True,
                "phase": "noop",
                "message": "No recognized cmd. Expect MISSION_REQUEST or ROUND_DONE.",
                "cmd": cmd,
            }
        ),
    }
