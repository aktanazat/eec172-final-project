//*****************************************************************************
// ParkPilot Final Project Firmware (CC3200)
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
#include "timer.h"

#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"

#include "Adafruit_SSD1351.h"
#include "Adafruit_GFX.h"
#include "i2c_if.h"
#include "utils/network_utils.h"

#if defined(SELF_TEST) && !defined(PARKPILOT_ENABLE_SELF_TEST)
#undef SELF_TEST
#endif

#define SERVER_NAME           "a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com"
#define SERVER_PORT           8443

#define POSTHEADER "POST /things/akge_cc3200_board/shadow HTTP/1.1\r\n"
#define GETHEADER  "GET /things/akge_cc3200_board/shadow HTTP/1.1\r\n"
#define HOSTHEADER "Host: a126k3e19n75q0-ats.iot.us-east-2.amazonaws.com\r\n"
#define CHEADER    "Connection: Keep-Alive\r\n"
#define CTHEADER   "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1  "Content-Length: "
#define CLHEADER2  "\r\n\r\n"

#define DATE    19
#define MONTH   2
#define YEAR    2026
#define HOUR    22
#define MINUTE  30
#define SECOND  0

#define IR_GPIO_PORT         GPIOA1_BASE
#define IR_GPIO_PIN          0x10
#define SYSCLKFREQ           80000000ULL
#define SYSTICK_RELOAD_VAL   8000000UL
#define TICKS_TO_US(ticks) ((((ticks) / SYSCLKFREQ) * 1000000ULL) + ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))

#define UART1_BAUD           9600

#define BMA222_ADDR          0x18
#define COLLISION_THRESH2    280000

#define SHADOW_BUF_SIZE      4096
#define S3_BUF_SIZE          4096
#define S3_URL_BUF_SIZE      2048

#define MODE_IDLE            0
#define MODE_MANUAL          1
#define MODE_AVOID           2
#define MODE_PARK            3

#define PARK_SEARCH_GAP      0
#define PARK_REQUEST_CLOUD   1
#define PARK_WAIT_GUIDANCE   2
#define PARK_EXEC_1          3
#define PARK_EXEC_2          4
#define PARK_EXEC_3          5
#define PARK_DONE            6

#define DIST_NEAR            25
#define DIST_MED             50
#define DIST_FAR             80

#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

volatile unsigned long g_irCmd = 0;
volatile int g_bitCount = 0;
volatile int g_codeReady = 0;

volatile int g_sensorReady = 0;
unsigned long g_uart1RxBytes = 0;
unsigned long g_uart1RxLines = 0;
unsigned long g_uart1RxOverflow = 0;

char g_softLine[80];
volatile int g_softIdx = 0;
unsigned long g_softParseOk = 0;
unsigned long g_softParseFail = 0;

int g_mode = MODE_IDLE;
int g_parkState = PARK_SEARCH_GAP;
unsigned long g_parkCounter = 0;

int g_speed = 40;
int g_targetSpeed = 0;
int g_targetSteer = 0;

int g_front = 120, g_right = 120, g_left = 120, g_rear = 120;
int g_collision = 0;
int g_sockID = -1;

char g_httpBuf[SHADOW_BUF_SIZE];
char g_s3Buf[S3_BUF_SIZE];
char g_s3GuidanceUrl[S3_URL_BUF_SIZE];
char g_lastS3GuidanceUrl[S3_URL_BUF_SIZE];
int g_s3GuidanceFetched = 0;

#ifdef SELF_TEST
volatile int g_selfTestForceCollision = 0;
#endif

static inline void SysTickReset(void) { HWREG(NVIC_ST_CURRENT) = 1; }

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
    MAP_GPIOIntClear(IR_GPIO_PORT, status);
    if (!(status & IR_GPIO_PIN) || g_codeReady) return;

    unsigned long us = TICKS_TO_US(SYSTICK_RELOAD_VAL - MAP_SysTickValueGet());
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
    if (g_bitCount == 48) g_codeReady = 1;
}

static int IRCodeToButton(unsigned long cmd)
{
    switch (cmd) {
        case 0x0809: return 1;
        case 0x8889: return 2;
        case 0x4849: return 3;
        case 0xC8C9: return 4;
        case 0x2829: return 5;
        case 0xA8A9: return 6;
        case 0xE8E9: return 8;
        case 0x4C4D: return 10; // MUTE
        case 0x2C2D: return 12;
        case 0xACAD: return 13;
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

        if (g_softIdx >= (int)sizeof(g_softLine) - 1) {
            g_uart1RxOverflow++;
            g_softIdx = 0;
            continue;
        }

        g_softLine[g_softIdx++] = (char)c;
        g_softLine[g_softIdx] = '\0';

        if (c == '\n' || c == '\r') {
            g_uart1RxLines++;
            g_sensorReady = 1;
        }
    }
}

static void Uart1TxString(const char *s)
{
    while (*s) MAP_UARTCharPut(UARTA1_BASE, (unsigned char)*s++);
}

static void sendMotorCommand(int spd, int ang)
{
    static int lastSpd = 9999;
    static int lastAng = 9999;
    char msg[24];

    snprintf(msg, sizeof(msg), "$M,%03d,%+03d\n", spd, ang);
    Uart1TxString(msg);

    if (spd != lastSpd || ang != lastAng) {
        UART_PRINT("Motor cmd: %s", msg);
        lastSpd = spd;
        lastAng = ang;
    }
}

static int parseSensorFrame(const char *line)
{
    int f, r, l, b;
    if (sscanf(line, "$S,%d,%d,%d,%d", &f, &r, &l, &b) == 4) {
        g_front = f;
        g_right = r;
        g_left = l;
        g_rear = b;
        return 0;
    }
    return -1;
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

static int http_post_json(int sock, const char *json)
{
    char sendBuf[900];
    char recvBuf[512];
    char lenBuf[16];
    char *p = sendBuf;
    int ret;

    strcpy(p, POSTHEADER); p += strlen(POSTHEADER);
    strcpy(p, HOSTHEADER); p += strlen(HOSTHEADER);
    strcpy(p, CHEADER); p += strlen(CHEADER);
    strcpy(p, CTHEADER); p += strlen(CTHEADER);
    strcpy(p, CLHEADER1); p += strlen(CLHEADER1);
    sprintf(lenBuf, "%d", (int)strlen(json));
    strcpy(p, lenBuf); p += strlen(lenBuf);
    strcpy(p, CLHEADER2); p += strlen(CLHEADER2);
    strcpy(p, json); p += strlen(json);

    ret = sl_Send(sock, sendBuf, strlen(sendBuf), 0);
    if (ret < 0) return ret;

    ret = sl_Recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
    if (ret == SL_EAGAIN) return 0;
    if (ret < 0) return ret;
    recvBuf[ret] = 0;

    if (strstr(recvBuf, "HTTP/1.1 4") || strstr(recvBuf, "HTTP/1.1 5")) {
        UART_PRINT("Shadow POST server error\n\r");
        return -2;
    }

    // AWS may return body-only chunks on keep-alive sockets; treat as success.
    return 0;
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

static int extract_json_string_value(const char *json, const char *key, char *out, int outSize)
{
    char pattern[96];
    const char *p;
    int truncated = 0;
    int i = 0;

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);

    while (*p && *p != '"' && i < outSize - 1) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    if (*p && *p != '"') truncated = 1;
    out[i] = '\0';
    if (truncated) return -2;
    return (i > 0) ? 0 : -1;
}

static int parse_https_url(const char *url, char *host, int hostSize, char *path, int pathSize)
{
    const char *p;
    const char *slash;
    int hostLen;
    int pathLen;

    if (strncmp(url, "https://", 8) != 0) return -1;
    p = url + 8;
    slash = strchr(p, '/');
    if (!slash) return -1;

    hostLen = (int)(slash - p);
    if (hostLen <= 0 || hostLen >= hostSize) return -1;
    memcpy(host, p, hostLen);
    host[hostLen] = '\0';

    pathLen = (int)strlen(slash);
    if (pathLen <= 0 || pathLen >= pathSize) return -1;
    memcpy(path, slash, pathLen + 1);
    return 0;
}

static int https_get_url(const char *url, char *resp, int respSize)
{
    char host[256];
    char path[S3_URL_BUF_SIZE];
    char req[3072];
    unsigned int uiIP;
    SlSockAddrIn_t addr;
    SlTimeval_t recvTimeout;
    unsigned char ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256;
    int sock;
    int total = 0;
    long ret;

    if (parse_https_url(url, host, sizeof(host), path, sizeof(path)) < 0) return -10;

    ret = sl_NetAppDnsGetHostByName((signed char *)host, strlen(host), (unsigned long *)&uiIP, SL_AF_INET);
    if (ret < 0) return (int)ret;

    sock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET);
    if (sock < 0) return sock;

    ret = sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod, sizeof(ucMethod));
    if (ret < 0) {
        sl_Close(sock);
        return (int)ret;
    }

    ret = sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher, sizeof(uiCipher));
    if (ret < 0) {
        sl_Close(sock);
        return (int)ret;
    }

    recvTimeout.tv_sec = 2;
    recvTimeout.tv_usec = 0;
    ret = sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (unsigned char *)&recvTimeout, sizeof(recvTimeout));
    if (ret < 0) {
        sl_Close(sock);
        return (int)ret;
    }

    addr.sin_family = SL_AF_INET;
    addr.sin_port = sl_Htons(443);
    addr.sin_addr.s_addr = sl_Htonl(uiIP);

    ret = sl_Connect(sock, (SlSockAddr_t *)&addr, sizeof(SlSockAddrIn_t));
    if (ret < 0 && ret != SL_ESECSNOVERIFY) {
        sl_Close(sock);
        return (int)ret;
    }

    if (snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host) >= (int)sizeof(req)) {
        sl_Close(sock);
        return -11;
    }

    ret = sl_Send(sock, req, strlen(req), 0);
    if (ret < 0) {
        sl_Close(sock);
        return (int)ret;
    }

    ret = sl_Recv(sock, resp, respSize - 1, 0);
    if (ret == SL_EAGAIN) {
        sl_Close(sock);
        return -12;
    }
    if (ret < 0) {
        sl_Close(sock);
        return (int)ret;
    }

    total = (int)ret;
    resp[total] = '\0';

    if (!(strstr(resp, "200 OK") || strstr(resp, "\"steps\""))) {
        ret = sl_Recv(sock, resp + total, respSize - total - 1, 0);
        if (ret > 0) {
            total += (int)ret;
            resp[total] = '\0';
        }
    }

    sl_Close(sock);

    if (strstr(resp, "200 OK") || strstr(resp, "\"steps\"")) return 0;
    UART_PRINT("S3 GET unexpected response: %.96s\n\r", resp);
    return -13;
}

static void awsShadowUpdate(int collision)
{
    char payload[320];
    int postRet;
    snprintf(payload, sizeof(payload),
        "{\"state\":{\"reported\":{\"mode\":%d,\"speed\":%d,\"front\":%d,\"right\":%d,\"left\":%d,\"rear\":%d,\"collision\":%s}}}",
        g_mode, g_speed, g_front, g_right, g_left, g_rear, collision ? "true" : "false");

    if (g_sockID < 0) g_sockID = tls_connect();
    if (g_sockID < 0) return;

    postRet = http_post_json(g_sockID, payload);
    if (postRet < 0) {
        UART_PRINT("Shadow POST failed: %d\n\r", postRet);
        sl_Close(g_sockID);
        g_sockID = tls_connect();
        if (g_sockID >= 0) {
            postRet = http_post_json(g_sockID, payload);
            if (postRet < 0) UART_PRINT("Shadow POST retry failed: %d\n\r", postRet);
        }
    }
}

static void requestCloudParking(void)
{
    const char *payload = "{\"state\":{\"desired\":{\"var\":\"PARK_REQUEST\"}}}";
    if (g_sockID >= 0) http_post_json(g_sockID, payload);
}

static int hasCloudGuidance(void)
{
    int urlRet;
    int s3Ret;

    if (g_sockID < 0) return 0;
    if (http_get_shadow(g_sockID, g_httpBuf, sizeof(g_httpBuf)) < 0) return 0;

    urlRet = extract_json_string_value(g_httpBuf, "park_guidance_url", g_s3GuidanceUrl, sizeof(g_s3GuidanceUrl));
    if (urlRet == -2) {
        UART_PRINT("park_guidance_url truncated\n\r");
    }

    if (urlRet == 0) {
        if (!g_s3GuidanceFetched || strcmp(g_s3GuidanceUrl, g_lastS3GuidanceUrl) != 0) {
            s3Ret = https_get_url(g_s3GuidanceUrl, g_s3Buf, sizeof(g_s3Buf));
            if (s3Ret == 0) {
                strncpy(g_lastS3GuidanceUrl, g_s3GuidanceUrl, sizeof(g_lastS3GuidanceUrl) - 1);
                g_lastS3GuidanceUrl[sizeof(g_lastS3GuidanceUrl) - 1] = '\0';
                g_s3GuidanceFetched = 1;
                UART_PRINT("S3 guidance fetched\n\r");
            } else {
                UART_PRINT("S3 guidance fetch failed: %d\n\r", s3Ret);
            }
        }
    }

    if (g_s3GuidanceFetched) return 1;
    return (strstr(g_httpBuf, "park_guidance") != NULL);
}

static int bmaReadAccel(int *mag2)
{
    unsigned char reg = 0x02;
    unsigned char data[6];
    short x, y, z;

    if (I2C_IF_ReadFrom(BMA222_ADDR, &reg, 1, data, 6) < 0) return -1;

    x = (short)(((short)data[1] << 8) | data[0]) >> 6;
    y = (short)(((short)data[3] << 8) | data[2]) >> 6;
    z = (short)(((short)data[5] << 8) | data[4]) >> 6;

    *mag2 = (x * x) + (y * y) + (z * z);
    return 0;
}

static void drawBar(int x, int y, int w, int h, int dist)
{
    unsigned int c;
    int fill = h;

    if (dist < DIST_NEAR) c = RED;
    else if (dist < DIST_MED) c = YELLOW;
    else c = GREEN;

    if (dist > DIST_FAR) dist = DIST_FAR;
    fill = (h * dist) / DIST_FAR;
    if (fill < 2) fill = 2;

    drawRect(x, y, w, h, WHITE);
    fillRect(x + 1, y + 1, w - 2, h - 2, BLACK);
    fillRect(x + 1, y + h - fill, w - 2, fill - 1, c);
}

static void drawOLED(void)
{
    char line[32];

    fillRect(0, 0, 128, 16, BLUE);
    setCursor(2, 4);
    if (g_mode == MODE_IDLE) strcpy(line, "IDLE");
    else if (g_mode == MODE_MANUAL) strcpy(line, "MANUAL");
    else if (g_mode == MODE_AVOID) strcpy(line, "AVOID");
    else strcpy(line, "PARK");
    Outstr(line);

    snprintf(line, sizeof(line), " S:%03d", g_speed);
    setCursor(70, 4); Outstr(line);

    fillRect(0, 16, 128, 96, BLACK);
    drawRect(56, 56, 16, 16, WHITE); // car center

    drawBar(58, 20, 12, 28, g_front);
    drawBar(98, 58, 12, 28, g_right);
    drawBar(18, 58, 12, 28, g_left);
    drawBar(58, 90, 12, 18, g_rear);

    fillRect(0, 112, 128, 16, CYAN);
    snprintf(line, sizeof(line), "F%03d R%03d L%03d B%03d", g_front, g_right, g_left, g_rear);
    setCursor(0, 116); Outstr(line);
}

static void handleIrButton(int b)
{
    if (b == 10) {
        g_mode = MODE_IDLE;
        g_targetSpeed = 0;
        g_targetSteer = 0;
        return;
    }

    if (b == 1) g_mode = MODE_MANUAL;
    else if (b == 2 && g_mode == MODE_IDLE) g_mode = MODE_AVOID;
    else if (b == 3) {
        g_mode = MODE_PARK;
        g_parkState = PARK_SEARCH_GAP;
        g_parkCounter = 0;
    }

    if (g_mode != MODE_MANUAL) return;

    if (b == 2) { g_targetSpeed = g_speed; g_targetSteer = 0; }
    else if (b == 8) { g_targetSpeed = -g_speed / 2; g_targetSteer = 0; }
    else if (b == 4) { g_targetSteer = -45; }
    else if (b == 6) { g_targetSteer = 45; }
    else if (b == 5) { g_targetSpeed = 0; g_targetSteer = 0; }
    else if (b == 12 && g_speed < 95) { g_speed += 5; }
    else if (b == 13 && g_speed > 15) { g_speed -= 5; }
}

static void runAvoidLogic(void)
{
    if (g_front < DIST_NEAR) {
        if (g_left < DIST_NEAR && g_right < DIST_NEAR) {
            g_targetSpeed = -25;
            g_targetSteer = (g_left > g_right) ? -40 : 40;
        } else if (g_left > g_right) {
            g_targetSpeed = 25;
            g_targetSteer = -45;
        } else {
            g_targetSpeed = 25;
            g_targetSteer = 45;
        }
    } else {
        g_targetSteer = 0;
        if (g_front > DIST_FAR) g_targetSpeed = g_speed;
        else g_targetSpeed = (g_speed * g_front) / DIST_FAR;
    }
}

static void runParkLogic(void)
{
    g_parkCounter++;

    switch (g_parkState) {
        case PARK_SEARCH_GAP:
            g_targetSpeed = 20;
            g_targetSteer = 0;
            if (g_right > 70) {
                g_parkState = PARK_REQUEST_CLOUD;
            }
            break;

        case PARK_REQUEST_CLOUD:
            g_targetSpeed = 0;
            g_s3GuidanceFetched = 0;
            g_s3GuidanceUrl[0] = '\0';
            g_lastS3GuidanceUrl[0] = '\0';
            requestCloudParking();
            g_parkCounter = 0;
            g_parkState = PARK_WAIT_GUIDANCE;
            break;

        case PARK_WAIT_GUIDANCE:
            g_targetSpeed = 0;
            if (hasCloudGuidance() || g_parkCounter > 20) {
                g_parkState = PARK_EXEC_1;
                g_parkCounter = 0;
            }
            break;

        case PARK_EXEC_1: // pull forward
            g_targetSpeed = 20;
            g_targetSteer = 35;
            if (g_parkCounter > 10) { g_parkState = PARK_EXEC_2; g_parkCounter = 0; }
            break;

        case PARK_EXEC_2: // reverse cut
            g_targetSpeed = -20;
            g_targetSteer = -45;
            if (g_parkCounter > 14) { g_parkState = PARK_EXEC_3; g_parkCounter = 0; }
            break;

        case PARK_EXEC_3: // straighten
            g_targetSpeed = -15;
            g_targetSteer = 0;
            if (g_parkCounter > 10) { g_parkState = PARK_DONE; }
            break;

        case PARK_DONE:
        default:
            g_targetSpeed = 0;
            g_targetSteer = 0;
            g_mode = MODE_IDLE;
            break;
    }
}

#ifdef SELF_TEST
static void runSelfTest(int loop)
{
    static int sensorPhase = 0;
    static const char *frames[] = {
        "$S,090,040,045,080\n", // clear front
        "$S,045,028,055,070\n", // moderate front
        "$S,020,015,030,060\n", // near obstacle
        "$S,075,085,050,055\n", // right-side open gap
        "$S,060,030,080,040\n"  // left-side open
    };

    if ((loop % 20) == 0) {
        strcpy(g_softLine, frames[sensorPhase]);
        g_softIdx = (int)strlen(g_softLine);
        g_sensorReady = 1;
        sensorPhase = (sensorPhase + 1) % (sizeof(frames) / sizeof(frames[0]));
    }

    if ((loop % 100) == 0) {
        UART_PRINT("SELF_TEST heartbeat loop=%d mode=%d\n\r", loop, g_mode);
    }

    if (loop == 10)  { UART_PRINT("SELF_TEST: MANUAL\n\r"); g_mode = MODE_MANUAL; }
    if (loop == 30)  { UART_PRINT("SELF_TEST: MANUAL FWD\n\r"); g_targetSpeed = g_speed; g_targetSteer = 0; }
    if (loop == 70)  { UART_PRINT("SELF_TEST: MANUAL STOP\n\r"); g_targetSpeed = 0; g_targetSteer = 0; }
    if (loop == 100) { UART_PRINT("SELF_TEST: IDLE\n\r"); g_mode = MODE_IDLE; }
    if (loop == 130) { UART_PRINT("SELF_TEST: AVOID\n\r"); g_mode = MODE_AVOID; }
    if (loop == 210) { UART_PRINT("SELF_TEST: PARK\n\r"); g_mode = MODE_PARK; g_parkState = PARK_SEARCH_GAP; g_parkCounter = 0; }

    if (loop == 280) { UART_PRINT("SELF_TEST: FORCE COLLISION ON\n\r"); g_selfTestForceCollision = 1; }
    if (loop == 320) { UART_PRINT("SELF_TEST: FORCE COLLISION OFF\n\r"); g_selfTestForceCollision = 0; }

    if (loop == 380) { UART_PRINT("SELF_TEST: BACK TO IDLE\n\r"); g_mode = MODE_IDLE; g_targetSpeed = 0; g_targetSteer = 0; }
}
#endif

int main(void)
{
    int loop = 0;

    BoardInit();
    PinMuxConfig();
    InitTerm();
    ClearTerm();

    UART_PRINT("\n\rParkPilot booted\n\r");

    GPIO_IF_LedConfigure(LED1 | LED3);
    GPIO_IF_LedOff(MCU_ALL_LED_IND);

    SPIInit();
    Adafruit_Init();
    fillScreen(BLACK);

    IRInit();
    Uart1Init();

    I2C_IF_Open(I2C_MASTER_MODE_FST);

#ifndef SELF_TEST
    g_app_config.host = (signed char *)SERVER_NAME;
    g_app_config.port = SERVER_PORT;

    connectToAccessPoint();
    set_time();
    g_sockID = tls_connect();
#else
    UART_PRINT("SELF_TEST enabled: skipping WiFi/TLS init\n\r");
#endif

    while (1) {
        int accelMag2 = 0;

#ifdef SELF_TEST
        runSelfTest(loop);
#endif

        Uart1PollRx();

        if (g_codeReady) {
            int b = IRCodeToButton(g_irCmd);
            if (b >= 0) handleIrButton(b);
            g_codeReady = 0;
        }

        if (g_sensorReady) {
            int parseRet;
            UART_PRINT("Sensor frame: %s\n\r", g_softLine);
            parseRet = parseSensorFrame(g_softLine);
            if (parseRet == 0) {
                g_softParseOk++;
            } else {
                g_softParseFail++;
                UART_PRINT("Sensor frame parse failed\n\r");
            }
            g_softIdx = 0;
            g_softLine[0] = '\0';
            g_sensorReady = 0;
        }

        if ((loop % 32) == 0) {
            UART_PRINT("UART dbg baud=%d rxB=%lu rxL=%lu ok=%lu bad=%lu ovf=%lu\n\r",
                       UART1_BAUD,
                       g_uart1RxBytes,
                       g_uart1RxLines,
                       g_softParseOk,
                       g_softParseFail,
                       g_uart1RxOverflow);
        }

        if ((loop % 5) == 0) {
#ifdef SELF_TEST
            if (g_selfTestForceCollision) {
                g_collision = 1;
                g_targetSpeed = 0;
                g_targetSteer = 0;
                fillScreen(RED);
            } else {
                g_collision = 0;
            }
#else
            if (bmaReadAccel(&accelMag2) == 0 && accelMag2 > COLLISION_THRESH2) {
                g_collision = 1;
                g_targetSpeed = 0;
                g_targetSteer = 0;
                fillScreen(RED);
            } else {
                g_collision = 0;
            }
#endif
        }

        if (g_mode == MODE_IDLE) {
            g_targetSpeed = 0;
            g_targetSteer = 0;
        } else if (g_mode == MODE_AVOID) {
            runAvoidLogic();
        } else if (g_mode == MODE_PARK) {
            runParkLogic();
        }

        if ((loop % 2) == 0) {
            sendMotorCommand(g_targetSpeed, g_targetSteer);
        }

        if ((loop % 8) == 0) {
            drawOLED();
        }

#ifndef SELF_TEST
        if ((loop % 16) == 0) {
            awsShadowUpdate(g_collision);
        }
#endif

        MAP_UtilsDelay(120000);
        loop++;
    }
}
