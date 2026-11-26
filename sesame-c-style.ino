#include <WiFi.h>         
#include <WebServer.h>         // вебсервер
#include <DNSServer.h>         // DNS: будь-яка адреса веде на робота (сайт відкривається сам)
#include <ESPmDNS.h>           // mDNS: адреса http://sesame-robot.local замість цифр IP
#include <Wire.h>              // шина I2C — по ній плата "розмовляє" з екраном (2 дроти SDA/SCL)
#include <ESP32Servo.h>        // керування сервоприводами (моторами ніг)
#include <Adafruit_GFX.h>   
#include <Adafruit_SSD1306.h>  // драйвер саме OLED-екрана 128x64 (чип SSD1306)

#include <string.h>    // strcmp, strncmp, strncpy, strlen, strstr, memset — робота з рядками
#include <strings.h>   // strcasecmp — порівняння рядків без різниці великих/малих літер
#include <stdio.h>     // snprintf, sscanf — "друк" у масив і читання чисел з тексту
#include <stdlib.h>    // atoi — перетворити текст "90" на число 90
#include <stdarg.h>    // va_list — для функції зі змінною кількістю аргументів (appendText)

// номери пінів, назву мережі тощо.
#include "config.h"             // налаштування: плата, Wi-Fi, швидкості, калібрування
#include "face-bitmaps.h"       // картинки облич (масиви байтів) + список FACE_LIST
#include "movement-sequences.h" // усі рухи робота + таблиця команд COMMANDS[]
#include "web-page.h"           // HTML-сторінка сайту (змінна index_html)

//1
DNSServer        dnsServer;     // об'єкт DNS-сервера; далі: dnsServer.start(), dnsServer.processNextRequest()
WebServer        server(80); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo            servos[SERVO_COUNT];  // масив із 8 моторів

// Номери пінів, до яких підключені мотори. SERVO_PINS береться з config.h
const int servoPins[SERVO_COUNT]  = SERVO_PINS;
// Поправки калібрування для кожного мотора, у градусах (-90..+90).
// int8_t — ціле число від -128 до 127, займає 1 байт (економимо пам'ять).
int8_t    servoSubtrim[SERVO_COUNT] = SERVO_SUBTRIM;

#define DNS_PORT 53   // 

// Налаштування швидкості 
int frameDelay        = DEFAULT_FRAME_DELAY;   // пауза між фазами ходьби, мс (за замовчуванням 100)
int walkCycles        = DEFAULT_WALK_CYCLES;   // скільки кроків робить ходьба за раз (10)
int motorCurrentDelay = DEFAULT_MOTOR_DELAY;   // пауза після кожного мотора, мс (20) — захист живлення
int faceFps           = DEFAULT_FACE_FPS;  // швидкість анімації обличчя за замовчуванням, кадрів/с
char currentCommand[24] = "";

// Обличчя 
char currentFaceName[24] = "default";                 // назва обличчя, яке зараз на екрані
// Вказівник на масив кадрів поточного обличчя. "const unsigned char* const*"
// читається так: вказівник на (незмінні вказівники на (незмінні байти картинки)).
// NULL — "ще нікуди не вказує".
const unsigned char* const* currentFaceFrames = NULL;
uint8_t       currentFaceFrameCount = 0;   // скільки кадрів у поточному обличчі (1..6); uint8_t = 0..255
uint8_t       currentFaceFrameIndex = 0;   // який кадр зараз показується (0 = перший)
unsigned long lastFaceFrameMs    = 0;   // коли (за millis()) показали останній кадр
FaceAnimMode  currentFaceMode  = FACE_ANIM_LOOP;  // режим анімації: LOOP / ONCE / BOOMERANG
int8_t        faceFrameDirection    = 1;   // для BOOMERANG: +1 = вперед по кадрах, -1 = назад
bool          faceAnimFinished    = false;  // для ONCE: true, коли дійшли до останнього кадру
int           currentFaceFps   = 0;   // швидкість саме цього обличчя (з таблиці faceFpsEntries)

//Очікування (робот кліпає очима
bool          idleActive           = false;  // true = робот у режимі очікування ("дихає")
bool          idleBlinkActive      = false;  // true = саме зараз іде анімація кліпання
unsigned long nextIdleBlinkMs      = 0;      // на якій мілісекунді кліпнути наступного разу
uint8_t       idleBlinkRepeatsLeft = 0;      // скільки ще разів кліпнути поспіль (для подвійного)

//Біжучий рядок з Wi-Fi 
unsigned long lastInputTime      = 0;      // коли була остання команда від людини
bool          firstInputReceived = false;  // чи була вже хоч одна команда після ввімкнення
bool          showingWifiInfo    = false;  // чи біжить зараз рядок на екрані
int           wifiScrollPos      = 0;      // на скільки пікселів текст уже зсунувся вліво
unsigned long lastWifiScrollMs   = 0;      // коли текст зсували востаннє
char          wifiInfoText[200]  = "";     // сам текст рядка (назва мережі, пароль, IP)

// Людина 3 Зв'язок
bool      networkConnected = false;  // чи підключений робот до домашнього роутера
IPAddress networkIP;                 // IP-адреса, яку роутер видав роботу (тип з бібліотеки WiFi)
bool      mdnsOk = false;            // чи запустився mDNS (адреса sesame-robot.local)
typedef enum {
  WIFI_SETUP_IDLE,       
  WIFI_SETUP_QUEUED,      // запит прийшов, чекаємо 300 мс, щоб сайт отримав відповідь
  WIFI_SETUP_CONNECTING   // підключаємось
} WifiSetupState;

WifiSetupState wifiSetupState    = WIFI_SETUP_IDLE;  // поточний етап підключення
char           wifiSetupSsid[33] = "";   // SSID максимум 32 символи (+1 місце для '\0')
char           wifiSetupPass[65] = "";   // пароль максимум 64 символи (+1 для '\0')
char           wifiSetupError[64] = "";  // "" = помилки не було
unsigned long  wifiSetupQueuedMs = 0;    // коли прийшов запит на підключення
unsigned long  wifiSetupStartMs  = 0;    // коли почали підключатися (для тайм-ауту)
bool           wifiRestoreApOnly = false; // після пошуку мереж повернути режим "тільки точка доступу"

#define WIFI_CONNECT_TIMEOUT_MS   15000   // максимум 15 секунд на підключення
#define WIFI_SETUP_START_DELAY_MS 300     // 300 мс паузи перед стартом підключення

//    2 

typedef struct {
  const char* name;                     // назв
  const unsigned char* const* frames;   // вказівник на масив кадрів цього обличчя
  uint8_t maxFrames;                    // максимум кадрів (6)
} FaceEntry;

#define MAX_FACE_FRAMES 6   // у кожного обличчя може бути до 6 кадрів анімації
#define MAKE_FACE_FRAMES(name) \
  const unsigned char* const face_##name##_frames[] = { \
    epd_bitmap_##name, epd_bitmap_##name##_1, epd_bitmap_##name##_2, \
    epd_bitmap_##name##_3, epd_bitmap_##name##_4, epd_bitmap_##name##_5 \
  };
#define X(name) MAKE_FACE_FRAMES(name)   // кажемо: кожен X(...) у списку = MAKE_FACE_FRAMES(...)
FACE_LIST                                // розгортаємо список: X(walk) X(rest) X(happy) 
#undef X                                 // "забуваємо" X, щоб нижче дати йому інше значення
#undef MAKE_FACE_FRAMES                  // шаблон більше не потрібен

const FaceEntry faceEntries[] = {
#define X(name) { #name, face_##name##_frames, MAX_FACE_FRAMES },
  FACE_LIST
#undef X
  // останній рядок — запасне обличчя "default" (у оригіналі назва масиву
  // з опискою "defualt", тому залишаємо так, як в face-bitmaps.h)
  { "default", face_defualt_frames, MAX_FACE_FRAMES }
};
// Кількість облич рахується сама: розмір усієї таблиці / розмір одного рядка.
#define FACE_COUNT (sizeof(faceEntries) / sizeof(faceEntries[0]))

// Швидкість анімації для конкретних облич (кадрів за секунду)
typedef struct {
  const char* name;   // назва обличчя
  uint8_t fps;        // кадрів за секунду для нього
} FaceFpsEntry;

// Більшість облич — 1 кадр/с (повільно). "point" — 5, кліпання "idle_blink" — 7,
// щоб кліпання було швидким і природним.
const FaceFpsEntry faceFpsEntries[] = {
  { "walk", 1 }, { "rest", 1 }, { "swim", 1 }, { "dance", 1 }, { "wave", 1 },
  { "point", 5 }, { "stand", 1 }, { "cute", 1 }, { "pushup", 1 }, { "freaky", 1 },
  { "bow", 1 }, { "worm", 1 }, { "shake", 1 }, { "shrug", 1 }, { "dead", 2 },
  { "crab", 1 }, { "idle", 1 }, { "idle_blink", 7 }, { "default", 1 },
  // обличчя для "розмови"
  { "happy", 1 }, { "talk_happy", 1 }, { "sad", 1 }, { "talk_sad", 1 },
  { "angry", 1 }, { "talk_angry", 1 }, { "surprised", 1 }, { "talk_surprised", 1 },
  { "sleepy", 1 }, { "talk_sleepy", 1 }, { "love", 1 }, { "talk_love", 1 },
  { "excited", 1 }, { "talk_excited", 1 }, { "confused", 1 }, { "talk_confused", 1 },
  { "thinking", 1 }, { "talk_thinking", 1 },
};
#define FACE_FPS_COUNT (sizeof(faceFpsEntries) / sizeof(faceFpsEntries[0]))  // скільки рядків у таблиці

// прототипи — це оголошення функцій наперед. Формат прототипу: ТИП_РЕЗУЛЬТАТУ назва(параметри);  — без тіла,
//  bool = повертає true/false.
// const char* = вказівник на рядок, який функція не змінює.
// size_t = беззнакове ціле для розмірів масивів.
void copyText(char* dst, size_t size, const char* src);
void appendText(char* buf, size_t size, size_t* pos, const char* fmt, ...);
void jsonEscape(char* out, size_t size, const char* in);
bool jsonGetString(const char* json, const char* key, char* out, size_t size);
void ipToText(IPAddress ip, char* out, size_t size);
void getArg(const char* name, char* out, size_t size);
int  getArgInt(const char* name);
// команди
void setCommand(const char* cmd);
void clearCommand(void);
bool isCommand(const char* cmd);
void runCommandFromLoop(void);
void handleSerial(void);
void printHelp(void);
// обличчя
void updateFaceBitmap(const unsigned char* bitmap);
uint8_t countFrames(const unsigned char* const* frames, uint8_t maxFrames);
int  getFaceFpsForName(const char* faceName);
void updateAnimatedFace(void);
void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs);
void exitIdle(void);
void updateIdleBlink(void);
void recordInput(void);
void updateWifiInfoScroll(void);
// Wi-Fi
bool startMdns(void);
void announceNetwork(const char* ssid);
void setApOnlyInfoText(void);
void showWifiInfoNow(void);
bool connectToWifi(const char* ssid, const char* pass, uint32_t timeoutMs);
void finishWifiSetup(const char* err);
void updateWifiSetup(void);
// сайт
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
//3                                          Безпечне копіювання рядка: ніколи не вийде за межі масиву dst
// dst  — куди копіюємо, size — розмір масиву dst, src — що копіюємо.
void copyText(char* dst, size_t size, const char* src) {
  if (size == 0) return;           // масив нульового розміру — нікуди писати, виходимо
  if (src == NULL) src = "";       // якщо замість рядка прийшов NULL — вважаємо його порожнім
  strncpy(dst, src, size - 1);     // копіюємо максимум size-1 символів (останнє місце — для '\0')
  dst[size - 1] = '\0';            // гарантовано ставимо кінець рядка в останню клітинку
}

// 
// Дописати текст у кінець буфера (як printf, але в масив).
// pos — де зараз кінець тексту, функція сама його зсуває.
void appendText(char* buf, size_t size, size_t* pos, const char* fmt, ...) {
  if (*pos >= size - 1) return;           // буфер уже повний
  va_list args;                           // "список" додаткових аргументів після fmt
  va_start(args, fmt);                    // починаємо читати аргументи, що йдуть після fmt
  // Друкуємо в буфер, починаючи з позиції *pos; size - *pos — скільки місця залишилось.
  // vsnprintf повертає, скільки символів ХОТІВ написати.
  int written = vsnprintf(buf + *pos, size - *pos, fmt, args);
  va_end(args);                           // закінчили роботу з аргументами (обов'язкова пара до va_start)
  if (written < 0) return;                // від'ємне число = помилка форматування
  *pos += (size_t)written;                // зсуваємо кінець тексту на кількість написаних символів
  if (*pos > size - 1) *pos = size - 1;   // текст обрізався
}

// Підготувати текст для JSON: " -> \"   \ -> \\   керуючі символи -> \u00XX
// Потрібно, бо назва Wi-Fi може містити лапки, які зламали б JSON.
void jsonEscape(char* out, size_t size, const char* in) {
  size_t o = 0;                                     // позиція запису в out
  // Йдемо по вхідному рядку до '\0'. Умова o + 7 < size залишає запас
  // на найдовшу заміну (\u00XX = 6 символів) + '\0'.
  for (; *in != '\0' && o + 7 < size; in++) {
    unsigned char c = (unsigned char)*in;          // поточний символ як число 0..255
    if (c == '\\' || c == '"') {                    // зворотний слеш або лапка?
      out[o++] = '\\';                              //   ставимо перед ним зворотний слеш
      out[o++] = (char)c;                           //   і сам символ
    } else if (c < 0x20) {                          // керуючий символ (Enter, Tab...)?
      o += snprintf(out + o, size - o, "\\u%04x", c);  //   записуємо як \u000a тощо
    } else {
      out[o++] = (char)c;                           // звичайний символ — копіюємо як є
    }
  }
  out[o] = '\0';                                    // закриваємо рядок
}


// Дуже простий "парсер" JSON: знаходить "key": "значення" і копіює значення в out.
// Повертає true, якщо знайшов.
// Приклад: json = {"command":"wave"}, key = "command"  ->  out = "wave"
bool jsonGetString(const char* json, const char* key, char* out, size_t size) {
  char pattern[40];                                   // сюди складемо шаблон пошуку
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);  // key у лапках: "command"

  const char* p = strstr(json, pattern);   // strstr шукає шаблон у тексті, повертає вказівник на знайдене
  if (p == NULL) return false;             // не знайшли ключ — виходимо
  p += strlen(pattern);                    // перестрибуємо через сам ключ, p тепер одразу після нього

  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;  // пропускаємо пробіли
  if (*p != ':') return false;             // після ключа має бути двокрапка
  p++;                                     // перестрибуємо двокрапку
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;  // знову пропускаємо пробіли
  if (*p != '"') return false;             // значення має починатися з лапки (тобто бути текстом)
  p++;                                     // перестрибуємо відкриваючу лапку

  size_t i = 0;                            // скільки символів уже скопіювали
  // Копіюємо символи, поки не зустрінемо закриваючу лапку, кінець тексту
  // або поки не заповнимо out (залишаючи місце для '\0').
  while (*p != '\0' && *p != '"' && i < size - 1) {
    out[i++] = *p++;                       // скопіювали символ і зсунули обидва "курсори"
  }
  out[i] = '\0';                           // закриваємо рядок
  return (*p == '"');                      // true тільки якщо дійшли саме до закриваючої лапки
}

// IP-адреса -> "192.168.4.1"
// IPAddress поводиться як масив із 4 чисел: ip[0]..ip[3]. %u — беззнакове число.
void ipToText(IPAddress ip, char* out, size_t size) {
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// ---- ЗАХИСТ [Людина 3 · Зв'язок] --------------------------------
// Що сказати: це єдиний "міст" між C++ і C. Бібліотека WebServer
// повертає String, .c_str() дає звичайний char*, і ми одразу копіюємо
// його в наш масив. Далі в логіці String не з'являється.
// ------------------------------------------------------------------
// Взяти параметр запиту з сайту (наприклад ?pose=wave) у звичайний масив.
// Бібліотека WebServer повертає String, тому перетворюємо його тут, в одному місці.
void getArg(const char* name, char* out, size_t size) {
  // server.arg("pose") -> String "wave";  .c_str() -> звичайний char* "wave"
  copyText(out, size, server.arg(name).c_str());
}

// Те саме, але одразу число: ?value=90 -> 90. atoi = "ASCII to integer".
int getArgInt(const char* name) {
  return atoi(server.arg(name).c_str());
}

// Відправити відповідь у форматі JSON з кодом (200 = OK, 400 = помилка...).
void sendJson(int code, const char* json) {
  server.send(code, "application/json", json);   // "application/json" — тип вмісту, щоб браузер знав, що це JSON
}










// 1 
void setCommand(const char* cmd) { copyText(currentCommand, sizeof(currentCommand), cmd); }  // покласти слово в скриньку
void clearCommand(void)          { currentCommand[0] = '\0'; }                               // очистити: перший символ = кінець рядка
bool isCommand(const char* cmd)  { return strcmp(currentCommand, cmd) == 0; }                // чи лежить у скриньці саме це слово

// 1
//  це серце системи. Кроки:
//   1) скринька порожня - одразу виходимо;
//   2) шукаємо назву в таблиці COMMANDS (movement-sequences.h);
//   3) викликаємо функцію руху через вказівник: cmd->run();
//   4) якщо це поза, а не ходьба, стираємо команду.
// Це ТОЧКА СТИКУ 2: тут Ядро передає роботу Тілу.
void runCommandFromLoop(void) {
  if (currentCommand[0] == '\0') return;   // скринька порожня — нічого робити, одразу виходимо

  // Шукаємо слово в таблиці COMMANDS. Отримуємо вказівник на рядок таблиці
  // (назва, код, функція, прапорець) або NULL, якщо такої команди немає.
  const RobotCommand* cmd = findCommandByName(currentCommand);
  if (cmd == NULL) {                                      // невідома команда (наприклад, помилка в назві)
    Serial.printf("Unknown command: %s\n", currentCommand);  // пишемо в Serial Monitor; %s — підставити рядок
    clearCommand();                                       // стираємо, щоб не намагатись виконати знову
    return;
  }

  cmd->run();   // ВИКЛИК РУХУ через вказівник на функцію (для "wave" це runWavePose())

  // Пози виконуються один раз. Ходьба повторюється, поки кнопку тримають.
  // Якщо під час руху прийшла інша команда, не стираємо її.
  // !cmd->repeatWhileHeld — це поза (false у таблиці), а не ходьба.
  // isCommand(cmd->name)  — у скриньці досі та сама команда, нової не прийшло.
  if (!cmd->repeatWhileHeld && isCommand(cmd->name)) {
    clearCommand();
  }
}

//1
// #  8. SETUP() — ЗАПУСК ПРИ ВВІМКНЕННІ 
void setup() {
  Serial.begin(115200);    //  на швидкості 115200 біт/с
  randomSeed(micros());    // "зерно" для random(): micros() щоразу різний, тож кліпання буде різним

  // Екран
  Wire.begin(I2C_SDA, I2C_SCL);   // запускаємо шину I2C на пінах з config.h 
  // display.begin(...) вмикає екран. SSD1306_SWITCHCAPVCC — живлення екрана від
  // його власного перетворювача, OLED_I2C_ADDR  — адреса екрана на шині.
  // Повертає false, якщо екран не відповів. "!" — заперечення: "якщо НЕ вдалося".
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("OLED not found! Check wiring (SDA/SCL) and board in config.h");
    while (1) delay(1000);        // безкінечний цикл: зупиняємо програму назавжди
  }
  display.clearDisplay();               // очистити буфер екрана (все чорне)
  display.setTextColor(SSD1306_WHITE);  // колір тексту — білий (екран монохромний)
  display.setTextSize(1);               // розмір шрифту 1 (найменший, 6x8 пікселів на символ)
  display.setCursor(0, 0);              // курсор у лівий верхній кут
  display.println("Setting up WiFi..."); // написати текст у буфер
  display.display();                    // ПОКАЗАТИ буфер на екрані (без цього нічого не видно)

  // Wi-Fi: не записуємо паролі у флеш-пам'ять
  WiFi.persistent(false);   // не зберігати налаштування вайфай у флеш (менше зносу пам'яті)

  // Якщо в config.h увімкнено домашній Wi-Fi і вказано назву мережі —
  // пробуємо підключитися (максимум 10 000 мс = 10 секунд).
  // strlen(...) > 0 — назва мережі не порожня.
  if (ENABLE_NETWORK_MODE && strlen(NETWORK_SSID) > 0) {
    if (!connectToWifi(NETWORK_SSID, NETWORK_PASS, 10000)) {
      Serial.println("Failed to connect to network. Running in AP-only mode.");
      WiFi.mode(WIFI_AP);   // не вийшло — режим "тільки своя точка доступу"
    }
  } else {
    WiFi.mode(WIFI_AP);     // домашній Wi-Fi вимкнено — тільки своя точка доступу
    Serial.println("Network mode disabled. Running in AP-only mode.");
  }

  // Точка доступу робота
  WiFi.softAP(AP_SSID, AP_PASS);       // створюємо мережу "Sesame-Controller" з паролем з config.h
  IPAddress apIP = WiFi.softAPIP();    // дізнаємось IP робота в цій мережі (зазвичай 192.168.4.1)
  char apIpText[16];                   // масив для тексту IP: "255.255.255.255" = 15 символів + '\0'
  ipToText(apIP, apIpText, sizeof(apIpText));             // IP -> текст
  Serial.printf("AP created: %s  IP: %s\n", AP_SSID, apIpText);  // повідомляємо в Serial

  // Готуємо текст для біжучого рядка на екрані.
  if (networkConnected) {
    announceNetwork(NETWORK_SSID);   // підключились до роутера — показуємо обидві адреси
  } else {
    setApOnlyInfoText();             // тільки своя мережа — показуємо назву, пароль і IP
    startMdns();                     // запускаємо адресу sesame-robot.local
  }

  lastInputTime      = millis();   // millis() — мілісекунди від увімкнення; запам'ятали "зараз"
  firstInputReceived = false;      // команд ще не було
  showingWifiInfo    = false;      // біжучий рядок поки не показуємо

  // DNS: будь-яка адреса веде на робота (тому сайт відкривається сам)
  // "*" — будь-яке ім'я сайту, apIP — куди його направляти (на робота).
  dnsServer.start(DNS_PORT, "*", apIP);

  // Адреси сайту -> функції, які їх обробляють
  // 3
  server.on("/",                 handleRoot);          // головна сторінка -> віддати HTML сайту
  server.on("/cmd",              handleCommandWeb);    // кнопки руху, поз, стоп, мотори
  server.on("/getSettings",      handleGetSettings);   // прочитати налаштування швидкості
  server.on("/setSettings",      handleSetSettings);   // змінити налаштування швидкості
  server.on("/api/status",       handleGetStatus);     // стан робота (для крапки "онлайн")
  server.on("/api/command",      handleApiCommand);    // JSON-команди (обличчя, програми)
  server.on("/api/wifi/scan",    handleWifiScan);      // пошук Wi-Fi мереж
  server.on("/api/wifi/connect", handleWifiConnect);   // підключитися до вибраної мережі
  server.on("/api/wifi/status",  handleWifiStatus);    // стан підключення до роутера
  server.onNotFound(handleNotFound);                   // будь-яка інша адреса
  server.begin();                                      // запустити вебсервер

  // Мотори
  // 2
  ESP32PWM::allocateTimer(0);   // резервуємо апаратний таймер 0 для імпульсів моторів
  ESP32PWM::allocateTimer(1);   // таймер 1
  ESP32PWM::allocateTimer(2);   // таймер 2
  ESP32PWM::allocateTimer(3);   // таймер 3 (8 моторів ділять 4 таймери)
  for (int i = 0; i < SERVO_COUNT; i++) {        // для кожного з 8 моторів (i = 0..7):
    servos[i].setPeriodHertz(50);                //   імпульс 50 разів на секунду (стандарт для серво)
    servos[i].attach(servoPins[i], 732, 2929);  // 0..180 градусів
    //   attach(пін, мін. імпульс мкс, макс. імпульс мкс): 732 мкс = 0°, 2929 мкс = 180°
  }
  delay(10);   // коротка пауза 10 мс, щоб мотори "прокинулись"

  setFace("rest");   // показати обличчя, але мотори не рухати
  Serial.println("HTTP server & Captive Portal started. Type 'help' in Serial Monitor.");
}

// 1
void loop() {
  dnsServer.processNextRequest();   // відповісти на один DNS-запит, якщо він є
  server.handleClient();            // обробити один запит від браузера (натискання кнопки)
  updateWifiSetup();                // один крок підключення до домашнього Wi-Fi (машина станів)
  updateAnimatedFace();             // якщо настав час — показати наступний кадр обличчя
  updateIdleBlink();                // якщо робот чекає і настав час — кліпнути
  updateWifiInfoScroll();           // зсунути біжучий рядок з адресою (якщо він показується)

  runCommandFromLoop();             // якщо в скриньці є команда — виконати рух

  if (Serial.available()) handleSerial();   // якщо з комп'ютера прийшов символ — обробити його
}

// 1
//  SERIAL MONITOR — команди для налагодження (115200 бод)
// 
// Друкує список усіх команд у Serial Monitor (команда "help").
void printHelp(void) {
  Serial.println("--- Commands ---");
  Serial.println("rn <code>         run move, codes:");
  // Список рухів не написаний вручну, а береться з таблиці COMMANDS:
  // додали рух у таблицю — він сам з'явиться в help.
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    Serial.printf("   %s = %s\n", COMMANDS[i].shortCode, COMMANDS[i].name);  // наприклад "   wv = wave"
  }
  Serial.println("fc <name>         set face (happy, sad, love...)");
  Serial.println("<motor> <angle>   one motor, e.g. '0 90'");
  Serial.println("all <angle>       all motors");
  Serial.println("st                show subtrim");
  Serial.println("st <motor> <val>  set subtrim (-90..90)");
  Serial.println("st save           print line for config.h");
  Serial.println("st reset          subtrim to 0");
}

// 1
void handleSerial(void) {
  static char buffer[32];      // сюди збираємо рядок; static — не стирається між викликами
  static uint8_t pos = 0;      // скільки символів уже в буфері

  char c = (char)Serial.read();     // прочитати ОДИН символ, що прийшов з комп'ютера
  if (c != '\n' && c != '\r') {     // це не Enter (\n — новий рядок, \r — повернення каретки)?
    if (pos < sizeof(buffer) - 1) buffer[pos++] = c;   // є місце — додаємо символ у буфер
    return;                         // чекаємо наступний символ
  }
  if (pos == 0) return;      // порожній рядок
  buffer[pos] = '\0';        // закриваємо рядок
  pos = 0;                   // наступний рядок почнемо збирати з початку
  recordInput();             // запам'ятати, що людина щось зробила (вимикає біжучий рядок)

  int motor, angle;          // змінні для чисел, які витягнемо з команди

  // старі довгі назви залишені для сумісності
  // Якщо написали "run walk" — підміняємо на сучасну "rn wf" і обробляємо далі.
  if (strcmp(buffer, "run walk") == 0)  copyText(buffer, sizeof(buffer), "rn wf");
  if (strcmp(buffer, "run rest") == 0)  copyText(buffer, sizeof(buffer), "rn rs");
  if (strcmp(buffer, "run stand") == 0) copyText(buffer, sizeof(buffer), "rn st");

  // "help" або "?" — показати список команд 
  if (strcmp(buffer, "help") == 0 || strcmp(buffer, "?") == 0) {
    printHelp();
  }
  // "rn wv" — виконати рух за коротким кодом 
  // strncmp(buffer, "rn ", 3) порівнює тільки перші 3 символи.
  else if (strncmp(buffer, "rn ", 3) == 0) {
    const RobotCommand* cmd = findCommandByCode(buffer + 3);   // buffer + 3 = текст після "rn "
    if (cmd == NULL) {
      Serial.println("Unknown code. Type 'help'.");            // такого коду немає в таблиці
    } else {
      // з Serial кожен рух виконується рівно один раз
      setCommand(cmd->name);    // кладемо назву в скриньку (потрібно для pressingCheck у ходьбі)
      exitIdle();               // виходимо з режиму очікування (перестати кліпати)
      cmd->run();               // виконуємо рух через вказівник на функцію
      if (isCommand(cmd->name)) clearCommand();   // прибираємо за собою, якщо нової команди не прийшло
    }
  }
  // "fc happy" або "face happy" — змінити обличчя
  else if (strncmp(buffer, "face ", 5) == 0 || strncmp(buffer, "fc ", 3) == 0) {
    // Тернарний оператор: (умова) ? якщо_так : якщо_ні.
    // Якщо другий символ 'c' — це "fc ", назва з 4-го символу; інакше "face ", з 6-го.
    const char* faceName = (buffer[1] == 'c') ? buffer + 3 : buffer + 5;
    clearCommand();               // зупинити рух, якщо був
    exitIdle();                   // вимкнути кліпання, щоб воно не перебило обличчя
    setFace(faceName);            // показати обличчя
    Serial.printf("Face set to %s\n", currentFaceName);
  }
  // --- "st" — показати поточне калібрування ---
  else if (strcmp(buffer, "subtrim") == 0 || strcmp(buffer, "st") == 0) {
    Serial.println("Subtrim values:");
    for (int i = 0; i < SERVO_COUNT; i++) {
      // %d — ціле число, %+d — ціле зі знаком (+5 / -3)
      Serial.printf("Motor %d (%s): %+d\n", i, SERVO_NAMES[i], servoSubtrim[i]);
    }
  }
  // --- "st save" — надрукувати рядок для вставки в config.h ---
  else if (strcmp(buffer, "subtrim save") == 0 || strcmp(buffer, "st save") == 0) {
    Serial.println("Copy this line into config.h:");
    Serial.print("#define SERVO_SUBTRIM { ");
    for (int i = 0; i < SERVO_COUNT; i++) {
      Serial.print(servoSubtrim[i]);
      if (i < SERVO_COUNT - 1) Serial.print(", ");   // кома після всіх, крім останнього
    }
    Serial.println(" }");
  }
  // --- "st reset" — обнулити калібрування ---
  else if (strcmp(buffer, "subtrim reset") == 0 || strcmp(buffer, "st reset") == 0) {
    for (int i = 0; i < SERVO_COUNT; i++) servoSubtrim[i] = 0;
    Serial.println("All subtrim values reset to 0");
  }
  // --- "st 3 5" — поправка +5° для мотора 3 ---
  // Ця перевірка стоїть ПІСЛЯ "st save" і "st reset", інакше вони б сюди потрапляли.
  else if (strncmp(buffer, "subtrim ", 8) == 0 || strncmp(buffer, "st ", 3) == 0) {
    const char* params = (buffer[1] == 't') ? buffer + 3 : buffer + 8;   // текст після "st " або "subtrim "
    int value;
    // sscanf читає з тексту два цілих числа; повертає, скільки вдалося прочитати.
    if (sscanf(params, "%d %d", &motor, &value) == 2) {
      if (motor < 0 || motor >= SERVO_COUNT)  Serial.println("Invalid motor number (0-7)");
      else if (value < -90 || value > 90)     Serial.println("Subtrim must be -90..+90");
      else {
        servoSubtrim[motor] = (int8_t)value;   // (int8_t) — явне перетворення до типу 1 байт
        Serial.printf("Motor %d subtrim set to %+d\n", motor, value);
      }
    }
  }
  // --- "all 90" — усі мотори на один кут ---
  else if (strncmp(buffer, "all ", 4) == 0) {
    if (sscanf(buffer + 4, "%d", &angle) == 1) {   // одне число після "all "
      for (int i = 0; i < SERVO_COUNT; i++) setServoAngle(i, angle);
      Serial.printf("All servos set to %d\n", angle);
    }
  }
  // --- "3 90" — один мотор на кут ---
  // &motor — адреса змінної: sscanf записує прочитане число прямо в неї.
  else if (sscanf(buffer, "%d %d", &motor, &angle) == 2) {
    if (motor >= 0 && motor < SERVO_COUNT) {
      setServoAngle(motor, angle);
      Serial.printf("Servo %d set to %d\n", motor, angle);
    } else {
      Serial.println("Invalid motor number (0-7)");
    }
  }
  // --- нічого не підійшло ---
  else {
    Serial.println("Unknown command. Type 'help'.");
  }
}

// 222222
void setServoAngle(uint8_t channel, int angle) {
  if (channel >= SERVO_COUNT) return;     // захист: мотора з таким номером немає
  // constrain(x, мін, макс) — обрізає значення до меж: -5 -> 0, 190 -> 180.
  int adjusted = constrain(angle + servoSubtrim[channel], 0, 180);
  servos[channel].write(adjusted);        // команда мотору повернутись на кут (C++-виклик бібліотеки)
  delayWithFace(motorCurrentDelay);   // не вмикати всі мотори одночасно
}
bool pressingCheck(const char* cmd, int ms) {
  unsigned long start = millis();                    // запам'ятали момент початку
  while (millis() - start < (unsigned long)ms) {    // поки не минуло ms мілісекунд:
    server.handleClient();                           //   приймаємо запити (зокрема "stop")
    dnsServer.processNextRequest();                  //   відповідаємо на DNS
    updateAnimatedFace();                            //   анімуємо обличчя
    if (!isCommand(cmd)) {                           //   команда в скриньці змінилась?
      runStandPose(1);                               //     стати рівно і перейти в очікування
      return false;                                  //     сказати ходьбі "зупинись"
    }
    yield();                                         //   дати час системним задачам ESP32 (Wi-Fi)
  }
  return true;                                       // кнопку досі тримають — ходьба продовжується
}
void delayWithFace(unsigned long ms) {
  unsigned long start = millis();        // момент початку паузи
  while (millis() - start < ms) {        // поки не минуло ms мілісекунд:
    updateAnimatedFace();                //   обличчя живе
    server.handleClient();               //   сайт відповідає (саме тому "стоп" працює)
    dnsServer.processNextRequest();      //   DNS відповідає
    delay(5);                            //   маленька пауза 5 мс, щоб не "крутити" процесор даремно
  }
}

// облича
void updateFaceBitmap(const unsigned char* bitmap) {
  display.clearDisplay();                                     // очистити буфер
  display.drawBitmap(0, 0, bitmap, 128, 64, SSD1306_WHITE);  // картинка з точки (0,0), розмір 128x64, білим
  display.display();                                          // показати на екрані
}

// Скільки кадрів реально існує у face_xxx_frames
// Кадри, яких немає у face-bitmaps.h, мають адресу NULL (через weak).
uint8_t countFrames(const unsigned char* const* frames, uint8_t maxFrames) {
  if (frames == NULL || frames[0] == NULL) return 0;   // немає навіть першого кадру
  uint8_t count = 0;
  for (uint8_t i = 0; i < maxFrames; i++) {   // перебираємо до 6 кадрів
    if (frames[i] == NULL) break;             // перший відсутній кадр — далі не дивимось
    count++;                                  // кадр є — рахуємо
  }
  return count;
}

// Знайти в таблиці faceFpsEntries швидкість для обличчя з такою назвою.
int getFaceFpsForName(const char* faceName) {
  for (size_t i = 0; i < FACE_FPS_COUNT; i++) {
    // strcasecmp — як strcmp, але "Happy" і "happy" вважає однаковими
    if (strcasecmp(faceName, faceFpsEntries[i].name) == 0) return faceFpsEntries[i].fps;
  }
  return faceFps;   // не знайшли — загальна швидкість з налаштувань
}
void setFace(const char* faceName) {
  if (faceName == NULL || faceName[0] == '\0') return;   // порожня назва — нічого не робимо
  // Це обличчя вже на екрані — не перемальовуємо (&& — "і": обидві умови мають бути true)
  if (strcmp(faceName, currentFaceName) == 0 && currentFaceFrames != NULL) return;

  copyText(currentFaceName, sizeof(currentFaceName), faceName);   // запам'ятали нову назву
  currentFaceFrameIndex = 0;                         // почнемо з першого кадру
  lastFaceFrameMs       = 0;                         // таймер кадрів з нуля
  faceFrameDirection    = 1;                         // напрямок "вперед"
  faceAnimFinished      = false;                     // анімація ще не закінчилась
  currentFaceFps        = getFaceFpsForName(faceName);   // швидкість для цього обличчя

  // спочатку "default", потім шукаємо потрібне
  currentFaceFrames     = face_defualt_frames;
  currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);

  // Шукаємо назву в таблиці всіх облич.
  for (size_t i = 0; i < FACE_COUNT; i++) {
    if (strcasecmp(faceName, faceEntries[i].name) == 0) {        // знайшли
      currentFaceFrames     = faceEntries[i].frames;              // беремо його кадри
      currentFaceFrameCount = countFrames(faceEntries[i].frames, faceEntries[i].maxFrames);
      break;                                                      // далі не шукаємо
    }
  }

  // такого обличчя немає  показуємо default
  if (currentFaceFrameCount == 0) {
    currentFaceFrames     = face_defualt_frames;
    currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);
    copyText(currentFaceName, sizeof(currentFaceName), "default");
    currentFaceFps = getFaceFpsForName("default");
  }

  if (currentFaceFrameCount > 0) updateFaceBitmap(currentFaceFrames[0]);   // малюємо перший кадр
}

// Змінити режим анімації (LOOP / ONCE / BOOMERANG) і скинути його стан.
void setFaceMode(FaceAnimMode mode) {
  currentFaceMode    = mode;
  faceFrameDirection = 1;
  faceAnimFinished   = false;
}

// Зручна функція "обличчя + режим" одним викликом — її використовують усі рухи.
// Спочатку режим, потім обличчя (setFace може не перемальовувати те саме обличчя).
void setFaceWithMode(const char* faceName, FaceAnimMode mode) {
  setFaceMode(mode);
  setFace(faceName);
}

//2
void updateAnimatedFace(void) {
  if (currentFaceFrames == NULL || currentFaceFrameCount <= 1) return;   // 0 або 1 кадр — нема що анімувати
  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished) return;     // ONCE вже дограв — стоїмо

  unsigned long now = millis();                                    // поточний час
  int fps = (currentFaceFps > 0) ? currentFaceFps : faceFps;       // швидкість обличчя або загальна
  if (fps < 1) fps = 1;                                            // захист від ділення на нуль
  // 1000UL / fps — скільки мілісекунд показувати один кадр (UL = unsigned long).
  if (now - lastFaceFrameMs < 1000UL / (unsigned long)fps) return; // ще не час — виходимо
  lastFaceFrameMs = now;                                           // запам'ятали момент зміни кадру

  uint8_t last = currentFaceFrameCount - 1;   // номер останнього кадру (кадри нумеруються з 0)

  if (currentFaceMode == FACE_ANIM_LOOP) {
    // По колу: 0,1,2,0,1,2... Остача від ділення повертає на 0 після останнього.
    currentFaceFrameIndex = (currentFaceFrameIndex + 1) % currentFaceFrameCount;
  }
  else if (currentFaceMode == FACE_ANIM_ONCE) {
    // Один раз: 0,1,2 і стоп на останньому.
    if (currentFaceFrameIndex >= last) {
      currentFaceFrameIndex = last;
      faceAnimFinished = true;               // позначаємо, що анімація закінчилась
    } else {
      currentFaceFrameIndex++;               // ++ — збільшити на 1
    }
  }
  else {  // FACE_ANIM_BOOMERANG
    // Туди-назад: 0,1,2,1,0,1,2...
    if (faceFrameDirection > 0) {                 // йдемо вперед
      if (currentFaceFrameIndex >= last) {        //   дійшли до кінця —
        faceFrameDirection = -1;                  //   розвертаємось
        if (currentFaceFrameIndex > 0) currentFaceFrameIndex--;   // і крок назад (-- — зменшити на 1)
      } else {
        currentFaceFrameIndex++;                  //   інакше крок вперед
      }
    } else {                                      // йдемо назад
      if (currentFaceFrameIndex == 0) {           //   дійшли до початку —
        faceFrameDirection = 1;                   //   розвертаємось
        currentFaceFrameIndex++;                  //   і крок вперед
      } else {
        currentFaceFrameIndex--;                  //   інакше крок назад
      }
    }
  }
  updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);   // малюємо вибраний кадр
}

//  РЕЖИМ ОЧІКУВАННЯ (кліпання)
// 
void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs) {
  nextIdleBlinkMs = millis() + (unsigned long)random(minMs, maxMs);
}

// 
// Увійти в режим очікування. Викликається з runStandPose(1) після кожного руху.
void enterIdle(void) {
  idleActive           = true;                     // режим очікування увімкнено
  idleBlinkActive      = false;                    // зараз не кліпаємо
  idleBlinkRepeatsLeft = 0;                        // повторів немає
  setFaceWithMode("idle", FACE_ANIM_BOOMERANG);   // обличчя "дихає" туди-назад
  scheduleNextIdleBlink(3000, 7000);               // перше кліпання через 3–7 секунд
}

// Вийти з режиму очікування (коли прийшла команда).
void exitIdle(void) {
  idleActive      = false;
  idleBlinkActive = false;
}

// Викликається з loop(): керує кліпанням.
void updateIdleBlink(void) {
  if (!idleActive) return;     // робот не в очікуванні — нічого не робимо

  if (!idleBlinkActive) {                        // зараз не кліпаємо:
    if (millis() >= nextIdleBlinkMs) {           //   настав час кліпнути?
      idleBlinkActive = true;                    //     починаємо кліпання
      // random(0, 100) < 30 — з імовірністю 30% кліпнемо двічі
      if (idleBlinkRepeatsLeft == 0 && random(0, 100) < 30) {
        idleBlinkRepeatsLeft = 1;   // іноді кліпає двічі
      }
      setFaceWithMode("idle_blink", FACE_ANIM_ONCE);   // анімація кліпання, один раз
    }
    return;
  }

  // Зараз кліпаємо. Чекаємо, поки анімація ONCE дограє до кінця.
  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished) {
    idleBlinkActive = false;                         // кліпання закінчилось
    setFaceWithMode("idle", FACE_ANIM_BOOMERANG);   // повертаємо "дихання"
    if (idleBlinkRepeatsLeft > 0) {                  // треба кліпнути ще раз?
      idleBlinkRepeatsLeft--;
      scheduleNextIdleBlink(120, 220);               //   майже одразу, через 0,12–0,22 с
    } else {
      scheduleNextIdleBlink(3000, 7000);             //   інакше через 3–7 секунд
    }
  }
}

//2
//  БІЖУЧИЙ РЯДОК З АДРЕСОЮ (показується, поки ніхто не керує роботом)
// Викликається при кожній команді від людини.
void recordInput(void) {
  lastInputTime = millis();           // запам'ятали час останньої дії
  if (!firstInputReceived) {          // це перша команда після ввімкнення?
    firstInputReceived = true;        //   так — більше не показуємо підказку
    showingWifiInfo    = false;
  }
}

// 
// назва мережі і пароль, щоб нова людина знала, як підключитись.
// Кожні 150 мс текст зсувається на 2 пікселі (курсор з від'ємним X).
// Перша ж команда вимикає рядок (recordInput).
void updateWifiInfoScroll(void) {
  if (firstInputReceived) {                 // людина вже керувала роботом:
    if (showingWifiInfo) {                  //   якщо рядок ще на екрані —
      showingWifiInfo = false;              //   прибираємо його
      if (currentFaceFrames != NULL && currentFaceFrameCount > 0) {
        updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);   // перемальовуємо чисте обличчя
      }
    }
    return;
  }

  unsigned long now = millis();
  // 30 000 мс (30 с) без команд — вмикаємо біжучий рядок.
  if (!showingWifiInfo && (now - lastInputTime >= 30000)) {
    showingWifiInfo  = true;
    wifiScrollPos    = 0;                   // починаємо з початку тексту
    lastWifiScrollMs = now;
  }
  if (!showingWifiInfo) return;             // рядок не показується — виходимо
  if (now - lastWifiScrollMs < 150) return; // зсуваємо раз на 150 мс
  lastWifiScrollMs = now;

  display.clearDisplay();
  if (currentFaceFrames != NULL && currentFaceFrameCount > 0) {
    // спочатку малюємо обличчя
    display.drawBitmap(0, 0, currentFaceFrames[currentFaceFrameIndex], 128, 64, SSD1306_WHITE);
  }
  display.fillRect(0, 0, 128, 10, SSD1306_BLACK);   // чорна смуга вгорі 128x10 пікселів під текст
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);                       // не переносити текст на новий рядок
  display.setCursor(-wifiScrollPos, 1);             // від'ємний X — текст "заїжджає" за лівий край
  display.print(wifiInfoText);
  display.setTextWrap(true);                        // повертаємо перенос як було
  display.display();

  wifiScrollPos += 2;                               // наступного разу зсунемо ще на 2 пікселі
  // Один символ шрифту = 6 пікселів. Текст повністю виїхав — починаємо знову.
  if (wifiScrollPos >= (int)(strlen(wifiInfoText) * 6)) wifiScrollPos = 0;
}

// 3
// Запустити mDNS — тоді робота можна відкрити за адресою http://sesame-robot.local
bool startMdns(void) {
  mdnsOk = MDNS.begin(DEVICE_HOSTNAME);         // реєструємо ім'я "sesame-robot" у мережі
  if (mdnsOk) {
    MDNS.addService("http", "tcp", 80);         // оголошуємо: "у мене є сайт на порту 80"
    Serial.printf("mDNS started: http://%s.local\n", DEVICE_HOSTNAME);
  } else {
    Serial.println("Error setting up mDNS responder!");
  }
  return mdnsOk;
}

// Після підключення до роутера: оновити mDNS і текст на екрані
void announceNetwork(const char* ssid) {
  char apIp[16], netIp[16];                         // два масиви під тексти IP
  MDNS.end();                                       // зупиняємо старий mDNS
  startMdns();                                      // і запускаємо знову вже в новій мережі
  ipToText(WiFi.softAPIP(), apIp, sizeof(apIp));    // IP робота в його власній мережі
  ipToText(networkIP, netIp, sizeof(netIp));        // IP робота в домашній мережі
  // Складаємо текст біжучого рядка. snprintf ніколи не вийде за 200 символів масиву.
  snprintf(wifiInfoText, sizeof(wifiInfoText),
           "AP: %s (%s)  |  Network: %s (%s) or %s.local  |  ",
           AP_SSID, apIp, ssid, netIp, DEVICE_HOSTNAME);
}

// Текст біжучого рядка, коли є тільки своя мережа робота.
void setApOnlyInfoText(void) {
  char apIp[16];
  ipToText(WiFi.softAPIP(), apIp, sizeof(apIp));
  snprintf(wifiInfoText, sizeof(wifiInfoText),
           "Connect to WiFi: %s  |  Pass: %s  |  IP: %s  |  Captive Portal will auto-open!  |  ",
           AP_SSID, AP_PASS, apIp);
}

// Знову показати адресу на екрані (після підключення з сайту)
void showWifiInfoNow(void) {
  firstInputReceived = false;            // "забуваємо", що людина вже керувала
  lastInputTime      = millis() - 30000; // ніби 30 с уже минуло — рядок з'явиться одразу
  showingWifiInfo    = false;
}

// 3
// Підключення при старті (блокує, але сайт ще не запущений, тож це нормально)
// Повертає true, якщо підключились.
bool connectToWifi(const char* ssid, const char* pass, uint32_t timeoutMs) {
  if (ssid == NULL || ssid[0] == '\0') return false;   // немає назви мережі

  Serial.printf("Connecting to WiFi network: %s\n", ssid);
  WiFi.mode(WIFI_AP_STA);            // точка доступу робота теж залишається
  WiFi.setHostname(DEVICE_HOSTNAME); // як робот називатиметься в списку пристроїв роутера
  WiFi.begin(ssid, pass);            // почати підключення (функція не чекає результату)

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {        // чекаємо не довше timeoutMs
    wl_status_t st = WiFi.status();             // поточний стан підключення
    if (st == WL_CONNECTED) break;              // підключились — виходимо з циклу
    // Після першої секунди: мережу не знайдено або пароль неправильний — далі чекати нема сенсу.
    if (millis() - start > 1000 && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL)) break;
    delay(250);                                 // перевіряємо 4 рази на секунду
    Serial.print(".");                          // крапки в Serial, щоб було видно, що чекаємо
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {          // так і не підключились
    Serial.println("WiFi connect failed.");
    WiFi.disconnect();                          // припиняємо спроби
    return false;
  }

  networkConnected = true;                      // запам'ятали, що підключені
  networkIP = WiFi.localIP();                   // IP, який видав роутер
  char ip[16];
  ipToText(networkIP, ip, sizeof(ip));
  Serial.printf("Connected! IP: %s\n", ip);
  return true;
}

// Завершити спробу підключення з сайту: записати помилку (або "") і повернутись у IDLE.
void finishWifiSetup(const char* err) {
  copyText(wifiSetupError, sizeof(wifiSetupError), err);
  memset(wifiSetupPass, 0, sizeof(wifiSetupPass));   // не тримати пароль у пам'яті
  // memset(масив, 0, розмір) — заповнити весь масив нулями
  wifiSetupState = WIFI_SETUP_IDLE;
}

// 3
// Підключення з сайту: працює кроками з loop(), щоб сайт не зависав
void updateWifiSetup(void) {
  // ===== СТАН 1: IDLE — нічого не підключаємо =====
  if (wifiSetupState == WIFI_SETUP_IDLE) {
    // раз на 5 секунд перевіряємо, чи не пропав зв'язок з роутером
    static unsigned long lastCheckMs = 0;          // static — пам'ятає значення між викликами
    if (millis() - lastCheckMs < 5000) return;     // ще не минуло 5 с — виходимо
    lastCheckMs = millis();

    bool live = (WiFi.status() == WL_CONNECTED);   // чи є зв'язок з роутером зараз
    if (live != networkConnected) {                // стан змінився з минулого разу?
      networkConnected = live;
      if (live) {                                  // зв'язок з'явився —
        networkIP = WiFi.localIP();
        announceNetwork(WiFi.SSID().c_str());      //   оновлюємо текст на екрані
      } else {
        Serial.println("Station link lost.");      // зв'язок пропав
      }
    }
    return;
  }

  // СТАН 2: QUEUED — запит прийшов, чекаємо 300 мс =====
  if (wifiSetupState == WIFI_SETUP_QUEUED) {
    // чекаємо, щоб сайт встиг отримати відповідь, бо канал Wi-Fi може змінитись
    if (millis() - wifiSetupQueuedMs < WIFI_SETUP_START_DELAY_MS) return;
    Serial.printf("Connecting to WiFi network: %s\n", wifiSetupSsid);
    WiFi.mode(WIFI_AP_STA);                        // своя мережа + підключення до роутера
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.begin(wifiSetupSsid, wifiSetupPass);      // починаємо підключення (не чекаємо!)
    wifiSetupStartMs = millis();                   // запам'ятали час старту для тайм-ауту
    wifiSetupState   = WIFI_SETUP_CONNECTING;      // переходимо в наступний стан
    return;
  }

  // СТАН 3: CONNECTING — перевіряємо, чи підключились =====
  // WIFI_SETUP_CONNECTING
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {                        // УСПІХ
    networkConnected = true;
    networkIP = WiFi.localIP();
    char ip[16];
    ipToText(networkIP, ip, sizeof(ip));
    Serial.printf("Connected! IP: %s\n", ip);
    announceNetwork(wifiSetupSsid);                // новий текст біжучого рядка
    showWifiInfoNow();                             // одразу показати його на екрані
    finishWifiSetup("");                           // "" = помилки немає, повертаємось у IDLE
    return;
  }

  // "Остаточна" помилка: минуло більше секунди, і мережа не знайдена або пароль невірний.
  bool terminal = (millis() - wifiSetupStartMs > 1000) &&
                  (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL);
  // Помилка або минуло 15 секунд — здаємось.
  if (terminal || millis() - wifiSetupStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
    const char* err;                                              // текст помилки для сайту
    if (st == WL_NO_SSID_AVAIL)       err = "Network not found";
    else if (st == WL_CONNECT_FAILED) err = "Wrong password or connection rejected";
    else                              err = "Connection timed out";

    Serial.printf("WiFi connect failed: %s\n", err);
    WiFi.disconnect();                             // припиняємо спроби
    networkConnected = false;
    setApOnlyInfoText();                           // текст на екрані — тільки своя мережа
    finishWifiSetup(err);                          // запам'ятати помилку, повернутись у IDLE
  }
  // Інакше — ще підключаємось; перевіримо знову на наступному оберті loop().
}

// 3
// "/" — віддати сторінку сайту. 200 = OK, "text/html" = це HTML,
// index_html — уся сторінка з web-page.h.
void handleRoot(void) {
  server.send(200, "text/html", index_html);
}

// 3
// /cmd?go=forward   /cmd?pose=wave   /cmd?stop=1   /cmd?motor=3&value=90
void handleCommandWeb(void) {
  char value[24];   // сюди скопіюємо назву команди з запиту

  // --- Поза або ходьба: /cmd?pose=wave або /cmd?go=forward ---
  // server.hasArg("pose") — чи є в адресі параметр pose=...
  if (server.hasArg("pose") || server.hasArg("go")) {
    // Тернарний оператор вибирає, який параметр читати: "pose" чи "go".
    getArg(server.hasArg("pose") ? "pose" : "go", value, sizeof(value));
    setCommand(value);    // ПОКЛАСТИ В СКРИНЬКУ (точка стику 1)
    recordInput();        // людина щось натиснула
    exitIdle();           // перестати кліпати
    server.send(200, "text/plain", "OK");   // відповідаємо одразу, рух іде в loop()
  }
  // --- Стоп: /cmd?stop=1 (сайт надсилає, коли відпускаєш кнопку ходьби) ---
  else if (server.hasArg("stop")) {
    clearCommand();       // порожня скринька -> pressingCheck() зупинить ходьбу
    recordInput();
    server.send(200, "text/plain", "OK");
  }
  // --- Ручний мотор: /cmd?motor=3&value=90 ---
  else if (server.hasArg("motor") && server.hasArg("value")) {
    char motorText[8];
    getArg("motor", motorText, sizeof(motorText));   // номер або ім'я мотора як текст
    int motorNum = atoi(motorText);                  // "1".."8"
    int servoIdx = servoNameToIndex(motorText);      // або "R1", "L3"...
    int angle    = getArgInt("value");               // кут як число

    if (angle < 0 || angle > 180) {                  // кут поза межами
      server.send(400, "text/plain", "Invalid motor or angle");
    } else if (motorNum >= 1 && motorNum <= SERVO_COUNT) {   // прийшов номер 1..8
      setServoAngle(motorNum - 1, angle);            // на сайті 1..8, у коді 0..7, тому -1
      recordInput();
      server.send(200, "text/plain", "OK");
    } else if (servoIdx != -1) {                     // прийшло ім'я на кшталт "L3"
      setServoAngle(servoIdx, angle);
      recordInput();
      server.send(200, "text/plain", "OK");
    } else {                                         // ні номер, ні ім'я
      server.send(400, "text/plain", "Invalid motor or angle");
    }
  }
  // --- Нічого з переліченого ---
  else {
    server.send(400, "text/plain", "Bad Args");
  }
}

// -3
// — це лапка всередині рядка C. Буфер має фіксований розмір, тому
// переповнення не буде.
// ------------------------------------------------------------------
// "/getSettings" — віддати поточні налаштування швидкості у вигляді JSON.
void handleGetSettings(void) {
  char json[128];   // 128 символів вистачає з запасом
  // Результат: {"frameDelay":100,"walkCycles":10,"motorCurrentDelay":20,"faceFps":8}
  snprintf(json, sizeof(json),
           "{\"frameDelay\":%d,\"walkCycles\":%d,\"motorCurrentDelay\":%d,\"faceFps\":%d}",
           frameDelay, walkCycles, motorCurrentDelay, faceFps);
  sendJson(200, json);
}

// беремо нові значення з запиту і обмежуємо їх через
// constrain. Випадкове walkCycles=99999 не змусить робота ходити годину.
// Налаштування живуть до перезавантаження, а назавжди — у config.h.
// ------------------------------------------------------------------
// "/setSettings?frameDelay=80&walkCycles=5..." — змінити налаштування.
// Кожен параметр міняємо, тільки якщо він є в запиті, і обмежуємо межами.
void handleSetSettings(void) {
  if (server.hasArg("frameDelay"))        frameDelay        = constrain(getArgInt("frameDelay"), 10, 2000);
  if (server.hasArg("walkCycles"))        walkCycles        = constrain(getArgInt("walkCycles"), 1, 100);
  if (server.hasArg("motorCurrentDelay")) motorCurrentDelay = constrain(getArgInt("motorCurrentDelay"), 0, 500);
  if (server.hasArg("faceFps"))           faceFps           = constrain(getArgInt("faceFps"), 1, 30);
  server.send(200, "text/plain", "OK");
}

// GET /api/status
// Стан робота для сайту (зелена крапка "онлайн" і "виконує: ...").
void handleGetStatus(void) {
  char json[256];
  size_t pos = 0;     // де зараз кінець тексту в json (для appendText)
  char ip[16];

  ipToText(WiFi.softAPIP(), ip, sizeof(ip));
  // Перша частина JSON. networkConnected ? "true" : "false" — bool перетворюємо на текст.
  appendText(json, sizeof(json), &pos,
             "{\"currentCommand\":\"%s\",\"currentFace\":\"%s\",\"networkConnected\":%s,\"apIP\":\"%s\"",
             currentCommand, currentFaceName, networkConnected ? "true" : "false", ip);
  if (networkConnected) {                                   // якщо підключені до роутера —
    ipToText(networkIP, ip, sizeof(ip));
    appendText(json, sizeof(json), &pos, ",\"networkIP\":\"%s\"", ip);   // дописуємо ще й цей IP
  }
  appendText(json, sizeof(json), &pos, "}");               // закриваємо JSON
  sendJson(200, json);
}

//  це API для програм (скрипт на Python, голосовий асистент).
// Приймає POST із JSON. Спочатку перевіряємо метод, потім розбираємо
// тіло через наш jsonGetString. Кнопки облич на сайті теж ідуть сюди.
//
// П: Різниця між GET і POST?
// В: GET передає параметри в адресі (/cmd?pose=wave). POST передає дані
//    в тілі запиту. POST використовують, коли щось надсилають.
// ------------------------------------------------------------------
// POST /api/command   {"command":"wave"}  або  {"face":"happy"}  або обидва
void handleApiCommand(void) {
  if (server.method() != HTTP_POST) {                       // приймаємо тільки POST
    sendJson(405, "{\"error\":\"Method not allowed\"}");    // 405 = не той метод
    return;
  }

  char body[256];             // тіло запиту (сам JSON)
  char command[24] = "";      // сюди дістанемо значення "command"
  char face[24]    = "";      // сюди — значення "face"
  getArg("plain", body, sizeof(body));    // "plain" — так бібліотека WebServer віддає тіло POST-запиту
  Serial.printf("API Command received: %s\n", body);

  bool hasFace         = jsonGetString(body, "face", face, sizeof(face));   // чи є поле face
  bool hasCommandField = (strstr(body, "\"command\"") != NULL);             // чи є в тексті слово "command"

  if (!hasFace && !hasCommandField) {                       // немає ні того, ні іншого
    sendJson(400, "{\"error\":\"Missing command field\"}");
    return;
  }
  // Поле command є, але його значення не текст у лапках (наприклад "command":5)
  if (hasCommandField && !jsonGetString(body, "command", command, sizeof(command))) {
    sendJson(400, "{\"error\":\"Invalid command format\"}");
    return;
  }

  if (hasFace) {
    if (!hasCommandField) exitIdle();   // інакше кліпання одразу змінить обличчя
    setFace(face);
  }
  recordInput();

  if (!hasCommandField) {                                   // прийшло тільки обличчя
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Face updated\"}");
  } else if (strcmp(command, "stop") == 0) {                // команда "stop"
    clearCommand();
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Command stopped\"}");
  } else {                                                  // будь-яка інша команда руху
    setCommand(command);                                    // кладемо в скриньку, як і кнопки сайту
    exitIdle();
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

// пошук мереж триває 2–4 секунди, тому scanNetworks(true)
// працює у фоні. Сайт перепитує кожні 0,7 с, поки замість
// {"scanning":true} не прийде список мереж.
//
// П: Навіщо тут static char json[3072]?
// В: 3 КБ — це забагато для стека функції, він може переповнитись.
//    static кладе масив у звичайну пам'ять один раз на весь час роботи.
// GET /api/wifi/scan  -> {"scanning":true}, поки шукає, потім список мереж
void handleWifiScan(void) {
  // scanComplete() повертає: кількість знайдених мереж (0 і більше),
  // WIFI_SCAN_RUNNING (пошук іде) або інше від'ємне число (пошук не запускався).
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {                 // пошук ще триває
    sendJson(200, "{\"scanning\":true}");       //   кажемо сайту "зачекай"
    return;
  }
  if (n < 0) {   // результатів ще немає -> почати пошук
    if (wifiSetupState == WIFI_SETUP_IDLE) {    // не шукаємо, поки йде підключення
      if (WiFi.getMode() == WIFI_AP) {          // якщо зараз тільки своя мережа —
        WiFi.mode(WIFI_AP_STA);       // для пошуку треба увімкнути режим клієнта
        wifiRestoreApOnly = true;               //   і запам'ятати повернути назад
      }
      WiFi.scanNetworks(true);        // true = у фоні, сайт не зависає
    }
    sendJson(200, "{\"scanning\":true}");
    return;
  }

  // Сюди доходимо, коли пошук закінчився і n — кількість мереж.
  static char json[3072];   // великий буфер для списку мереж (static — не на стеку)
  size_t pos = 0;
  char ssid[200];   // з запасом для екранування

  appendText(json, sizeof(json), &pos, "[");           // JSON-масив починається з [
  for (int i = 0; i < n; i++) {                        // для кожної знайденої мережі:
    if (sizeof(json) - pos < 260) break;   // місце закінчується, решту мереж пропускаємо
    jsonEscape(ssid, sizeof(ssid), WiFi.SSID(i).c_str());   // назва мережі, безпечна для JSON
    // Додаємо {"ssid":"Home","rssi":-50,"secure":true}; кома перед усіма, крім першої.
    // RSSI — сила сигналу в dBm (ближче до 0 = сильніше).
    appendText(json, sizeof(json), &pos, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
               (i > 0) ? "," : "", ssid, (int)WiFi.RSSI(i),
               (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true");
  }
  appendText(json, sizeof(json), &pos, "]");           // закриваємо масив
  WiFi.scanDelete();                                   // звільняємо пам'ять з результатами пошуку

  // Якщо перед пошуком вмикали режим клієнта — повертаємо "тільки своя мережа".
  if (wifiRestoreApOnly && wifiSetupState == WIFI_SETUP_IDLE && WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
  }
  wifiRestoreApOnly = false;
  sendJson(200, json);
}

// POST /api/wifi/connect  (форма: ssid, password)
// Тільки ставить підключення в чергу; саме підключення робить updateWifiSetup().
void handleWifiConnect(void) {
  if (server.method() != HTTP_POST) {
    sendJson(405, "{\"success\":false,\"error\":\"Method not allowed\"}");
    return;
  }
  if (server.arg("ssid").length() == 0) {              // назву мережі не вказали
    sendJson(400, "{\"success\":false,\"error\":\"SSID required\"}");
    return;
  }
  if (wifiSetupState != WIFI_SETUP_IDLE) {             // уже підключаємось
    sendJson(409, "{\"success\":false,\"error\":\"Connection attempt already in progress\"}");  // 409 = конфлікт
    return;
  }

  getArg("ssid", wifiSetupSsid, sizeof(wifiSetupSsid));       // назва мережі
  getArg("password", wifiSetupPass, sizeof(wifiSetupPass));   // пароль
  wifiSetupError[0] = '\0';                  // стираємо стару помилку
  wifiSetupQueuedMs = millis();              // запам'ятали час запиту (для паузи 300 мс)
  wifiSetupState    = WIFI_SETUP_QUEUED;     // машина станів: IDLE -> QUEUED
  wifiRestoreApOnly = false;
  sendJson(200, "{\"success\":true,\"pending\":true}");       // "прийняв, підключаюсь"
}

// GET /api/wifi/status
// Сайт питає це кожну секунду під час підключення.
void handleWifiStatus(void) {
  char json[384];
  char text[200];      // тимчасовий масив для екранованого тексту
  size_t pos = 0;
  bool connected = (WiFi.status() == WL_CONNECTED);

  appendText(json, sizeof(json), &pos, "{\"connected\":%s,\"connecting\":%s",
             connected ? "true" : "false",
             (wifiSetupState != WIFI_SETUP_IDLE) ? "true" : "false");   // підключаємось, якщо не IDLE

  if (wifiSetupError[0] != '\0') {                        // була помилка —
    jsonEscape(text, sizeof(text), wifiSetupError);
    appendText(json, sizeof(json), &pos, ",\"lastError\":\"%s\"", text);   // додаємо її текст
  }
  if (connected) {                                        // підключені — додаємо деталі
    char ip[16];
    jsonEscape(text, sizeof(text), WiFi.SSID().c_str());  // назва мережі
    ipToText(WiFi.localIP(), ip, sizeof(ip));             // IP від роутера
    appendText(json, sizeof(json), &pos,
               ",\"ssid\":\"%s\",\"ip\":\"%s\",\"host\":\"%s.local\",\"mdns\":%s,\"rssi\":%d",
               text, ip, DEVICE_HOSTNAME, mdnsOk ? "true" : "false", (int)WiFi.RSSI());
  }
  appendText(json, sizeof(json), &pos, "}");
  sendJson(200, json);
}

// будь-яка невідома адреса віддає сторінку керування — саме
// так працює captive portal. Невідомі адреси /api/... отримують
// помилку 404 у JSON, щоб програми бачили, що адреса неправильна.
// Будь-яка невідома адреса відкриває сайт (так телефон сам показує сторінку).
// Невідомі /api/... адреси отримують помилку 404.
void handleNotFound(void) {
  // server.uri() — адреса, яку запитали; перевіряємо, чи починається з "/api/" (5 символів)
  if (strncmp(server.uri().c_str(), "/api/", 5) == 0) {
    sendJson(404, "{\"error\":\"Not found\"}");
    return;
  }
  handleRoot();   // будь-що інше  віддаємо сторінку сайту
}
