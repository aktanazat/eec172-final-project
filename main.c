//*****************************************************************************
// AEGIS-172 Final Project Firmware (CC3200)
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "simplelink.h"

#include "hw_types.h"
#include "hw_ints.h"
#include "hw_nvic.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "gpio.h"
#include "spi.h"
#include "systick.h"

#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"

#include "Adafruit_SSD1351.h"
#include "Adafruit_GFX.h"
#include "i2c_if.h"
#include "utils/network_utils.h"

#define SERVER_NAME           "a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com"
#define SERVER_PORT           8443

#define POSTHEADER            "POST /things/akge_cc3200_board/shadow HTTP/1.1\r\n"
#define SHADOWTOPICPOSTHEADER "POST /topics/$aws/things/akge_cc3200_board/shadow/update?qos=0 HTTP/1.1\r\n"
#define GETHEADER             "GET /things/akge_cc3200_board/shadow HTTP/1.1\r\n"
#define HOSTHEADER            "Host: a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com\r\n"
#define CHEADER               "Connection: close\r\n"
#define CTHEADER              "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1             "Content-Length: "
#define CLHEADER2             "\r\n\r\n"

#define DATE                  19
#define MONTH                 2
#define YEAR                  2026
#define HOUR                  22
#define MINUTE                30
#define SECOND                0

#define IR_GPIO_PORT          GPIOA1_BASE
#define IR_GPIO_PIN           GPIO_PIN_4
#define SYSCLKFREQ            80000000ULL
#define SYSTICK_RELOAD_VAL    8000000UL
#define TICKS_TO_US(ticks) ((((ticks) / SYSCLKFREQ) * 1000000ULL) + ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))

#define UART1_BAUD            9600

#define SHADOW_BUF_SIZE       4096
#define S3_URL_BUF_SIZE       2048

#define SECTOR_COUNT          16
#define LOOP_DELAY_TICKS      120000UL

#define SENSOR_TIMEOUT_LOOPS  96
#define CONTROL_INTERVAL_LOOPS 6
#define CONTROL_RESEND_LOOPS  18
#define DRAW_INTERVAL_LOOPS   5
#define LOG_INTERVAL_LOOPS    320
#define SHADOW_INTERVAL_LOOPS 160
#define MISSION_POLL_LOOPS    180
#define MISSION_REQUEST_RETRY_LOOPS 120
#define SCORE_INTERVAL_LOOPS  12
#define CONTROL_KEEPALIVE_LOOPS 80
#define IR_DEBOUNCE_LOOPS     16
#define CLOUD_RETRY_COOLDOWN_LOOPS 3000
#define SYNC_RETRY_LOOPS      800

#define CALIBRATE_LOOPS       220
#define MISSION_WAIT_LOOPS    400
#define PREP_LOOPS            120
#define ACTIVE_ROUND_LOOPS    2200
#define JUDGE_LOOPS           120
#define SYNC_LOOPS            6000

#define ATTACK_PULSE_LOOPS    140
#define ATTACK_JAM_LOOPS      170
#define ATTACK_BLIND_LOOPS    170
#define BLOCK_WINDOW_SECTORS  2

#define DIST_NEAR             12
#define DIST_MED              24
#define DIST_FAR              40

#define BTN_START             1
#define BTN_DIFF_UP           2
#define BTN_MISSION           3
#define BTN_ATTACK_PULSE      4
#define BTN_RESET             5
#define BTN_ATTACK_JAM        6
#define BTN_ATTACK_BLIND      8
#define BTN_ABORT             10
#define BTN_DIFF_DOWN         12
#define BTN_SYNC              13

#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

typedef struct SensorFrame {
    unsigned long ms;
    int sector;
    int distCm;
    int lux;
    int tilt;
    int tempC10;
    int hum10;
    int aux;
    int joy;
} SensorFrame;

typedef enum RoundState {
    RS_BOOT = 0,
    RS_CALIBRATE = 1,
    RS_MISSION = 2,
    RS_PREP = 3,
    RS_ACTIVE = 4,
    RS_JUDGE = 5,
    RS_SYNC = 6,
    RS_END = 7
} RoundState;

volatile unsigned long g_irCmd = 0;
volatile int g_bitCount = 0;
volatile int g_codeReady = 0;
unsigned long g_irEdgeCount = 0;
unsigned long g_irCodeCount = 0;

volatile int g_sensorReady = 0;
unsigned long g_uart1RxBytes = 0;
unsigned long g_uart1RxLines = 0;
unsigned long g_uart1RxOverflow = 0;
unsigned long g_uart1TxLines = 0;
unsigned long g_lastIrAcceptLoop = 0;

char g_softLine[128];
char g_readyLine[128];
volatile int g_softIdx = 0;
unsigned long g_softParseOk = 0;
unsigned long g_softParseFail = 0;

SensorFrame g_sensor = {0, 0, 400, 0, 0, 250, 500, 0, 512};

RoundState g_state = RS_BOOT;
unsigned long g_loopCount = 0;
unsigned long g_stateStartLoop = 0;
unsigned long g_lastSensorLoop = 0;
unsigned long g_lastControlLoop = 0;
unsigned long g_lastDrawLoop = 0;
unsigned long g_lastLogLoop = 0;
unsigned long g_lastShadowLoop = 0;
unsigned long g_lastMissionPollLoop = 0;
unsigned long g_lastMissionRequestLoop = 0;
int g_waitingForSensor = 0;

int g_servoDeg = 90;
int g_stepMode = 0;
int g_buzzMode = 0;
int g_rgbCode = 4;
int g_shieldSector = 0;

int g_defenderScore = 0;
int g_attackerScore = 0;
int g_missionDifficulty = 1;
int g_missionReady = 0;
int g_roundReported = 0;
int g_shadowDirty = 1;
int g_cloudOnline = 0;
int g_forceLocalMission = 0;
unsigned long g_nextCloudRetryLoop = 0;
int g_lastCloudError = 0;
char g_lastCloudOp[8] = "BOOT";

int g_baseDist = 200;
int g_baseLux = 500;
int g_baseTemp10 = 250;
int g_baseHum10 = 500;
long g_calDistSum = 0;
long g_calLuxSum = 0;
long g_calTempSum = 0;
long g_calHumSum = 0;
int g_calCount = 0;

int g_cachedThreat = 0;
int g_cachedThreatSector = 0;
int g_cachedBlocked = 0;

unsigned long g_attackPulseUntil = 0;
unsigned long g_attackJamUntil = 0;
unsigned long g_attackBlindUntil = 0;
unsigned long g_lastScoreLoop = 0;
int g_attackAction = 0;

int g_sockID = -1;
char g_httpBuf[SHADOW_BUF_SIZE];
char g_s3MissionUrl[S3_URL_BUF_SIZE];

static const int g_sectorDx[SECTOR_COUNT] = {0, 12, 23, 30, 32, 30, 23, 12, 0, -12, -23, -30, -32, -30, -23, -12};
static const int g_sectorDy[SECTOR_COUNT] = {-32, -30, -23, -12, 0, 12, 23, 30, 32, 30, 23, 12, 0, -12, -23, -30};

static inline void SysTickReset(void)
{
    HWREG(NVIC_ST_CURRENT) = 1;
}

static int clampInt(int value, int lo, int hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int absInt(int value)
{
    return (value < 0) ? -value : value;
}

static unsigned long loopsSince(unsigned long start)
{
    return g_loopCount - start;
}

static const char *stateLabel(RoundState state)
{
    switch (state) {
        case RS_BOOT: return "BOOT";
        case RS_CALIBRATE: return "CAL ";
        case RS_MISSION: return "NET ";
        case RS_PREP: return "PREP";
        case RS_ACTIVE: return "ACT ";
        case RS_JUDGE: return "JDG ";
        case RS_SYNC: return "SYNC";
        default: return "END ";
    }
}

static const char *cloudLabel(void)
{
    if (!g_cloudOnline) return "LOCAL";
    if (g_state == RS_MISSION && !g_missionReady) return "FETCH";
    if (g_state == RS_SYNC) return "SYNC";
    return "CLOUD";
}

static const char *attackLabel(void)
{
    if (g_attackPulseUntil > g_loopCount) return "PULSE";
    if (g_attackJamUntil > g_loopCount) return "JAM";
    if (g_attackBlindUntil > g_loopCount) return "BLIND";
    return "READY";
}

static int attackMode(void)
{
    if (g_attackPulseUntil > g_loopCount) return 1;
    if (g_attackJamUntil > g_loopCount) return 2;
    if (g_attackBlindUntil > g_loopCount) return 3;
    return 0;
}

static const char *winnerLabel(void)
{
    return (g_defenderScore >= g_attackerScore) ? "DEF" : "ATK";
}

static void BoardInit(void)
{
#ifndef USE_TIRTOS
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    PRCMCC3200MCUInit();
}

static void SPIInit(void)
{
    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                           20000000, SPI_MODE_MASTER, SPI_SUB_MODE_0,
                           (SPI_SW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_OFF |
                            SPI_CS_ACTIVELOW | SPI_WL_8));
    MAP_SPIEnable(GSPI_BASE);
}

static void SysTickHandler(void)
{
    g_bitCount = 0;
}

static void IRIntHandler(void)
{
    unsigned long status = MAP_GPIOIntStatus(IR_GPIO_PORT, true);
    unsigned long us;

    MAP_GPIOIntClear(IR_GPIO_PORT, status);
    if (!(status & IR_GPIO_PIN) || g_codeReady) return;
    g_irEdgeCount++;

    us = TICKS_TO_US(SYSTICK_RELOAD_VAL - MAP_SysTickValueGet());
    SysTickReset();

    if (us > 3000) {
        g_irCmd = 0;
        g_bitCount = 0;
        return;
    }

    if (g_bitCount >= 32 && g_bitCount < 48) {
        g_irCmd <<= 1;
        if (us > 1200) g_irCmd |= 1;
    }

    g_bitCount++;
    if (g_bitCount == 48) {
        g_codeReady = 1;
        g_irCodeCount++;
    }
}

static int IRCodeToButton(unsigned long cmd)
{
    switch (cmd) {
        case 0x9899: return 0;
        case 0x0809: return BTN_START;
        case 0x8889: return BTN_DIFF_UP;
        case 0x4849: return BTN_MISSION;
        case 0xC8C9: return BTN_ATTACK_PULSE;
        case 0x2829: return BTN_RESET;
        case 0xA8A9: return BTN_ATTACK_JAM;
        case 0x6869: return 7;
        case 0xE8E9: return BTN_ATTACK_BLIND;
        case 0x4C4D: return BTN_ABORT;
        case 0xECED: return 11;
        case 0x2C2D: return BTN_DIFF_DOWN;
        case 0xACAD: return BTN_SYNC;
        default: return -1;
    }
}

static void IRInit(void)
{
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);
    MAP_SysTickIntRegister(SysTickHandler);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();

    MAP_GPIOIntRegister(IR_GPIO_PORT, IRIntHandler);
    MAP_GPIOIntTypeSet(IR_GPIO_PORT, IR_GPIO_PIN, GPIO_FALLING_EDGE);
    MAP_GPIOIntClear(IR_GPIO_PORT, MAP_GPIOIntStatus(IR_GPIO_PORT, false));
    MAP_IntEnable(INT_GPIOA1);
    MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);
}

static void Uart1Init(void)
{
    MAP_UARTConfigSetExpClk(UARTA1_BASE,
                            MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            UART1_BAUD,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));
    MAP_UARTEnable(UARTA1_BASE);
}

static void Uart1PollRx(void)
{
    while (MAP_UARTCharsAvail(UARTA1_BASE)) {
        unsigned char c = (unsigned char)MAP_UARTCharGet(UARTA1_BASE);
        g_uart1RxBytes++;

        if (c == '$') {
            g_softIdx = 0;
        }

        if (c == '\n' || c == '\r') {
            if (g_softIdx > 0) {
                g_softLine[g_softIdx] = '\0';
                strncpy(g_readyLine, g_softLine, sizeof(g_readyLine) - 1);
                g_readyLine[sizeof(g_readyLine) - 1] = '\0';
                g_uart1RxLines++;
                g_sensorReady = 1;
            }
            g_softIdx = 0;
            continue;
        }

        if (g_softIdx >= (int)sizeof(g_softLine) - 1) {
            g_uart1RxOverflow++;
            g_softIdx = 0;
            continue;
        }

        g_softLine[g_softIdx++] = (char)c;
    }
}

static void Uart1TxString(const char *s)
{
    const char *p = s;
    while (*p) {
        if (*p == '\n') g_uart1TxLines++;
        p++;
    }
    while (*s) {
        MAP_UARTCharPut(UARTA1_BASE, (unsigned char)*s++);
    }
}

static int set_time(void)
{
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_hour = HOUR;
    g_time.tm_min = MINUTE;
    g_time.tm_sec = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                       SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                       sizeof(SlDateTime), (unsigned char *)(&g_time));
    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

static int ensureTlsSocket(void)
{
    if (g_sockID >= 0) return 0;
    if (g_loopCount < g_nextCloudRetryLoop) return -1;

    g_sockID = tls_connect();
    if (g_sockID < 0) {
        g_cloudOnline = 0;
        g_lastCloudError = g_sockID;
        snprintf(g_lastCloudOp, sizeof(g_lastCloudOp), "TLS");
        g_nextCloudRetryLoop = g_loopCount + CLOUD_RETRY_COOLDOWN_LOOPS;
        return -1;
    }

    g_cloudOnline = 1;
    return 0;
}

static int http_post_path_json(int sock, const char *pathHeader, const char *json)
{
    char sendBuf[1400];
    char recvBuf[512];
    int reqLen;
    int ret;

    reqLen = snprintf(sendBuf, sizeof(sendBuf),
                      "%s%s%s%s%s%d%s%s",
                      pathHeader,
                      HOSTHEADER,
                      CHEADER,
                      CTHEADER,
                      CLHEADER1,
                      (int)strlen(json),
                      CLHEADER2,
                      json);
    if (reqLen <= 0 || reqLen >= (int)sizeof(sendBuf)) return -3;

    ret = sl_Send(sock, sendBuf, reqLen, 0);
    if (ret < 0) return ret;

    ret = sl_Recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
    if (ret == SL_EAGAIN) return 0;
    if (ret < 0) return ret;
    recvBuf[ret] = '\0';

    if (strstr(recvBuf, "HTTP/1.1 4") || strstr(recvBuf, "HTTP/1.1 5")) {
        return -2;
    }

    return 0;
}

static int http_post_path_json_fire_and_forget(int sock, const char *pathHeader, const char *json)
{
    char sendBuf[1400];
    int reqLen;
    int ret;

    reqLen = snprintf(sendBuf, sizeof(sendBuf),
                      "%s%s%s%s%s%d%s%s",
                      pathHeader,
                      HOSTHEADER,
                      CHEADER,
                      CTHEADER,
                      CLHEADER1,
                      (int)strlen(json),
                      CLHEADER2,
                      json);
    if (reqLen <= 0 || reqLen >= (int)sizeof(sendBuf)) return -3;

    ret = sl_Send(sock, sendBuf, reqLen, 0);
    if (ret < 0) return ret;
    if (ret != reqLen) return -4;
    return 0;
}

static int http_post_json(int sock, const char *json)
{
    return http_post_path_json(sock, POSTHEADER, json);
}

static int http_get_shadow(int sock, char *resp, int respSize)
{
    char sendBuf[256];
    char *p = sendBuf;
    int ret;

    strcpy(p, GETHEADER); p += strlen(GETHEADER);
    strcpy(p, HOSTHEADER); p += strlen(HOSTHEADER);
    strcpy(p, CHEADER); p += strlen(CHEADER);
    strcpy(p, "\r\n"); p += 2;

    ret = sl_Send(sock, sendBuf, strlen(sendBuf), 0);
    if (ret < 0) return ret;

    ret = sl_Recv(sock, resp, respSize - 1, 0);
    if (ret == SL_EAGAIN) return -1;
    if (ret < 0) return ret;
    resp[ret] = '\0';
    return ret;
}

static int extract_json_int_value(const char *json, const char *key, int *out)
{
    char pattern[64];
    const char *p;

    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (sscanf(p, "%d", out) == 1) return 0;
    return -1;
}

static int extract_json_string_value(const char *json, const char *key, char *out, int outSize)
{
    char pattern[96];
    const char *p;
    int i = 0;

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);

    while (*p && *p != '"' && i < outSize - 1) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (i > 0) ? 0 : -1;
}

static int requestCloudMission(void)
{
    char payload[192];
    int ret;

    snprintf(g_lastCloudOp, sizeof(g_lastCloudOp), "REQ");
    if (ensureTlsSocket() < 0) return -1;

    snprintf(payload, sizeof(payload),
             "{\"state\":{\"desired\":{\"cmd\":\"MISSION_REQUEST\",\"project\":\"AEGIS-172\",\"mission_level\":%d}}}",
             g_missionDifficulty);

    ret = http_post_json(g_sockID, payload);
    if (ret < 0) {
        sl_Close(g_sockID);
        g_sockID = -1;
        g_cloudOnline = 0;
        g_lastCloudError = ret;
        g_nextCloudRetryLoop = g_loopCount + CLOUD_RETRY_COOLDOWN_LOOPS;
        return -1;
    } else {
        sl_Close(g_sockID);
        g_sockID = -1;
        g_lastCloudError = 0;
    }
    return 0;
}

static int pollCloudMission(void)
{
    int missionLevel = 0;

    snprintf(g_lastCloudOp, sizeof(g_lastCloudOp), "POLL");
    if (ensureTlsSocket() < 0) return -1;
    if (http_get_shadow(g_sockID, g_httpBuf, sizeof(g_httpBuf)) < 0) {
        g_lastCloudError = -2;
        sl_Close(g_sockID);
        g_sockID = -1;
        g_cloudOnline = 0;
        g_nextCloudRetryLoop = g_loopCount + CLOUD_RETRY_COOLDOWN_LOOPS;
        return -1;
    }

    if (extract_json_int_value(g_httpBuf, "mission_level", &missionLevel) == 0) {
        g_missionDifficulty = clampInt(missionLevel, 1, 5);
        g_missionReady = 1;
    }

    if (extract_json_string_value(g_httpBuf, "mission_url", g_s3MissionUrl, sizeof(g_s3MissionUrl)) == 0) {
        g_missionReady = 1;
    }

    if (strstr(g_httpBuf, "\"mission_ready\":true") || strstr(g_httpBuf, "MISSION_READY")) {
        g_missionReady = 1;
    }

    sl_Close(g_sockID);
    g_sockID = -1;
    g_lastCloudError = 0;
    return 0;
}

static int awsShadowUpdate(int roundDone)
{
    char payload[512];
    int payloadLen;
    int ret;

    snprintf(g_lastCloudOp, sizeof(g_lastCloudOp), "SYNC");
    if (ensureTlsSocket() < 0) return -1;

    payloadLen = snprintf(payload, sizeof(payload),
                          "{\"state\":{\"reported\":{\"project\":\"AEGIS-172\",\"cmd\":\"%s\",\"phase\":\"%s\","
                          "\"mission_level\":%d,\"defender_score\":%d,\"attacker_score\":%d,\"threat\":%d,"
                          "\"sector\":%d,\"shield\":%d,\"distance\":%d,\"winner\":\"%s\",\"round_done\":%s,"
                          "\"telemetry_drops\":%lu}}}",
                          roundDone ? "ROUND_DONE" : "ROUND_UPDATE",
                          stateLabel(g_state),
                          g_missionDifficulty,
                          g_defenderScore,
                          g_attackerScore,
                          g_cachedThreat,
                          g_cachedThreatSector,
                          g_shieldSector,
                          g_sensor.distCm,
                          winnerLabel(),
                          roundDone ? "true" : "false",
                          g_softParseFail);
    if (payloadLen <= 0 || payloadLen >= (int)sizeof(payload)) {
        g_lastCloudError = -3;
        return -1;
    }

    ret = http_post_json(g_sockID, payload);
    if (ret < 0) {
        sl_Close(g_sockID);
        g_sockID = -1;
        g_cloudOnline = 0;
        g_lastCloudError = ret;
        g_nextCloudRetryLoop = g_loopCount + CLOUD_RETRY_COOLDOWN_LOOPS;
        return -1;
    } else {
        sl_Close(g_sockID);
        g_sockID = -1;
        g_cloudOnline = 1;
        g_shadowDirty = 0;
        g_lastCloudError = 0;
    }
    return 0;
}

static int publishRoundEvent(void)
{
    int ret = awsShadowUpdate(1);
    if (ret == 0) {
        g_roundReported = 1;
        return 0;
    }

    UART_PRINT("SYNC publish failed: ret=%d err=%d\r\n", ret, g_lastCloudError);
    return ret;
}

static void updateShieldFromJoystick(void)
{
    static int lastSector = 7;
    int joy = g_sensor.joy;
    int targetSector;

    if (absInt(joy - 512) < 96) {
        joy = 512;
    }

    targetSector = clampInt((joy * (SECTOR_COUNT - 1)) / 1023, 0, SECTOR_COUNT - 1);
    if (absInt(targetSector - lastSector) >= 2) {
        lastSector = targetSector;
    }

    g_shieldSector = lastSector;
    g_servoDeg = 20 + ((140 * g_shieldSector) / (SECTOR_COUNT - 1));
}

static void setState(RoundState next)
{
    g_state = next;
    g_stateStartLoop = g_loopCount;
    g_shadowDirty = 1;

    if (g_state == RS_BOOT) {
        updateShieldFromJoystick();
        g_stepMode = 0;
        g_buzzMode = 0;
        g_rgbCode = 6;
        g_attackPulseUntil = 0;
        g_attackJamUntil = 0;
        g_attackBlindUntil = 0;
    } else if (g_state == RS_CALIBRATE) {
        g_calDistSum = 0;
        g_calLuxSum = 0;
        g_calTempSum = 0;
        g_calHumSum = 0;
        g_calCount = 0;
        updateShieldFromJoystick();
        g_stepMode = 2;
        g_buzzMode = 0;
        g_rgbCode = 6;
        g_attackPulseUntil = 0;
        g_attackJamUntil = 0;
        g_attackBlindUntil = 0;
    } else if (g_state == RS_MISSION) {
        updateShieldFromJoystick();
        g_stepMode = 2;
        g_buzzMode = 0;
        g_rgbCode = 5;
        g_missionReady = 0;
        g_lastMissionRequestLoop = g_loopCount - MISSION_REQUEST_RETRY_LOOPS;
        g_lastMissionPollLoop = g_loopCount;
    } else if (g_state == RS_PREP) {
        updateShieldFromJoystick();
        g_stepMode = 2;
        g_buzzMode = 0;
        g_rgbCode = 5;
    } else if (g_state == RS_ACTIVE) {
        g_defenderScore = 0;
        g_attackerScore = 0;
        g_lastScoreLoop = g_loopCount;
        g_stepMode = 2;
        g_roundReported = 0;
    } else if (g_state == RS_JUDGE) {
        g_stepMode = 0;
        g_buzzMode = 0;
        g_rgbCode = (g_defenderScore >= g_attackerScore) ? 1 : 3;
    } else if (g_state == RS_SYNC) {
        g_stepMode = 0;
        g_buzzMode = 0;
        g_rgbCode = 6;
        g_roundReported = 0;
        g_lastShadowLoop = g_loopCount - SYNC_RETRY_LOOPS;
    } else {
        g_stepMode = 0;
        g_buzzMode = 0;
        g_rgbCode = (g_defenderScore >= g_attackerScore) ? 1 : 3;
    }

    UART_PRINT("STATE -> %s\n\r", stateLabel(g_state));
}

static void sendControlFrame(void)
{
    static int lastServo = -999;
    static int lastStep = -999;
    static int lastBuzz = -999;
    static int lastRgb = -999;
    static int lastState = -999;
    char out[96];
    int changed;

    changed = (g_servoDeg != lastServo) ||
              (g_stepMode != lastStep) ||
              (g_buzzMode != lastBuzz) ||
              (g_rgbCode != lastRgb) ||
              ((int)g_state != lastState);

    if (!changed && loopsSince(g_lastControlLoop) < CONTROL_KEEPALIVE_LOOPS) return;
    if (changed && loopsSince(g_lastControlLoop) < CONTROL_INTERVAL_LOOPS) return;

    g_lastControlLoop = g_loopCount;
    snprintf(out, sizeof(out), "$C,%d,%d,%d,%d,%d\n",
             g_servoDeg, g_stepMode, g_buzzMode, g_rgbCode, (int)g_state);
    Uart1TxString(out);
    lastServo = g_servoDeg;
    lastStep = g_stepMode;
    lastBuzz = g_buzzMode;
    lastRgb = g_rgbCode;
    lastState = (int)g_state;
}

static int parseSensorFrame(const char *line)
{
    unsigned long ms = 0;
    int sector = 0;
    int distCm = 0;
    int lux = 0;
    int tilt = 0;
    int tempC10 = 0;
    int hum10 = 0;
    int aux = 0;
    int joy = 0;

    if (sscanf(line, "$S,%lu,%d,%d,%d,%d,%d,%d,%d,%d",
               &ms, &sector, &distCm, &lux, &tilt, &tempC10, &hum10, &aux, &joy) == 9) {
        g_sensor.ms = ms;
        g_sensor.sector = clampInt(sector, 0, SECTOR_COUNT - 1);
        g_sensor.distCm = clampInt(distCm, 2, 400);
        g_sensor.lux = clampInt(lux, 0, 1023);
        g_sensor.tilt = 0;
        g_sensor.tempC10 = clampInt(tempC10, -200, 800);
        g_sensor.hum10 = clampInt(hum10, 0, 1000);
        g_sensor.aux = aux;
        g_sensor.joy = clampInt(joy, 0, 1023);
        g_lastSensorLoop = g_loopCount;
        g_waitingForSensor = 0;
        g_softParseOk++;
        return 0;
    }

    g_softParseFail++;
    return -1;
}

static void handleIrButton(int button)
{
    if (button == BTN_ABORT || button == BTN_RESET) {
        setState(RS_BOOT);
        return;
    }

    if (button == BTN_DIFF_UP) {
        g_missionDifficulty = clampInt(g_missionDifficulty + 1, 1, 5);
        g_shadowDirty = 1;
        return;
    }

    if (button == BTN_DIFF_DOWN) {
        g_missionDifficulty = clampInt(g_missionDifficulty - 1, 1, 5);
        g_shadowDirty = 1;
        return;
    }

    if ((button == BTN_START || button == BTN_MISSION) && (g_state == RS_BOOT || g_state == RS_END)) {
        g_forceLocalMission = (button == BTN_START) ? 0 : 0;
        setState(RS_CALIBRATE);
        return;
    }

    if (button == BTN_MISSION && g_state == RS_MISSION) {
        requestCloudMission();
        return;
    }

    if (button == BTN_SYNC && g_state == RS_END) {
        setState(RS_SYNC);
        return;
    }

    if (g_state != RS_ACTIVE) return;

    if (button == BTN_ATTACK_PULSE) {
        g_attackPulseUntil = g_loopCount + ATTACK_PULSE_LOOPS;
        g_attackAction = BTN_ATTACK_PULSE;
    } else if (button == BTN_ATTACK_JAM) {
        g_attackJamUntil = g_loopCount + ATTACK_JAM_LOOPS;
        g_attackAction = BTN_ATTACK_JAM;
    } else if (button == BTN_ATTACK_BLIND) {
        g_attackBlindUntil = g_loopCount + ATTACK_BLIND_LOOPS;
        g_attackAction = BTN_ATTACK_BLIND;
    }
}

static int resolveThreatSector(void)
{
    int sector = g_sensor.sector;
    if (g_attackJamUntil > g_loopCount) {
        sector = (sector + 3) % SECTOR_COUNT;
    }
    return sector;
}

static int computeThreatLevel(void)
{
    int threat = 0;
    int dist = g_sensor.distCm;
    int luxDelta = absInt(g_sensor.lux - g_baseLux);
    int tempDelta = absInt(g_sensor.tempC10 - g_baseTemp10);

    if (g_attackBlindUntil > g_loopCount) luxDelta += 220;

    if (dist < DIST_FAR) threat += 1;
    if (dist < DIST_MED) threat += 1;
    if (dist < DIST_NEAR) threat += 1;

    if (luxDelta > 120) threat += 1;
    if (luxDelta > 260) threat += 1;

    if (tempDelta > 20) threat += 1;

    if (g_attackPulseUntil > g_loopCount) threat += 2;
    if (g_attackJamUntil > g_loopCount) threat += 1;
    if (g_attackBlindUntil > g_loopCount) threat += 1;

    threat += clampInt(g_missionDifficulty - 1, 0, 2);
    return clampInt(threat, 0, 15);
}

static void updateGameplayOutputs(int threat)
{
    if (loopsSince(g_lastSensorLoop) > SENSOR_TIMEOUT_LOOPS) {
        g_buzzMode = 2;
        g_rgbCode = 3;
        return;
    }

    if (threat >= 7) {
        g_buzzMode = 1;
        g_rgbCode = 3;
    } else if (threat >= 3) {
        g_buzzMode = 2;
        g_rgbCode = 2;
    } else {
        g_buzzMode = 0;
        g_rgbCode = 1;
    }
}

static void updateStateMachine(void)
{
    unsigned long elapsed = loopsSince(g_stateStartLoop);
    int threat;
    int threatSector;
    int delta;
    int blocked;

    updateShieldFromJoystick();

    if (g_state == RS_BOOT) {
        return;
    }

    if (g_state == RS_CALIBRATE) {
        g_calDistSum += g_sensor.distCm;
        g_calLuxSum += g_sensor.lux;
        g_calTempSum += g_sensor.tempC10;
        g_calHumSum += g_sensor.hum10;
        g_calCount++;

        if (elapsed >= CALIBRATE_LOOPS) {
            if (g_calCount > 0) {
                g_baseDist = (int)(g_calDistSum / g_calCount);
                g_baseLux = (int)(g_calLuxSum / g_calCount);
                g_baseTemp10 = (int)(g_calTempSum / g_calCount);
                g_baseHum10 = (int)(g_calHumSum / g_calCount);
            }
            setState(RS_MISSION);
        }
        return;
    }

    if (g_state == RS_MISSION) {
        if (elapsed >= MISSION_WAIT_LOOPS) {
            setState(RS_PREP);
        }
        return;
    }

    if (g_state == RS_PREP) {
        if (elapsed >= PREP_LOOPS) {
            setState(RS_ACTIVE);
        }
        return;
    }

    if (g_state == RS_ACTIVE) {
        threatSector = resolveThreatSector();
        threat = computeThreatLevel();
        delta = absInt(g_shieldSector - threatSector);
        if (delta > (SECTOR_COUNT / 2)) delta = SECTOR_COUNT - delta;
        blocked = (delta <= BLOCK_WINDOW_SECTORS);

        g_cachedThreat = threat;
        g_cachedThreatSector = threatSector;
        g_cachedBlocked = blocked;

        if (loopsSince(g_lastScoreLoop) >= SCORE_INTERVAL_LOOPS) {
            g_lastScoreLoop = g_loopCount;

            if (threat >= 6) {
                if (blocked) g_defenderScore += 3 + threat;
                else g_attackerScore += 1 + threat;
            } else if (threat >= 3) {
                if (blocked) g_defenderScore += 2 + threat;
                else g_attackerScore += 1 + threat;
            } else {
                g_defenderScore += 1;
            }

            g_shadowDirty = 1;
        }

        updateGameplayOutputs(threat);

        if (elapsed >= ACTIVE_ROUND_LOOPS) {
            setState(RS_JUDGE);
        }
        return;
    }

    if (g_state == RS_JUDGE) {
        if (elapsed >= JUDGE_LOOPS) {
            setState(RS_SYNC);
        }
        return;
    }

    if (g_state == RS_SYNC) {
        if (!g_roundReported && loopsSince(g_lastShadowLoop) >= SYNC_RETRY_LOOPS) {
            g_lastShadowLoop = g_loopCount;
            publishRoundEvent();
        }

        if (elapsed >= SYNC_LOOPS) {
            setState(RS_END);
        }
        return;
    }
}

static void drawText(int x, int y, unsigned int fg, unsigned int bg, const char *text)
{
    setTextColor(fg, bg);
    setCursor(x, y);
    Outstr((char *)text);
}

static void drawSectorMarkers(int cx, int cy)
{
    int i;
    int x0;
    int y0;
    int x1;
    int y1;

    for (i = 0; i < SECTOR_COUNT; i += 2) {
        x0 = cx + (g_sectorDx[i] * 24) / 32;
        y0 = cy + (g_sectorDy[i] * 24) / 32;
        x1 = cx + g_sectorDx[i];
        y1 = cy + g_sectorDy[i];
        drawLine(x0, y0, x1, y1, BLUE);
    }
}

static void drawRadar(void)
{
    int cx = 64;
    int cy = 66;
    int threatX = cx + g_sectorDx[g_cachedThreatSector];
    int threatY = cy + g_sectorDy[g_cachedThreatSector];
    int shieldX = cx + (g_sectorDx[g_shieldSector] * 22) / 32;
    int shieldY = cy + (g_sectorDy[g_shieldSector] * 22) / 32;
    int sweepX = cx + g_sectorDx[g_sensor.sector];
    int sweepY = cy + g_sectorDy[g_sensor.sector];

    drawCircle(cx, cy, 34, WHITE);
    drawCircle(cx, cy, 22, BLUE);
    drawSectorMarkers(cx, cy);
    drawLine(cx, cy, sweepX, sweepY, CYAN);
    fillCircle(threatX, threatY, 4, (g_cachedThreat >= 9) ? RED : YELLOW);
    fillCircle(shieldX, shieldY, 4, GREEN);
    fillCircle(cx, cy, 3, WHITE);

    if (g_cachedBlocked) {
        drawLine(shieldX, shieldY, threatX, threatY, GREEN);
    } else if (g_state == RS_ACTIVE) {
        drawLine(shieldX, shieldY, threatX, threatY, RED);
    }
}

static void drawThreatMeter(int threat)
{
    int i;
    int x = 100;
    int y = 34;

    drawText(98, 24, CYAN, BLACK, "TH");
    drawRect(x - 2, y - 2, 18, 52, WHITE);
    for (i = 0; i < 15; i++) {
        unsigned int color = (i < 5) ? GREEN : ((i < 10) ? YELLOW : RED);
        if (i < threat) {
            fillRect(x, y + (14 - i) * 3, 12, 2, color);
        } else {
            fillRect(x, y + (14 - i) * 3, 12, 2, BLACK);
        }
    }
}

static void drawOLED(void)
{
    char line[40];
    char line2[40];
    unsigned long elapsed = loopsSince(g_stateStartLoop);
    int remaining = 0;
    int currentAttackMode;
    static int s_init = 0;
    static RoundState s_state = (RoundState)(-1);
    static int s_cloudOnline = -1;
    static int s_missionReady = -1;
    static int s_missionDifficulty = -1;
    static int s_defenderScore = -1;
    static int s_attackerScore = -1;
    static int s_sensorSector = -1;
    static int s_shieldSector = -1;
    static int s_threatSector = -1;
    static int s_threat = -1;
    static int s_blocked = -1;
    static int s_dist = -1;
    static int s_remaining = -1;
    static int s_attackMode = -1;
    int headerDirty;
    int scoreDirty;
    int radarDirty;
    int footerDirty;

    if (loopsSince(g_lastDrawLoop) < DRAW_INTERVAL_LOOPS) return;
    g_lastDrawLoop = g_loopCount;

    if (g_state == RS_ACTIVE) {
        if (elapsed < ACTIVE_ROUND_LOOPS) {
            remaining = (int)((ACTIVE_ROUND_LOOPS - elapsed) / 50);
        }
    } else if (g_state == RS_PREP) {
        remaining = (int)((PREP_LOOPS - elapsed) / 40);
    }
    if (remaining < 0) remaining = 0;
    currentAttackMode = attackMode();

    if (!s_init || s_state != g_state) {
        fillScreen(BLACK);
        fillRect(0, 0, 128, 14, BLUE);
        fillRect(0, 108, 128, 20, CYAN);
        drawText(4, 18, CYAN, BLACK, "DEF");
        drawText(4, 48, MAGENTA, BLACK, "ATK");
        s_init = 1;
        s_cloudOnline = -1;
        s_missionReady = -1;
        s_missionDifficulty = -1;
        s_defenderScore = -1;
        s_attackerScore = -1;
        s_sensorSector = -1;
        s_shieldSector = -1;
        s_threatSector = -1;
        s_threat = -1;
        s_blocked = -1;
        s_dist = -1;
        s_remaining = -1;
        s_attackMode = -1;
    }

    headerDirty = (s_state != g_state) ||
                  (s_cloudOnline != g_cloudOnline) ||
                  (s_missionReady != g_missionReady) ||
                  (s_missionDifficulty != g_missionDifficulty);
    scoreDirty = (s_state != g_state) ||
                 (s_defenderScore != g_defenderScore) ||
                 (s_attackerScore != g_attackerScore);
    radarDirty = (s_state != g_state) ||
                 (s_sensorSector != g_sensor.sector) ||
                 (s_shieldSector != g_shieldSector) ||
                 (s_threatSector != g_cachedThreatSector) ||
                 (s_threat != g_cachedThreat) ||
                 (s_blocked != g_cachedBlocked);
    footerDirty = (s_state != g_state) ||
                  (s_threat != g_cachedThreat) ||
                  (s_dist != g_sensor.distCm) ||
                  (s_shieldSector != g_shieldSector) ||
                  (s_threatSector != g_cachedThreatSector) ||
                  (s_remaining != remaining) ||
                  (s_attackMode != currentAttackMode);

    if (headerDirty) {
        fillRect(0, 0, 128, 14, BLUE);
        drawText(2, 3, WHITE, BLUE, stateLabel(g_state));
        snprintf(line, sizeof(line), "M%d %-5s", g_missionDifficulty, cloudLabel());
        drawText(56, 3, WHITE, BLUE, line);
    }

    if (scoreDirty) {
        fillRect(0, 28, 24, 12, BLACK);
        snprintf(line, sizeof(line), "%03d", g_defenderScore % 1000);
        drawText(4, 30, WHITE, BLACK, line);

        fillRect(0, 58, 24, 12, BLACK);
        snprintf(line, sizeof(line), "%03d", g_attackerScore % 1000);
        drawText(4, 60, WHITE, BLACK, line);
    }

    if (radarDirty) {
        fillRect(24, 16, 96, 92, BLACK);
        drawRadar();
        drawThreatMeter(g_cachedThreat);
        if (g_state == RS_END) {
            fillRect(20, 46, 88, 22, BLACK);
            drawRect(20, 46, 88, 22, WHITE);
            snprintf(line, sizeof(line), "%s WINS", winnerLabel());
            drawText(34, 53, WHITE, BLACK, line);
        }
    }

    if (footerDirty) {
        fillRect(0, 108, 128, 20, CYAN);
        if (g_state == RS_BOOT) {
            drawText(2, 111, BLACK, CYAN, "1 START  5 RESET");
            drawText(2, 119, BLACK, CYAN, "GRN=YOU ORG=THRT");
        } else if (g_state == RS_CALIBRATE) {
            drawText(2, 111, BLACK, CYAN, "CALIBRATING");
            drawText(2, 119, BLACK, CYAN, "HOLD STEADY");
        } else if (g_state == RS_MISSION) {
            drawText(2, 111, BLACK, CYAN, "AWS MISSION");
            snprintf(line, sizeof(line), "M%d %-5s", g_missionDifficulty, cloudLabel());
            drawText(2, 119, BLACK, CYAN, line);
        } else if (g_state == RS_PREP) {
            drawText(2, 111, BLACK, CYAN, "JOY MOVE  HAND THRT");
            snprintf(line, sizeof(line), "4/6/8 ATK   %02d", remaining);
            drawText(2, 119, BLACK, CYAN, line);
        } else if (g_state == RS_ACTIVE) {
            snprintf(line, sizeof(line), "TH%02d D%03d %s", g_cachedThreat, g_sensor.distCm, attackLabel());
            drawText(2, 111, BLACK, CYAN, line);
            snprintf(line2, sizeof(line2), "JOY SHLD  4P6J8B");
            drawText(2, 119, BLACK, CYAN, line2);
        } else if (g_state == RS_END) {
            drawText(2, 111, BLACK, CYAN, "END 5 RESET");
            snprintf(line, sizeof(line), "%s WINS", winnerLabel());
            drawText(2, 119, BLACK, CYAN, line);
        } else {
            snprintf(line, sizeof(line), "T%02d D%03d %-5s", g_cachedThreat, g_sensor.distCm, attackLabel());
            drawText(2, 111, BLACK, CYAN, line);
            snprintf(line2, sizeof(line2), "S%02d H%02d %02d", g_cachedThreatSector, g_shieldSector, remaining);
            drawText(2, 119, BLACK, CYAN, line2);
        }
    }

    s_state = g_state;
    s_cloudOnline = g_cloudOnline;
    s_missionReady = g_missionReady;
    s_missionDifficulty = g_missionDifficulty;
    s_defenderScore = g_defenderScore;
    s_attackerScore = g_attackerScore;
    s_sensorSector = g_sensor.sector;
    s_shieldSector = g_shieldSector;
    s_threatSector = g_cachedThreatSector;
    s_threat = g_cachedThreat;
    s_blocked = g_cachedBlocked;
    s_dist = g_sensor.distCm;
    s_remaining = remaining;
    s_attackMode = currentAttackMode;
}

static void logStatus(void)
{
    if (loopsSince(g_lastLogLoop) < LOG_INTERVAL_LOOPS) return;
    g_lastLogLoop = g_loopCount;

    UART_PRINT("DBG state=%s txL=%lu rxB=%lu rxL=%lu ok=%lu bad=%lu ovf=%lu irE=%lu irC=%lu joy=%d dist=%d tilt=%d cloud=%s op=%s err=%d\n\r",
               stateLabel(g_state),
               g_uart1TxLines,
               g_uart1RxBytes,
               g_uart1RxLines,
               g_softParseOk,
               g_softParseFail,
               g_uart1RxOverflow,
               g_irEdgeCount,
               g_irCodeCount,
               g_sensor.joy,
               g_sensor.distCm,
               g_sensor.tilt,
               cloudLabel(),
               g_lastCloudOp,
               g_lastCloudError);
}

int main(void)
{
    int button;

    BoardInit();
    PinMuxConfig();
    InitTerm();
    ClearTerm();

    UART_PRINT("\n\rAEGIS-172 booted\n\r");

    GPIO_IF_LedConfigure(LED1 | LED3);
    GPIO_IF_LedOff(MCU_ALL_LED_IND);

    SPIInit();
    Adafruit_Init();
    fillScreen(BLACK);
    setTextSize(1);
    setTextColor(WHITE, BLACK);

    IRInit();
    Uart1Init();
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    g_app_config.host = (signed char *)SERVER_NAME;
    g_app_config.port = SERVER_PORT;

    if (connectToAccessPoint() == SUCCESS) {
        set_time();
        g_sockID = tls_connect();
        if (g_sockID >= 0) {
            sl_Close(g_sockID);
            g_sockID = -1;
            g_cloudOnline = 1;
        } else {
            g_sockID = -1;
            g_cloudOnline = 0;
        }
    }

    setState(RS_BOOT);

    while (1) {
        Uart1PollRx();

        if (g_sensorReady) {
            if (parseSensorFrame(g_readyLine) == 0) {
                g_shadowDirty = 1;
            }
            g_softIdx = 0;
            g_softLine[0] = '\0';
            g_readyLine[0] = '\0';
            g_sensorReady = 0;
        }

        if (g_codeReady) {
            button = IRCodeToButton(g_irCmd);
            if (button >= 0 && loopsSince(g_lastIrAcceptLoop) >= IR_DEBOUNCE_LOOPS) {
                g_lastIrAcceptLoop = g_loopCount;
                UART_PRINT("IR: raw=0x%04lx btn=%d\n\r", g_irCmd, button);
                handleIrButton(button);
            }
            g_codeReady = 0;
        }

        updateStateMachine();
        sendControlFrame();

        drawOLED();
        logStatus();

        MAP_UtilsDelay(LOOP_DELAY_TICKS);
        g_loopCount++;
    }
}
