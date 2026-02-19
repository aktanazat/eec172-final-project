# AWS IoT Setup Reference

Final project cloud resources in account `658947468902`, region `us-east-2`.

## 1. IoT Thing: `akge_cc3200_board`

- **Shadow endpoint:** `https://a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com`
- **Shadow path:** `/things/akge_cc3200_board/shadow`
- **Port:** 8443 (HTTPS)

The Thing's Classic Shadow JSON uses `desired` (cloud directives) and `reported` (device/lambda updates). Shadow updates publish to `$aws/things/akge_cc3200_board/shadow/update/accepted`.

## 2. IoT Rules

### Rule: `parkpilot_park_rule`

- **SQL:**
  `SELECT * FROM '$aws/things/akge_cc3200_board/shadow/update/accepted' WHERE state.desired.var = 'PARK_REQUEST'`
- **Action:** invoke Lambda `parkpilot_handler` (`$LATEST`)

### Rule: `parkpilot_collision_rule`

- **SQL:**
  `SELECT * FROM '$aws/things/akge_cc3200_board/shadow/update/accepted' WHERE state.reported.collision = true`
- **Action:** publish to SNS topic `akge_CC3200topic`

## 3. Lambda: `parkpilot_handler`

- Runtime: Python 3.12
- Region client: `us-east-2`
- Thing: `akge_cc3200_board`

Function flow:
1. `GetThingShadow`
2. Read `reported` distances (`front`, `right`, `left`, `rear`)
3. Build parking guidance sequence
4. `UpdateThingShadow` with:
   - `state.reported.var = "PARK_GUIDED"`
   - `state.reported.park_guidance`
   - `state.reported.park_guidance_ready = true`
   - `state.desired.var = "PARK_GUIDED"`

## 4. SNS Topic: `akge_CC3200topic`

- ARN: `arn:aws:sns:us-east-2:658947468902:akge_CC3200topic`
- Email subscription must be `Confirmed`
- Receives collision messages from `parkpilot_collision_rule`

## 5. IAM requirements

Lambda execution role needs:
- `iot:GetThingShadow`
- `iot:UpdateThingShadow`
- `logs:CreateLogGroup`
- `logs:CreateLogStream`
- `logs:PutLogEvents`

Scope IoT actions to:
`arn:aws:iot:us-east-2:658947468902:thing/akge_cc3200_board`

## 6. Firmware alignment (`main.c`)

Firmware constants must match `us-east-2`:
- `SERVER_NAME`: `a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com`
- `HOSTHEADER`: `Host: a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com`

If endpoint/certs/policy do not match this Thing/region, board-to-cloud requests fail.

## 7. Validation checklist

1. Thing and Classic Shadow exist in `us-east-2`.
2. Save shadow with `desired.var = "PARK_REQUEST"` and sensor values.
3. Confirm Lambda invocation in CloudWatch logs.
4. Confirm `reported.park_guidance` and `reported.park_guidance_ready` appear.
5. Save shadow with `reported.collision = true`.
6. Confirm SNS email delivery.
