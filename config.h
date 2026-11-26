
#pragma once
#define BOARD_S2_MINI        // Lolin S2 Mini (саморобна збірка)


#define AP_SSID  "Sesame-Controller"   // назва мережі робота
#define AP_PASS  "12345678"            // пароль, мінімум 8 символів

#define ENABLE_NETWORK_MODE 0          // 0 = вимкнено, 1 = увімкнено
#define NETWORK_SSID ""                // назва твоєї мережі
#define NETWORK_PASS ""                // пароль твоєї мережі
#define DEVICE_HOSTNAME "sesame-robot" // адреса буде http://sesame-robot.local


// 4. ШВИДКІСТЬ РУХІВ (значення за замовчуванням, можна міняти з сайту)

#define DEFAULT_FRAME_DELAY   100  // пауза між кроками ходьби, мс
#define DEFAULT_WALK_CYCLES   10   // скільки кроків за одне натискання
#define DEFAULT_MOTOR_DELAY   20   // пауза між моторами, мс (захист від просідання живлення)
#define DEFAULT_FACE_FPS      8    // швидкість анімації обличчя


// 5. КАЛІБРУВАННЯ МОТОРІВ (subtrim)

#define SERVO_SUBTRIM { 0, 0, 0, 0, 0, 0, 0, 0 }


//  Нижче піни для кожної плати. 
#if defined(BOARD_S2_MINI)
  #define I2C_SDA 33
  #define I2C_SCL 35
  #define SERVO_PINS { 1, 2, 4, 6, 8, 10, 13, 14 }
#elif defined(BOARD_DISTRO_V3)
  #define I2C_SDA 8
  #define I2C_SCL 9
  #define SERVO_PINS { 4, 5, 6, 7, 10, 11, 12, 13 }
#elif defined(BOARD_DISTRO_V2)
  #define I2C_SDA 8
  #define I2C_SCL 9
  #define SERVO_PINS { 4, 5, 6, 7, 15, 16, 17, 18 }
#elif defined(BOARD_DISTRO_V1)
  #define I2C_SDA 21
  #define I2C_SCL 22
  #define SERVO_PINS { 15, 2, 23, 19, 4, 16, 17, 18 }
#else
  #error "Вибери плату в config.h (пункт 1)"
#endif

// Екран OLED
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_I2C_ADDR  0x3C

// Кількість моторів
#define SERVO_COUNT 8
