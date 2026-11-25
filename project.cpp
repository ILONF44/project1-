#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"
#include "face-bitmaps.h"
#include "movement-sequences.h"
#include "web-page.h"

DNSServer        dnsServer;
WebServer        server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo            servos[SERVO_COUNT];

const int servoPins[SERVO_COUNT]  = SERVO_PINS;

int8_t    servoSubtrim[SERVO_COUNT] = SERVO_SUBTRIM;

#define DNS_PORT 53

int frameDelay        = DEFAULT_FRAME_DELAY;
int walkCycles        = DEFAULT_WALK_CYCLES;
int motorCurrentDelay = DEFAULT_MOTOR_DELAY;
int faceFps           = DEFAULT_FACE_FPS;
char currentCommand[24] = "";

char currentFaceName[24] = "default";

const unsigned char* const* currentFaceFrames = NULL;
uint8_t       currentFaceFrameCount = 0;
uint8_t       currentFaceFrameIndex = 0;
unsigned long lastFaceFrameMs    = 0;
FaceAnimMode  currentFaceMode  = FACE_ANIM_LOOP;
int8_t        faceFrameDirection    = 1;
bool          faceAnimFinished    = false;
int           currentFaceFps   = 0;

bool          idleActive           = false;
bool          idleBlinkActive      = false;
unsigned long nextIdleBlinkMs      = 0;
uint8_t       idleBlinkRepeatsLeft = 0;

unsigned long lastInputTime      = 0;
bool          firstInputReceived = false;
bool          showingWifiInfo    = false;
int           wifiScrollPos      = 0;
unsigned long lastWifiScrollMs   = 0;
char          wifiInfoText[200]  = "";

bool      networkConnected = false;
IPAddress networkIP;
bool      mdnsOk = false;
typedef enum {
  WIFI_SETUP_IDLE,
  WIFI_SETUP_QUEUED,
  WIFI_SETUP_CONNECTING
} WifiSetupState;

WifiSetupState wifiSetupState    = WIFI_SETUP_IDLE;
char           wifiSetupSsid[33] = "";
char           wifiSetupPass[65] = "";
char           wifiSetupError[64] = "";
unsigned long  wifiSetupQueuedMs = 0;
unsigned long  wifiSetupStartMs  = 0;
bool           wifiRestoreApOnly = false;

#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_SETUP_START_DELAY_MS 300

typedef struct {
  const char* name;
  const unsigned char* const* frames;
  uint8_t maxFrames;
} FaceEntry;

#define MAX_FACE_FRAMES 6
#define MAKE_FACE_FRAMES(name) \
  const unsigned char* const face_##name##_frames[] = { \
    epd_bitmap_##name, epd_bitmap_##name##_1, epd_bitmap_##name##_2, \
    epd_bitmap_##name##_3, epd_bitmap_##name##_4, epd_bitmap_##name##_5 \
  };
#define X(name) MAKE_FACE_FRAMES(name)
FACE_LIST
#undef X
#undef MAKE_FACE_FRAMES

const FaceEntry faceEntries[] = {
#define X(name) { #name, face_##name##_frames, MAX_FACE_FRAMES },
  FACE_LIST
#undef X

  { "default", face_defualt_frames, MAX_FACE_FRAMES }
};

#define FACE_COUNT (sizeof(faceEntries) / sizeof(faceEntries[0]))

typedef struct {
  const char* name;
  uint8_t fps;
} FaceFpsEntry;

const FaceFpsEntry faceFpsEntries[] = {
  { "walk", 1 }, { "rest", 1 }, { "swim", 1 }, { "dance", 1 }, { "wave", 1 },
  { "point", 5 }, { "stand", 1 }, { "cute", 1 }, { "pushup", 1 }, { "freaky", 1 },
  { "bow", 1 }, { "worm", 1 }, { "shake", 1 }, { "shrug", 1 }, { "dead", 2 },
  { "crab", 1 }, { "idle", 1 }, { "idle_blink", 7 }, { "default", 1 },

  { "happy", 1 }, { "talk_happy", 1 }, { "sad", 1 }, { "talk_sad", 1 },
  { "angry", 1 }, { "talk_angry", 1 }, { "surprised", 1 }, { "talk_surprised", 1 },
  { "sleepy", 1 }, { "talk_sleepy", 1 }, { "love", 1 }, { "talk_love", 1 },
  { "excited", 1 }, { "talk_excited", 1 }, { "confused", 1 }, { "talk_confused", 1 },
  { "thinking", 1 }, { "talk_thinking", 1 },
};
#define FACE_FPS_COUNT (sizeof(faceFpsEntries) / sizeof(faceFpsEntries[0]))

void copyText(char* dst, size_t size, const char* src);
void appendText(char* buf, size_t size, size_t* pos, const char* fmt, ...);
void jsonEscape(char* out, size_t size, const char* in);
bool jsonGetString(const char* json, const char* key, char* out, size_t size);
void ipToText(IPAddress ip, char* out, size_t size);
void getArg(const char* name, char* out, size_t size);
int  getArgInt(const char* name);

void setCommand(const char* cmd);
void clearCommand(void);
bool isCommand(const char* cmd);
void runCommandFromLoop(void);
void handleSerial(void);
void printHelp(void);

void updateFaceBitmap(const unsigned char* bitmap);
uint8_t countFrames(const unsigned char* const* frames, uint8_t maxFrames);
int  getFaceFpsForName(const char* faceName);
void updateAnimatedFace(void);
void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs);
void exitIdle(void);
void updateIdleBlink(void);
void recordInput(void);
void updateWifiInfoScroll(void);

bool startMdns(void);
void announceNetwork(const char* ssid);
void setApOnlyInfoText(void);
void showWifiInfoNow(void);
bool connectToWifi(const char* ssid, const char* pass, uint32_t timeoutMs);
void finishWifiSetup(const char* err);
void updateWifiSetup(void);

void handleRoot(void);
void handleCommandWeb(void);
void handleGetSettings(void);
void handleSetSettings(void);
void handleGetStatus(void);
void handleApiCommand(void);
void handleWifiScan(void);
void handleWifiConnect(void);
void handleWifiStatus(void);
void handleNotFound(void);
void sendJson(int code, const char* json);

void copyText(char* dst, size_t size, const char* src) {
  if (size == 0) return;
  if (src == NULL) src = "";
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

void appendText(char* buf, size_t size, size_t* pos, const char* fmt, ...) {
  if (*pos >= size - 1) return;
  va_list args;
  va_start(args, fmt);

  int written = vsnprintf(buf + *pos, size - *pos, fmt, args);
  va_end(args);
  if (written < 0) return;
  *pos += (size_t)written;
  if (*pos > size - 1) *pos = size - 1;
}

void jsonEscape(char* out, size_t size, const char* in) {
  size_t o = 0;

  for (; *in != '\0' && o + 7 < size; in++) {
    unsigned char c = (unsigned char)*in;
    if (c == '\\' || c == '"') {
      out[o++] = '\\';
      out[o++] = (char)c;
    } else if (c < 0x20) {
      o += snprintf(out + o, size - o, "\\u%04x", c);
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = '\0';
}

bool jsonGetString(const char* json, const char* key, char* out, size_t size) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);

  const char* p = strstr(json, pattern);
  if (p == NULL) return false;
  p += strlen(pattern);

  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != ':') return false;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != '"') return false;
  p++;

  size_t i = 0;

  while (*p != '\0' && *p != '"' && i < size - 1) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return (*p == '"');
}

void ipToText(IPAddress ip, char* out, size_t size) {
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void getArg(const char* name, char* out, size_t size) {

  copyText(out, size, server.arg(name).c_str());
}

int getArgInt(const char* name) {
  return atoi(server.arg(name).c_str());
}

void sendJson(int code, const char* json) {
  server.send(code, "application/json", json);
}

bool startMdns(void) {
  mdnsOk = MDNS.begin(DEVICE_HOSTNAME);
  if (mdnsOk) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS started: http://%s.local\n", DEVICE_HOSTNAME);
  } else {
    Serial.println("Error setting up mDNS responder!");
  }
  return mdnsOk;
}

void announceNetwork(const char* ssid) {
  char apIp[16], netIp[16];
  MDNS.end();
  startMdns();
  ipToText(WiFi.softAPIP(), apIp, sizeof(apIp));
  ipToText(networkIP, netIp, sizeof(netIp));

  snprintf(wifiInfoText, sizeof(wifiInfoText),
           "AP: %s (%s)  |  Network: %s (%s) or %s.local  |  ",
           AP_SSID, apIp, ssid, netIp, DEVICE_HOSTNAME);
}

void setApOnlyInfoText(void) {
  char apIp[16];
  ipToText(WiFi.softAPIP(), apIp, sizeof(apIp));
  snprintf(wifiInfoText, sizeof(wifiInfoText),
           "Connect to WiFi: %s  |  Pass: %s  |  IP: %s  |  Captive Portal will auto-open!  |  ",
           AP_SSID, AP_PASS, apIp);
}

void showWifiInfoNow(void) {
  firstInputReceived = false;
  lastInputTime      = millis() - 30000;
  showingWifiInfo    = false;
}

bool connectToWifi(const char* ssid, const char* pass, uint32_t timeoutMs) {
  if (ssid == NULL || ssid[0] == '\0') return false;

  Serial.printf("Connecting to WiFi network: %s\n", ssid);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid, pass);

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) break;

    if (millis() - start > 1000 && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL)) break;
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connect failed.");
    WiFi.disconnect();
    return false;
  }

  networkConnected = true;
  networkIP = WiFi.localIP();
  char ip[16];
  ipToText(networkIP, ip, sizeof(ip));
  Serial.printf("Connected! IP: %s\n", ip);
  return true;
}

void finishWifiSetup(const char* err) {
  copyText(wifiSetupError, sizeof(wifiSetupError), err);
  memset(wifiSetupPass, 0, sizeof(wifiSetupPass));

  wifiSetupState = WIFI_SETUP_IDLE;
}

void updateWifiSetup(void) {

  if (wifiSetupState == WIFI_SETUP_IDLE) {

    static unsigned long lastCheckMs = 0;
    if (millis() - lastCheckMs < 5000) return;
    lastCheckMs = millis();

    bool live = (WiFi.status() == WL_CONNECTED);
    if (live != networkConnected) {
      networkConnected = live;
      if (live) {
        networkIP = WiFi.localIP();
        announceNetwork(WiFi.SSID().c_str());
      } else {
        Serial.println("Station link lost.");
      }
    }
    return;
  }

  if (wifiSetupState == WIFI_SETUP_QUEUED) {

    if (millis() - wifiSetupQueuedMs < WIFI_SETUP_START_DELAY_MS) return;
    Serial.printf("Connecting to WiFi network: %s\n", wifiSetupSsid);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.begin(wifiSetupSsid, wifiSetupPass);
    wifiSetupStartMs = millis();
    wifiSetupState   = WIFI_SETUP_CONNECTING;
    return;
  }

  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    networkConnected = true;
    networkIP = WiFi.localIP();
    char ip[16];
    ipToText(networkIP, ip, sizeof(ip));
    Serial.printf("Connected! IP: %s\n", ip);
    announceNetwork(wifiSetupSsid);
    showWifiInfoNow();
    finishWifiSetup("");
    return;
  }

  bool terminal = (millis() - wifiSetupStartMs > 1000) &&
                  (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL);

  if (terminal || millis() - wifiSetupStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
    const char* err;
    if (st == WL_NO_SSID_AVAIL)       err = "Network not found";
    else if (st == WL_CONNECT_FAILED) err = "Wrong password or connection rejected";
    else                              err = "Connection timed out";

    Serial.printf("WiFi connect failed: %s\n", err);
    WiFi.disconnect();
    networkConnected = false;
    setApOnlyInfoText();
    finishWifiSetup(err);
  }

}

void handleRoot(void) {
  server.send(200, "text/html", index_html);
}

void handleCommandWeb(void) {
  char value[24];

  if (server.hasArg("pose") || server.hasArg("go")) {

    getArg(server.hasArg("pose") ? "pose" : "go", value, sizeof(value));
    setCommand(value);
    recordInput();
    exitIdle();
    server.send(200, "text/plain", "OK");
  }

  else if (server.hasArg("stop")) {
    clearCommand();
    recordInput();
    server.send(200, "text/plain", "OK");
  }

  else if (server.hasArg("motor") && server.hasArg("value")) {
    char motorText[8];
    getArg("motor", motorText, sizeof(motorText));
    int motorNum = atoi(motorText);
    int servoIdx = servoNameToIndex(motorText);
    int angle    = getArgInt("value");

    if (angle < 0 || angle > 180) {
      server.send(400, "text/plain", "Invalid motor or angle");
    } else if (motorNum >= 1 && motorNum <= SERVO_COUNT) {
      setServoAngle(motorNum - 1, angle);
      recordInput();
      server.send(200, "text/plain", "OK");
    } else if (servoIdx != -1) {
      setServoAngle(servoIdx, angle);
      recordInput();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid motor or angle");
    }
  }

  else {
    server.send(400, "text/plain", "Bad Args");
  }
}

void handleGetSettings(void) {
  char json[128];

  snprintf(json, sizeof(json),
           "{\"frameDelay\":%d,\"walkCycles\":%d,\"motorCurrentDelay\":%d,\"faceFps\":%d}",
           frameDelay, walkCycles, motorCurrentDelay, faceFps);
  sendJson(200, json);
}

void handleSetSettings(void) {
  if (server.hasArg("frameDelay"))        frameDelay        = constrain(getArgInt("frameDelay"), 10, 2000);
  if (server.hasArg("walkCycles"))        walkCycles        = constrain(getArgInt("walkCycles"), 1, 100);
  if (server.hasArg("motorCurrentDelay")) motorCurrentDelay = constrain(getArgInt("motorCurrentDelay"), 0, 500);
  if (server.hasArg("faceFps"))           faceFps           = constrain(getArgInt("faceFps"), 1, 30);
  server.send(200, "text/plain", "OK");
}

void handleGetStatus(void) {
  char json[256];
  size_t pos = 0;
  char ip[16];

  ipToText(WiFi.softAPIP(), ip, sizeof(ip));

  appendText(json, sizeof(json), &pos,
             "{\"currentCommand\":\"%s\",\"currentFace\":\"%s\",\"networkConnected\":%s,\"apIP\":\"%s\"",
             currentCommand, currentFaceName, networkConnected ? "true" : "false", ip);
  if (networkConnected) {
    ipToText(networkIP, ip, sizeof(ip));
    appendText(json, sizeof(json), &pos, ",\"networkIP\":\"%s\"", ip);
  }
  appendText(json, sizeof(json), &pos, "}");
  sendJson(200, json);
}

void handleApiCommand(void) {
  if (server.method() != HTTP_POST) {
    sendJson(405, "{\"error\":\"Method not allowed\"}");
    return;
  }

  char body[256];
  char command[24] = "";
  char face[24]    = "";
  getArg("plain", body, sizeof(body));
  Serial.printf("API Command received: %s\n", body);

  bool hasFace         = jsonGetString(body, "face", face, sizeof(face));
  bool hasCommandField = (strstr(body, "\"command\"") != NULL);

  if (!hasFace && !hasCommandField) {
    sendJson(400, "{\"error\":\"Missing command field\"}");
    return;
  }

  if (hasCommandField && !jsonGetString(body, "command", command, sizeof(command))) {
    sendJson(400, "{\"error\":\"Invalid command format\"}");
    return;
  }

  if (hasFace) {
    if (!hasCommandField) exitIdle();
    setFace(face);
  }
  recordInput();

  if (!hasCommandField) {
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Face updated\"}");
  } else if (strcmp(command, "stop") == 0) {
    clearCommand();
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Command stopped\"}");
  } else {
    setCommand(command);
    exitIdle();
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

void handleWifiScan(void) {

  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    sendJson(200, "{\"scanning\":true}");
    return;
  }
  if (n < 0) {
    if (wifiSetupState == WIFI_SETUP_IDLE) {
      if (WiFi.getMode() == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
        wifiRestoreApOnly = true;
      }
      WiFi.scanNetworks(true);
    }
    sendJson(200, "{\"scanning\":true}");
    return;
  }

  static char json[3072];
  size_t pos = 0;
  char ssid[200];

  appendText(json, sizeof(json), &pos, "[");
  for (int i = 0; i < n; i++) {
    if (sizeof(json) - pos < 260) break;
    jsonEscape(ssid, sizeof(ssid), WiFi.SSID(i).c_str());

    appendText(json, sizeof(json), &pos, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
               (i > 0) ? "," : "", ssid, (int)WiFi.RSSI(i),
               (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true");
  }
  appendText(json, sizeof(json), &pos, "]");
  WiFi.scanDelete();

  if (wifiRestoreApOnly && wifiSetupState == WIFI_SETUP_IDLE && WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
  }
  wifiRestoreApOnly = false;
  sendJson(200, json);
}

void handleWifiConnect(void) {
  if (server.method() != HTTP_POST) {
    sendJson(405, "{\"success\":false,\"error\":\"Method not allowed\"}");
    return;
  }
  if (server.arg("ssid").length() == 0) {
    sendJson(400, "{\"success\":false,\"error\":\"SSID required\"}");
    return;
  }
  if (wifiSetupState != WIFI_SETUP_IDLE) {
    sendJson(409, "{\"success\":false,\"error\":\"Connection attempt already in progress\"}");
    return;
  }

  getArg("ssid", wifiSetupSsid, sizeof(wifiSetupSsid));
  getArg("password", wifiSetupPass, sizeof(wifiSetupPass));
  wifiSetupError[0] = '\0';
  wifiSetupQueuedMs = millis();
  wifiSetupState    = WIFI_SETUP_QUEUED;
  wifiRestoreApOnly = false;
  sendJson(200, "{\"success\":true,\"pending\":true}");
}

void handleWifiStatus(void) {
  char json[384];
  char text[200];
  size_t pos = 0;
  bool connected = (WiFi.status() == WL_CONNECTED);

  appendText(json, sizeof(json), &pos, "{\"connected\":%s,\"connecting\":%s",
             connected ? "true" : "false",
             (wifiSetupState != WIFI_SETUP_IDLE) ? "true" : "false");

  if (wifiSetupError[0] != '\0') {
    jsonEscape(text, sizeof(text), wifiSetupError);
    appendText(json, sizeof(json), &pos, ",\"lastError\":\"%s\"", text);
  }
  if (connected) {
    char ip[16];
    jsonEscape(text, sizeof(text), WiFi.SSID().c_str());
    ipToText(WiFi.localIP(), ip, sizeof(ip));
    appendText(json, sizeof(json), &pos,
               ",\"ssid\":\"%s\",\"ip\":\"%s\",\"host\":\"%s.local\",\"mdns\":%s,\"rssi\":%d",
               text, ip, DEVICE_HOSTNAME, mdnsOk ? "true" : "false", (int)WiFi.RSSI());
  }
  appendText(json, sizeof(json), &pos, "}");
  sendJson(200, json);
}

void handleNotFound(void) {

  if (strncmp(server.uri().c_str(), "/api/", 5) == 0) {
    sendJson(404, "{\"error\":\"Not found\"}");
    return;
  }
  handleRoot();
}
