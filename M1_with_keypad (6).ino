#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------- LCD I2C (16x2) ----------------
// If your screen doesn't show anything, try changing 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int buzzer = 3;

// ---------------- Start push button ----------------
const int START_BUTTON = A0; // wire button between A0 and GND
const unsigned long DEBOUNCE_MS = 30;

// ---------------- Keypad 4x4 (lightweight manual scan, no Keypad library) ----------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {4, 5, 6, 7};  // R1-R4
byte colPins[COLS] = {8, 9, 10, 2}; // C1-C4 (moved off pin 11 to free it for the motor's ENA/PWM)

// ---------------- Reward motor (win only — no relay/punishment hardware) ----------------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;                // must be a PWM pin
const int MOTOR_SPEED = 200;       // 0-255
const unsigned long MOTOR_RUN_TIME = 3000; // ms the motor spins on a win

// ---------------- Punishments ----------------
const char* punishments[] = {"Sing", "Dance", "Dare", "Truth"};
const int NUM_PUNISHMENTS = sizeof(punishments) / sizeof(punishments[0]);

void runMotor(unsigned long durationMs) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, MOTOR_SPEED);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Dispensing the"));
  lcd.setCursor(0, 1);
  lcd.print(F("reward..."));

  delay(durationMs);
  stopMotor();

  lcd.clear();
}

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

// Scans the matrix once and returns the pressed key, or 0 if none pressed.
// Blocks briefly (~20ms) only when a key is actually held, for simple debounce.
char getKeyPress() {
  for (byte r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW);

    for (byte c = 0; c < COLS; c++) {
      pinMode(colPins[c], INPUT_PULLUP);
      if (digitalRead(colPins[c]) == LOW) {
        delay(20); // debounce
        if (digitalRead(colPins[c]) == LOW) {
          while (digitalRead(colPins[c]) == LOW) { /* wait for release */ }
          pinMode(rowPins[r], INPUT);
          return keys[r][c];
        }
      }
      pinMode(colPins[c], INPUT);
    }

    pinMode(rowPins[r], INPUT);
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(buzzer, OUTPUT);

  pinMode(START_BUTTON, INPUT_PULLUP);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  stopMotor();

  Wire.begin();

  lcd.init();
  lcd.backlight();

  randomSeed(analogRead(A1)); // A1 left floating for entropy; swap pins if A1 is in use

  lcd.setCursor(0, 0);
  lcd.print(F("  Smart Dice"));
  lcd.setCursor(0, 1);
  lcd.print(F(" Press button..."));
}

void loop() {
  waitForStart();                    // blocks until the push button is pressed

  int userNumber = getUserNumber();  // shows "Enter a number 1-6", waits for keypad entry
  int diceResult = rollDice();       // shuffling animation, returns the final face (1-6)

  if (diceResult == userNumber) {
    showWin();
  } else {
    showLose();                      // blocks until user presses # to restart
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  Smart Dice"));
  lcd.setCursor(0, 1);
  lcd.print(F(" Press button..."));
}

// =====================================================================
//  START BUTTON
// =====================================================================
void waitForStart() {
  // Wait for a press (active LOW), with simple debounce, then wait for release.
  while (digitalRead(START_BUTTON) == HIGH) {
    delay(10);
  }
  delay(DEBOUNCE_MS);
  if (digitalRead(START_BUTTON) == HIGH) return; // was noise, bail and let loop() re-check
  while (digitalRead(START_BUTTON) == LOW) { /* wait for release */ }
}

// =====================================================================
//  NUMBER ENTRY
// =====================================================================
int getUserNumber() {
  int digitEntered = -1;
  bool valid = false;

  showEntryScreen(digitEntered);

  while (!valid) {
    char key = getKeyPress();

    if (key) {
      if (key >= '1' && key <= '6') {
        digitEntered = key - '0';
        showEntryScreen(digitEntered);
      } else if (key == '#') {
        if (digitEntered >= 1 && digitEntered <= 6) {
          valid = true;
        }
      }
    }
  }

  return digitEntered;
}

void showEntryScreen(int digit) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Enter number 1-6"));
  lcd.setCursor(0, 1);
  lcd.print(F("Number: "));
  if (digit >= 0) {
    lcd.print(digit);
  } else {
    lcd.print(F("_"));
  }
}

// =====================================================================
//  DICE ROLL (LCD animation)
// =====================================================================
int rollDice() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Rolling..."));

  for (int i = 0; i < 15; i++) {
    int shuffled = random(1, 7);
    lcd.setCursor(0, 1);
    lcd.print(F("Number: "));
    lcd.print(shuffled);
    lcd.print(F(" "));
    tone(buzzer, 1200, 60);
    delay(80 + i * 5); // gradually slows down, like a spinning wheel settling
  }

  int result = random(1, 7);
  lcd.setCursor(0, 1);
  lcd.print(F("Number: "));
  lcd.print(result);
  lcd.print(F(" "));

  for (int i = 0; i < 3; i++) {
    tone(buzzer, 700, 200);
    delay(250);
  }

  return result;
}

// =====================================================================
//  WIN / LOSE
// =====================================================================
void showWin() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Congrats!"));
  lcd.setCursor(0, 1);
  lcd.print(F("You Win!"));
  delay(1500);

  runMotor(MOTOR_RUN_TIME); // spins for MOTOR_RUN_TIME ms, then stops itself

  delay(1000); // then loop() restarts the game automatically
}

void showLose() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("You lose!"));
  delay(1500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Loading"));
  lcd.setCursor(0, 1);
  lcd.print(F("punishment...")); // fits perfectly on line 2
  delay(2000);


  int choice = random(0, NUM_PUNISHMENTS);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Punishment:"));
  lcd.setCursor(0, 1);
  lcd.print(punishments[choice]);
  delay(7000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Press # button"));
  lcd.setCursor(0, 1);
  lcd.print(F("to restart"));

  char key;
  do {
    key = getKeyPress();
  } while (key != '#');
}
