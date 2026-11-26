// ======================================================================
//  movement-sequences.h — ВСІ РУХИ РОБОТА
//
//  Як додати свій рух:
//    1. Напиши функцію void runMyPose(void) { ... } унизу, біля інших.
//    2. Додай один рядок у таблицю COMMANDS[] в самому кінці файлу.
//    3. (необов'язково) Додай кнопку на сайті у web-page.h.
// ======================================================================
#pragma once

#include <Arduino.h>
#include <string.h>

// ---- Номери моторів (ніг) ----
// R = права сторона, L = ліва. 1,2 = плечі, 3,4 = коліна.
typedef enum {
  R1 = 0,
  R2 = 1,
  L1 = 2,
  L2 = 3,
  R4 = 4,
  R3 = 5,
  L3 = 6,
  L4 = 7
} ServoName;

// Назви моторів у тому ж порядку, що й номери вище
static const char* const SERVO_NAMES[8] = { "R1", "R2", "L1", "L2", "R4", "R3", "L3", "L4" };

// "L3" -> 6, невідома назва -> -1
static int servoNameToIndex(const char* name) {
  for (int i = 0; i < 8; i++) {
    if (strcmp(SERVO_NAMES[i], name) == 0) return i;
  }
  return -1;
}

// ---- Режими анімації обличчя ----
typedef enum {
  FACE_ANIM_LOOP = 0,       // по колу: 1-2-3-1-2-3
  FACE_ANIM_ONCE = 1,       // один раз і зупинитись на останньому кадрі
  FACE_ANIM_BOOMERANG = 2   // туди-назад: 1-2-3-2-1
} FaceAnimMode;

// ---- Змінні та функції з головного файлу (.ino) ----
extern int frameDelay;
extern int walkCycles;

void setServoAngle(uint8_t channel, int angle);
void setFace(const char* faceName);
void setFaceMode(FaceAnimMode mode);
void setFaceWithMode(const char* faceName, FaceAnimMode mode);
void delayWithFace(unsigned long ms);
void enterIdle(void);
bool pressingCheck(const char* cmd, int ms);

// face = 1: показати обличчя "stand" і перейти в режим очікування
// face = 0: тільки поставити ноги (використовується всередині інших рухів)
void runStandPose(int face);

// ======================================================================
//  ПОЗИ (кути моторів не змінені з оригіналу)
// ======================================================================
void runRestPose(void) { 
  Serial.println(F("REST")); 
  setFaceWithMode("rest", FACE_ANIM_BOOMERANG); 
  for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
}

void runStandPose(int face) { 
  Serial.println(F("STAND")); 
  if (face == 1) setFaceWithMode("stand", FACE_ANIM_ONCE); 
  setServoAngle(R1, 135); 
  setServoAngle(R2, 45); 
  setServoAngle(L1, 45); 
  setServoAngle(L2, 135); 
  setServoAngle(R4, 0); 
  setServoAngle(R3, 180); 
  setServoAngle(L3, 0); 
  setServoAngle(L4, 180); 
  if (face == 1) enterIdle();
}

void runWavePose(void) { 
  Serial.println(F("WAVE")); 
  setFaceWithMode("wave", FACE_ANIM_ONCE); 
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(R4, 80); setServoAngle(L3, 180); 
  setServoAngle(L2, 90); setServoAngle(R1, 100); 
  delayWithFace(200);
  setServoAngle(L3, 180); 
  delayWithFace(300); 
  for (int i = 0; i < 4; i++) { 
    setServoAngle(L3, 180); delayWithFace(300); 
    setServoAngle(L3, 100); delayWithFace(300); 
  } 
  runStandPose(1); 
}

void runDancePose(void) { 
  Serial.println(F("DANCE")); 
  setFaceWithMode("dance", FACE_ANIM_LOOP); 
  setServoAngle(R1, 90); setServoAngle(R2, 90); 
  setServoAngle(L1, 90); setServoAngle(L2, 90); 
  setServoAngle(R4, 160); setServoAngle(R3, 160); 
  setServoAngle(L3, 10); setServoAngle(L4, 10); 
  delayWithFace(300); 
  for (int i = 0; i < 5; i++) { 
    setServoAngle(R4, 115); setServoAngle(R3, 115); 
    setServoAngle(L3, 10); setServoAngle(L4, 10); 
    delayWithFace(300); 
    setServoAngle(R4, 160); setServoAngle(R3, 160); 
    setServoAngle(L3, 65); setServoAngle(L4, 65); 
    delayWithFace(300); 
  } 
  runStandPose(1); 
}

void runSwimPose(void) { 
  Serial.println(F("SWIM")); 
  setFaceWithMode("swim", FACE_ANIM_ONCE); 
  for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
  for (int i = 0; i < 4; i++) { 
    setServoAngle(R1, 135); setServoAngle(R2, 45); 
    setServoAngle(L1, 45); setServoAngle(L2, 135); 
    delayWithFace(400); 
    setServoAngle(R1, 90); setServoAngle(R2, 90); 
    setServoAngle(L1, 90); setServoAngle(L2, 90); 
    delayWithFace(400); 
  } 
  runStandPose(1); 
}

void runPointPose(void) { 
  Serial.println(F("POINT")); 
  setFaceWithMode("point", FACE_ANIM_BOOMERANG); 
  setServoAngle(L2, 90); setServoAngle(R1, 135); 
  setServoAngle(R2, 100); setServoAngle(L4, 180); 
  setServoAngle(L1, 25); setServoAngle(L3, 145);
  setServoAngle(R4, 80); setServoAngle(R3, 170); 
  delayWithFace(2000); 
  runStandPose(1); 
}

void runPushupPose(void) {
  Serial.println(F("PUSHUP"));
  setFaceWithMode("pushup", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 90);
  setServoAngle(R3, 90);
  delayWithFace(500);
  for (int i = 0; i < 4; i++) {
    setServoAngle(L3, 0);
    setServoAngle(R3, 180);
    delayWithFace(600);
    setServoAngle(L3, 90);
    setServoAngle(R3, 90);
    delayWithFace(500);
  }
  runStandPose(1);
}

void runBowPose(void) {
  Serial.println(F("BOW"));
  setFaceWithMode("bow", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 0);
  setServoAngle(R3, 180);
  setServoAngle(L2, 180);
  setServoAngle(R2, 0);
  setServoAngle(R4, 0);
  setServoAngle(L4, 180);
  delayWithFace(600);
  setServoAngle(L3, 90);
  setServoAngle(R3, 90);
  delayWithFace(3000);
  runStandPose(1);
}

void runCutePose(void) {
  Serial.println(F("CUTE"));
  setFaceWithMode("cute", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L2, 160);
  setServoAngle(R2, 20);
  setServoAngle(R4, 180);
  setServoAngle(L4, 0);

  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 180);
  setServoAngle(R3, 0);
  delayWithFace(200);
  for (int i = 0; i < 5; i++) {
    setServoAngle(R4, 180);
    setServoAngle(L4, 45);
    delayWithFace(300);
    setServoAngle(R4, 135);
    setServoAngle(L4, 0);
    delayWithFace(300);
  }
  runStandPose(1);
}

void runFreakyPose(void) {
  Serial.println(F("FREAKY"));
  setFaceWithMode("freaky", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L2, 180);
  setServoAngle(R2, 0);
  setServoAngle(R4, 90);
  setServoAngle(R3, 0);
  delayWithFace(200);
  for (int i = 0; i < 3; i++) {
    setServoAngle(R3, 25);
    delayWithFace(400);
    setServoAngle(R3, 0);
    delayWithFace(400);
  }
  runStandPose(1);
}

void runWormPose(void) {
  Serial.println(F("WORM"));
  setFaceWithMode("worm", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 180); setServoAngle(R2, 0); setServoAngle(L1, 0); setServoAngle(L2, 180);
  setServoAngle(R4, 90); setServoAngle(R3, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
  delayWithFace(200);
  for(int i=0; i<5; i++) {
    setServoAngle(R3, 45); setServoAngle(L3, 135); setServoAngle(R4, 45); setServoAngle(L4, 135);
    delayWithFace(300);
    setServoAngle(R3, 135); setServoAngle(L3, 45); setServoAngle(R4, 135); setServoAngle(L4, 45);
    delayWithFace(300);
  }
  runStandPose(1);
}

void runShakePose(void) {
  Serial.println(F("SHAKE"));
  setFaceWithMode("shake", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 135); setServoAngle(L1, 45); setServoAngle(L3, 90); setServoAngle(R3, 90);
  setServoAngle(L2, 90); setServoAngle(R2, 90);
  delayWithFace(200);
  for(int i=0; i<5; i++) {
    setServoAngle(R4, 45); setServoAngle(L4, 135);
    delayWithFace(300);
    setServoAngle(R4, 0); setServoAngle(L4, 180);
    delayWithFace(300);
  }
  runStandPose(1);
}

void runShrugPose(void) {
  Serial.println(F("SHRUG"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_ONCE);
  delayWithFace(200);
  setServoAngle(R3, 90); setServoAngle(R4, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
  delayWithFace(1000);
  setFaceWithMode("shrug", FACE_ANIM_ONCE);
  setServoAngle(R3, 0); setServoAngle(R4, 180); setServoAngle(L3, 180); setServoAngle(L4, 0);
  delayWithFace(1500);
  runStandPose(1);
}

void runDeadPose(void) {
  Serial.println(F("DEAD"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_BOOMERANG);
  delayWithFace(200);
  setServoAngle(R3, 90); setServoAngle(R4, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
}

void runCrabPose(void) {
  Serial.println(F("CRAB"));
  setFaceWithMode("crab", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 90); setServoAngle(R2, 90); setServoAngle(L1, 90); setServoAngle(L2, 90);
  setServoAngle(R4, 0); setServoAngle(R3, 180); setServoAngle(L3, 45); setServoAngle(L4, 135);
  for(int i=0; i<5; i++) {
    setServoAngle(R4, 45); setServoAngle(R3, 135); setServoAngle(L3, 0); setServoAngle(L4, 180);
    delayWithFace(300);
    setServoAngle(R4, 0); setServoAngle(R3, 180); setServoAngle(L3, 45); setServoAngle(L4, 135);
    delayWithFace(300);
  }
  runStandPose(1);
}

// --- MOVEMENT ANIMATIONS ---
void runWalkPose(void) {
  Serial.println(F("WALK FWD"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  // Initial Step
  setServoAngle(R3, 135); setServoAngle(L3, 45);
  setServoAngle(R2, 100); setServoAngle(L1, 25);
  if (!pressingCheck("forward", frameDelay)) return;
  
  for (int i = 0; i < walkCycles; i++) {
    setServoAngle(R3, 135); setServoAngle(L3, 0);
    if (!pressingCheck("forward", frameDelay)) return;
    setServoAngle(L4, 135); setServoAngle(L2, 90);
    setServoAngle(R4, 0); setServoAngle(R1, 180);
    if (!pressingCheck("forward", frameDelay)) return;    
    setServoAngle(R2, 45); setServoAngle(L1, 90);
    if (!pressingCheck("forward", frameDelay)) return;
    setServoAngle(R4, 45); setServoAngle(L4, 180);
    if (!pressingCheck("forward", frameDelay)) return;
    setServoAngle(R3, 180); setServoAngle(L3, 45);
    setServoAngle(R2, 90); setServoAngle(L1, 0);
    if (!pressingCheck("forward", frameDelay)) return;  
    setServoAngle(L2, 135); setServoAngle(R1, 90);
    if (!pressingCheck("forward", frameDelay)) return;
  }
  runStandPose(1);
}

// Logic reversed from Walk
void runWalkBackward(void) {
  Serial.println(F("WALK BACK"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  if (!pressingCheck("backward", frameDelay)) return;
  
  for (int i = 0; i < walkCycles; i++) {
    setServoAngle(R3, 135); setServoAngle(L3, 0);
    if (!pressingCheck("backward", frameDelay)) return;
    setServoAngle(L4, 135); setServoAngle(L2, 135);
    setServoAngle(R4, 0); setServoAngle(R1, 90);
    if (!pressingCheck("backward", frameDelay)) return;    
    setServoAngle(R2, 90); setServoAngle(L1, 0);
    if (!pressingCheck("backward", frameDelay)) return;
    setServoAngle(R4, 45); setServoAngle(L4, 180);
    if (!pressingCheck("backward", frameDelay)) return;
    setServoAngle(R3, 180); setServoAngle(L3, 45);
    setServoAngle(R2, 45); setServoAngle(L1, 90);
    if (!pressingCheck("backward", frameDelay)) return;  
    setServoAngle(L2, 90); setServoAngle(R1, 180);
    if (!pressingCheck("backward", frameDelay)) return;
  }
  runStandPose(1);
}

// Simple turn logic
void runTurnLeft(void) {
  Serial.println(F("TURN LEFT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  for (int i = 0; i < walkCycles; i++) {
    //legset 1 (R1 L2)
    setServoAngle(R3, 135); setServoAngle(L4, 135); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R1, 180); setServoAngle(L2, 180); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R3, 180); setServoAngle(L4, 180); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R1, 135); setServoAngle(L2, 135);
    if (!pressingCheck("left", frameDelay)) return;
      //legset 2 (R2 L1)
    setServoAngle(R4, 45); setServoAngle(L3, 45); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R2, 90); setServoAngle(L1, 90); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R4, 0); setServoAngle(L3, 0); 
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R2, 45); setServoAngle(L1, 45);
    if (!pressingCheck("left", frameDelay)) return;  
  }
  runStandPose(1);
}

void runTurnRight(void) {
  Serial.println(F("TURN RIGHT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  for (int i = 0; i < walkCycles; i++) {
    //legset 2 (R2 L1)
    setServoAngle(R4, 45); setServoAngle(L3, 45); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R2, 0); setServoAngle(L1, 0); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R4, 0); setServoAngle(L3, 0); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R2, 45); setServoAngle(L1, 45);
    if (!pressingCheck("right", frameDelay)) return;  
    //legset 1 (R1 L2)
    setServoAngle(R3, 135); setServoAngle(L4, 135); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R1, 90); setServoAngle(L2, 90); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R3, 180); setServoAngle(L4, 180); 
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(R1, 135); setServoAngle(L2, 135);
    if (!pressingCheck("right", frameDelay)) return;
  }
  runStandPose(1);
}

// ======================================================================
//  ТАБЛИЦЯ КОМАНД
//  Кожен рядок: { назва для сайту, короткий код для Serial, функція, ходьба? }
//
//  "ходьба?" = true  -> рух повторюється, поки тримаєш кнопку на сайті
//  "ходьба?" = false -> рух виконується один раз
// ======================================================================
typedef void (*MoveFunction)(void);

typedef struct {
  const char*  name;        // те, що надсилає сайт: /cmd?pose=wave
  const char*  shortCode;   // для Serial Monitor: "rn wv"
  MoveFunction run;         // яка функція виконується
  bool         repeatWhileHeld;
} RobotCommand;

// runStandPose приймає параметр, тому для таблиці потрібна обгортка без параметрів
static void runStandCommand(void) { runStandPose(1); }

static const RobotCommand COMMANDS[] = {
  // ходьба (повторюється, поки кнопка натиснута)
  { "forward",  "wf", runWalkPose,     true  },
  { "backward", "wb", runWalkBackward, true  },
  { "left",     "tl", runTurnLeft,     true  },
  { "right",    "tr", runTurnRight,    true  },
  // пози (один раз)
  { "rest",     "rs", runRestPose,     false },
  { "stand",    "st", runStandCommand, false },
  { "wave",     "wv", runWavePose,     false },
  { "dance",    "dn", runDancePose,    false },
  { "swim",     "sw", runSwimPose,     false },
  { "point",    "pt", runPointPose,    false },
  { "pushup",   "pu", runPushupPose,   false },
  { "bow",      "bw", runBowPose,      false },
  { "cute",     "ct", runCutePose,     false },
  { "freaky",   "fk", runFreakyPose,   false },
  { "worm",     "wm", runWormPose,     false },
  { "shake",    "sk", runShakePose,    false },
  { "shrug",    "sg", runShrugPose,    false },
  { "dead",     "dd", runDeadPose,     false },
  { "crab",     "cb", runCrabPose,     false },
};

#define COMMAND_COUNT (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

// Знайти команду за назвою ("wave"). Якщо немає, повертає NULL.
static const RobotCommand* findCommandByName(const char* name) {
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (strcmp(COMMANDS[i].name, name) == 0) return &COMMANDS[i];
  }
  return NULL;
}

// Знайти команду за коротким кодом ("wv"). Якщо немає, повертає NULL.
static const RobotCommand* findCommandByCode(const char* code) {
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (strcmp(COMMANDS[i].shortCode, code) == 0) return &COMMANDS[i];
  }
  return NULL;
}
