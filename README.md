# Sesame Robot — прошивка в C-стилі

Перероблена прошивка для робота [Sesame](https://github.com/dorianborian/sesame-robot).
Рухи, обличчя та API ті самі, що в оригіналі. Змінено стиль коду та сайт.

## Файли

| Файл | Що в ньому | Чи треба міняти |
|---|---|---|
| `sesame-c-style.ino` | головна програма | ні |
| `config.h` | **плата, Wi-Fi, швидкість, калібрування** | **так, тут усе налаштовується** |
| `movement-sequences.h` | усі рухи + таблиця команд | якщо хочеш свої рухи |
| `web-page.h` | сайт керування | якщо хочеш змінити сайт |
| `face-bitmaps.h` | картинки облич (без змін з оригіналу) | ні |

> ⚠️ Усі 5 файлів мають лежати в одній папці, і папка має називатися **так само, як .ino файл**: `sesame-c-style`.

---

## Як запустити (крок за кроком)

### 1. Встановити Arduino IDE
Завантаж з https://www.arduino.cc/en/software (версія 2.x).

### 2. Додати підтримку ESP32
1. **File → Preferences** (Файл → Налаштування).
2. У поле **Additional boards manager URLs** встав:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager**, знайди **esp32** і встанови **esp32 by Espressif Systems**.

### 3. Встановити бібліотеки
**Tools → Manage Libraries** (або значок книжки зліва), знайди та встанови:
- **ESP32Servo** — обов'язково версію **3.0.9** (вибери її у випадаючому списку; новіші мають баг, коли один мотор смикає інші)
- **Adafruit SSD1306**
- **Adafruit GFX Library**

(Якщо Arduino спитає «встановити залежності?» — натисни **Install all**.)

### 4. Відкрити проєкт
**File → Open** → вибери `sesame-c-style/sesame-c-style.ino`. Зверху з'являться вкладки з усіма файлами.

### 5. Вибрати свою плату в коді
Відкрий вкладку **config.h**, пункт 1. Залиш без `//` тільки свою плату:
```c
#define BOARD_S2_MINI        // Lolin S2 Mini
//#define BOARD_DISTRO_V3    // Distro Board V3
```

### 6. Вибрати плату в Arduino IDE
**Tools → Board → esp32 →**

| Твоя плата | Що вибрати | Додатково в Tools |
|---|---|---|
| Lolin S2 Mini | **LOLIN S2 Mini** | USB CDC On Boot: **Enabled** |
| Distro Board V2 / V3 | **ESP32S3 Dev Module** | USB CDC On Boot: **Enabled**, Flash Mode: **QIO 80MHz** |
| Distro Board V1 | **ESP32 Dev Module** | — |

### 7. Підключити і прошити
1. Підключи плату USB-кабелем (кабель має бути з даними, не тільки зарядний).
2. **Tools → Port** → вибери порт, що з'явився (COM3, /dev/ttyACM0 тощо).
3. Натисни **→ Upload** (стрілка вгорі).

**Якщо S2 Mini не з'являється в портах:** затисни кнопку **0**, натисни і відпусти **RST**, відпусти **0**. Плата перейде в режим прошивки. Після прошивки натисни **RST** ще раз.

### 8. Керувати роботом
1. На телефоні підключись до Wi-Fi **Sesame-Controller**, пароль **12345678**.
2. Сайт має відкритися сам. Якщо ні — відкрий у браузері **http://192.168.4.1**.
3. Кнопки руху **треба тримати**: відпустив — робот зупинився.

---

## Serial Monitor (налагодження)
**Tools → Serial Monitor**, швидкість **115200**. Напиши `help` — побачиш список команд:

- `rn wv` — помахати (коди всіх рухів показує `help`)
- `fc happy` — змінити обличчя
- `3 90` — мотор 3 на 90°
- `st 3 5` — підкрутити мотор 3 на +5°, потім `st save` і встав рядок у `config.h`

---

## Як додати свій рух
1. У `movement-sequences.h` напиши функцію, наприклад:
   ```c
   void runJumpPose(void) {
     setFaceWithMode("excited", FACE_ANIM_ONCE);
     setServoAngle(R3, 90); setServoAngle(L3, 90);
     delayWithFace(500);
     runStandPose(1);
   }
   ```
2. Додай рядок у таблицю `COMMANDS[]` в кінці того ж файлу:
   ```c
   { "jump", "jp", runJumpPose, false },
   ```
3. Додай кнопку на сайті: у `web-page.h` знайди `const POSES = [` і допиши `["jump","🦘","Стрибок"],`.

---

## Що змінено порівняно з оригіналом

**Переписано в C-стилі:**
- `String` → масиви `char[]`; порівняння через `strcmp()`, копіювання через `copyText()`
- JSON збирається через `snprintf()` замість `String +=`
- довгий ланцюжок `if (cmd == "...")` → таблиця `COMMANDS[]` з вказівниками на функції
- `nullptr` → `NULL`, `enum X : uint8_t` → `typedef enum`, без аргументів за замовчуванням
- `if (currentCommand == "wave") currentCommand = ""` в кожній позі → одна перевірка в `loop()`
- налаштування винесено в `config.h`, вибір плати одним рядком

**Залишилось C++ (так треба):** виклики бібліотек (`server.on()`, `display.print()`, `WiFi.begin()`, `servos[i].write()`) і HTML-сторінка в `R"rawliteral(...)"`.

**Сайт:** новий, простіший, українською. Додано кнопки облич, керування стрілками/WASD, статус робота. Адреси API ті самі, тому скрипти з оригінального репозиторію (Python/JavaScript) працюють як раніше.
