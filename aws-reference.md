# AWS IoT Setup Reference

Reusable reference from Lab 4. Account `658947468902`, region `us-east-1`.

## 1. IoT Thing: `akge_cc3200_board`

- **Shadow endpoint:** `https://a126k3e19n75q0-ats.iot.us-east-1.amazonaws.com`
- **Shadow path:** `/things/akge_cc3200_board/shadow`
- **Port:** 8443 (HTTPS)

The Thing's Classic Shadow is a JSON document with `desired` (cloud sets) and `reported` (device sets) sections. Any shadow change publishes to MQTT topic `$aws/things/akge_cc3200_board/shadow/update/accepted`, which IoT Rules can listen to.

CC3200 interacts via:
- `POST /things/akge_cc3200_board/shadow` with `{"state":{"desired":{"var":"<value>"}}}` to update
- `GET /things/akge_cc3200_board/shadow` to read current state

## 2. Certificates (TLS Auth)

Three files flashed to CC3200 under `/cert/`:
- `client.der` - device certificate
- `private.der` - device private key
- `rootCA.der` - Amazon root CA

Generated in IoT Console -> Security -> Certificates, attached to the Thing. The certificate has a Policy granting `iot:*` on the Thing ARN.

Same certs work for the same Thing. For a new Thing, generate new certs.

DER conversion from AWS PEM files:
```bash
openssl x509 -outform der -in certificate.pem.crt -out client.der
openssl rsa  -outform der -in private.pem.key    -out private.der
```

## 3. IoT Rules

Rules subscribe to MQTT topics and trigger AWS services.

**SNS email rule (Lab 4 Task 2):**
- Topic: `$aws/things/akge_cc3200_board/shadow/update/accepted`
- Action: publish to SNS topic `akge_CC3200topic`
- Fires on every shadow update

**Icon rule `akge_icon_rule` (Lab 4 EC):**
- SQL: `SELECT * FROM '$aws/things/akge_cc3200_board/shadow/update/accepted' WHERE state.desired.var = 'ICON_REQUEST'`
- Action: invoke Lambda `akge_icon_handler`
- `WHERE` clause filters so only icon requests trigger it

Create rules: IoT Console -> Message routing -> Rules -> Create rule -> SQL -> action (SNS, Lambda, DynamoDB, etc.)

## 4. SNS Topic: `akge_CC3200topic`

- ARN: `arn:aws:sns:us-east-1:658947468902:akge_CC3200topic`
- Subscription: `akge.azka@yahoo.com` (email)
- IoT Rule publishes here -> subscriber gets email

## 5. S3 Bucket: `eec172-akge-icons`

- Contains `icons/0_heart.json` through `icons/5_lightning.json`
- Each file: `{"name":"heart","color":"F800","bitmap":"256 hex chars"}`
- No public access; Lambda accesses via IAM role

## 6. Lambda: `akge_icon_handler`

- Runtime: Python 3.12, timeout 10s
- Reads shadow -> gets `icon_idx` -> fetches next icon from S3 -> updates shadow `reported`
- IAM role `akge_icon_lambda_role`:
  - `s3:GetObject` on `arn:aws:s3:::eec172-akge-icons/*`
  - `iot:GetThingShadow` + `iot:UpdateThingShadow` on the Thing ARN
  - `AWSLambdaBasicExecutionRole` (CloudWatch logs)

## CC3200 Firmware Side

**Connection flow (runs once at boot):**
1. `connectToAccessPoint()` - joins WiFi (SSID/password in SDK `common.h`)
2. `set_time()` - sets device clock for TLS cert validation
3. `tls_connect()` - DNS, secure socket, TLS 1.2 handshake -> returns socket ID

**Sending data:**
```c
http_post(g_sockID, "your message");
// sends: {"state":{"desired":{"var":"your message"}}}
```

**Reading shadow:**
```c
http_get(g_sockID, buffer, bufferSize);
// returns full shadow JSON
```

**Reconnect on failure:** call `tls_connect()` again for a new socket ID.

## Extending for Final Project

- **New shadow fields:** POST different JSON keys in `desired`/`reported`
- **New IoT Rules:** different `WHERE` clauses trigger different Lambdas/SNS
- **New Lambdas:** same IAM pattern, grant access to needed services
- **Same certs, endpoint, Thing** work without recreation
