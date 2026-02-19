# ParkPilot AWS Setup

Final project region/account: `us-east-2`, thing `akge_cc3200_board`, topic `akge_CC3200topic`.

- **IoT endpoint:** `a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com`
- **Topic ARN:** `arn:aws:sns:us-east-2:658947468902:akge_CC3200topic`

## 1) IoT Rule: collision -> SNS

**Name:** `parkpilot_collision_rule`

**SQL:**
```sql
SELECT *
FROM '$aws/things/akge_cc3200_board/shadow/update/accepted'
WHERE state.reported.collision = true
```

**Action:** Publish to SNS topic `akge_CC3200topic`.

This sends your existing email alert whenever firmware reports a collision.

---

## 2) IoT Rule: PARK_REQUEST -> Lambda

**Name:** `parkpilot_park_rule`

**SQL:**
```sql
SELECT *
FROM '$aws/things/akge_cc3200_board/shadow/update/accepted'
WHERE state.desired.var = 'PARK_REQUEST'
```

**Action:** Invoke Lambda `parkpilot_handler`.

---

## 3) Lambda function

File in repo: `lambda/parkpilot_handler.py`

Function behavior:
1. `GetThingShadow`
2. Read latest reported distances (`front/right/left/rear`)
3. Build simple guidance sequence
4. `UpdateThingShadow` with:
   - `state.reported.var = "PARK_GUIDED"`
   - `state.reported.park_guidance`
   - `state.reported.park_guidance_ready = true`
   - `state.desired.var = "PARK_GUIDED"`

### IAM role for Lambda

Attach permissions:
- `iot:GetThingShadow`
- `iot:UpdateThingShadow`
- `logs:CreateLogGroup`
- `logs:CreateLogStream`
- `logs:PutLogEvents`

Scope IoT permissions to thing ARN for `akge_cc3200_board` if possible.

---

## 4) Firmware shadow payloads

### Telemetry (periodic)
Posted by CC3200:
```json
{
  "state": {
    "reported": {
      "mode": 2,
      "speed": 40,
      "front": 55,
      "right": 40,
      "left": 70,
      "rear": 25,
      "collision": false
    }
  }
}
```

### Parking request
Posted by CC3200 when entering cloud-guided park stage:
```json
{
  "state": {
    "desired": {
      "var": "PARK_REQUEST"
    }
  }
}
```

---

## 5) Verification checklist

1. Flash firmware, confirm UART prints `ParkPilot booted`.
2. Confirm shadow gets periodic `reported` updates.
3. Force collision -> verify `collision=true` update -> SNS email arrives.
4. Trigger park mode -> verify desired `PARK_REQUEST` appears.
5. Check CloudWatch logs for `parkpilot_handler` invocation.
6. Confirm shadow gets `reported.park_guidance` from Lambda.
