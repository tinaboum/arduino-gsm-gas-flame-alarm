/*
 * GSM-connected gas and flame alarm
 * Target: Arduino Uno
 * Reconstructed from the June 2022 project video and its wiring diagram.
 *
 * Verified hardware shown in the video:
 * - Arduino Uno
 * - MQ-5 analog gas-sensor module
 * - Digital IR flame-sensor module
 * - 16x2 LCD with I2C backpack
 * - Neoway M660 GSM/GPRS module
 * - Active buzzer, red and green LEDs
 * - Relay-controlled 12 V fan
 *
 * IMPORTANT BEFORE USE:
 * 1. Enter the destination phone number below.
 * 2. Confirm the LCD I2C address (commonly 0x27 or 0x3F).
 * 3. Calibrate the MQ-5 thresholds for the real enclosure and target gas.
 * 4. Confirm the relay and flame-module active levels.
 * 5. Confirm the M660 carrier-board UART baud rate and logic-level shifting.
 *
 * This is prototype firmware, not certified life-safety equipment.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// ---------------------------------------------------------------------------
// Hardware configuration reconstructed from the video wiring diagram
// ---------------------------------------------------------------------------
constexpr uint8_t GAS_SENSOR_PIN = A2;
constexpr uint8_t FLAME_SENSOR_PIN = 5;
constexpr uint8_t BUZZER_PIN = 2;
constexpr uint8_t GREEN_LED_PIN = 7;
constexpr uint8_t RED_LED_PIN = 8;
constexpr uint8_t FAN_RELAY_PIN = 10;

// Arduino RX connects to the GSM module TX; Arduino TX connects to module RX.
constexpr uint8_t GSM_RX_PIN = 9;
constexpr uint8_t GSM_TX_PIN = 12;

constexpr uint8_t LCD_ADDRESS = 0x27;
constexpr uint8_t LCD_COLUMNS = 16;
constexpr uint8_t LCD_ROWS = 2;

// Most LM393 flame modules assert LOW when flame is detected.
constexpr bool FLAME_ACTIVE_LOW = true;

// Many relay modules are active LOW. Change to false if yours is active HIGH.
constexpr bool RELAY_ACTIVE_LOW = true;

// The video proves raw values near 15-26 at ambient and 721 during gas
// exposure. It does not document the original threshold. These two values are
// safe placeholders for functional testing and MUST be calibrated.
constexpr uint16_t GAS_ALARM_ON_THRESHOLD = 300;
constexpr uint16_t GAS_ALARM_OFF_THRESHOLD = 250;

constexpr uint8_t GAS_AVERAGE_SAMPLES = 8;
constexpr uint8_t FLAME_CONFIRM_SAMPLES = 3;
constexpr unsigned long SAMPLE_PERIOD_MS = 250UL;
constexpr unsigned long SENSOR_STABILIZE_MS = 60000UL;
constexpr unsigned long SMS_MIN_INTERVAL_MS = 60000UL;
constexpr unsigned long SMS_RETRY_INTERVAL_MS = 10000UL;

// The M660 module itself commonly defaults to 115200 baud, while Arduino
// carrier-board examples are often configured for 9600. Use the baud rate of
// the specific board. SoftwareSerial on an Uno is most reliable at 9600.
constexpr long GSM_BAUD = 9600;

// Replace with the required international-format destination number.
const char ALERT_PHONE_NUMBER[] = "+000000000000";

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);

bool alarmActive = false;
bool gsmReady = false;
bool smsSentForCurrentAlarm = false;
uint8_t flameActiveCount = 0;
unsigned long lastSampleAt = 0;
unsigned long lastSmsAttemptAt = 0;
unsigned long lastSmsSentAt = 0;

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------
void setRelay(bool enabled) {
  const uint8_t activeLevel = RELAY_ACTIVE_LOW ? LOW : HIGH;
  const uint8_t inactiveLevel = RELAY_ACTIVE_LOW ? HIGH : LOW;
  digitalWrite(FAN_RELAY_PIN, enabled ? activeLevel : inactiveLevel);
}

void setAlarmOutputs(bool enabled) {
  digitalWrite(RED_LED_PIN, enabled ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, enabled ? LOW : HIGH);
  setRelay(enabled);

  if (enabled) {
    tone(BUZZER_PIN, 2200);
  } else {
    noTone(BUZZER_PIN);
  }
}

uint16_t readAveragedGasLevel() {
  uint32_t total = 0;
  for (uint8_t i = 0; i < GAS_AVERAGE_SAMPLES; ++i) {
    total += analogRead(GAS_SENSOR_PIN);
    delay(2);
  }
  return static_cast<uint16_t>(total / GAS_AVERAGE_SAMPLES);
}

bool readConfirmedFlame() {
  const bool rawActive = FLAME_ACTIVE_LOW
                           ? (digitalRead(FLAME_SENSOR_PIN) == LOW)
                           : (digitalRead(FLAME_SENSOR_PIN) == HIGH);

  if (rawActive) {
    if (flameActiveCount < FLAME_CONFIRM_SAMPLES) {
      ++flameActiveCount;
    }
  } else {
    flameActiveCount = 0;
  }

  return flameActiveCount >= FLAME_CONFIRM_SAMPLES;
}

void clearGsmInput() {
  while (gsmSerial.available() > 0) {
    gsmSerial.read();
  }
}

bool readGsmResponse(const char *expected, unsigned long timeoutMs) {
  char response[96];
  size_t length = 0;
  response[0] = '\0';
  const unsigned long startedAt = millis();

  while (millis() - startedAt < timeoutMs) {
    while (gsmSerial.available() > 0) {
      const char value = static_cast<char>(gsmSerial.read());
      Serial.write(value);

      if (length < sizeof(response) - 1) {
        response[length++] = value;
        response[length] = '\0';
      } else {
        memmove(response, response + 32, sizeof(response) - 33);
        length = sizeof(response) - 33;
        response[length++] = value;
        response[length] = '\0';
      }

      if (strstr(response, expected) != nullptr) {
        return true;
      }
      if (strstr(response, "ERROR") != nullptr) {
        return false;
      }
    }
  }
  return false;
}

bool sendAtCommand(const __FlashStringHelper *command,
                   const char *expected,
                   unsigned long timeoutMs) {
  clearGsmInput();
  gsmSerial.println(command);
  return readGsmResponse(expected, timeoutMs);
}

bool isNetworkRegistered() {
  clearGsmInput();
  gsmSerial.println(F("AT+CREG?"));

  char response[96];
  size_t length = 0;
  response[0] = '\0';
  const unsigned long startedAt = millis();

  while (millis() - startedAt < 1500UL) {
    while (gsmSerial.available() > 0) {
      const char value = static_cast<char>(gsmSerial.read());
      Serial.write(value);
      if (length < sizeof(response) - 1) {
        response[length++] = value;
        response[length] = '\0';
      }
    }
  }

  return strstr(response, ",1") != nullptr || strstr(response, ",5") != nullptr;
}

bool initializeGsm() {
  for (uint8_t attempt = 0; attempt < 5; ++attempt) {
    if (sendAtCommand(F("AT"), "OK", 1200UL)) {
      break;
    }
    if (attempt == 4) {
      return false;
    }
    delay(600);
  }

  if (!sendAtCommand(F("ATE0"), "OK", 1500UL)) {
    return false;
  }
  if (!sendAtCommand(F("AT+CMGF=1"), "OK", 2000UL)) {
    return false;
  }

  // Network registration can take time. Local protection remains operational
  // even when this check fails.
  for (uint8_t attempt = 0; attempt < 20; ++attempt) {
    if (isNetworkRegistered()) {
      return true;
    }
    delay(1000);
  }
  return false;
}

bool sendAlarmSms(uint16_t gasLevel, bool flameDetected) {
  if (!gsmReady) {
    return false;
  }

  if (!sendAtCommand(F("AT+CMGF=1"), "OK", 2000UL)) {
    return false;
  }

  clearGsmInput();
  gsmSerial.print(F("AT+CMGS=\""));
  gsmSerial.print(ALERT_PHONE_NUMBER);
  gsmSerial.println(F("\""));
  if (!readGsmResponse(">", 3000UL)) {
    return false;
  }

  gsmSerial.print(F("SAFETY ALERT: "));
  if (flameDetected && gasLevel >= GAS_ALARM_ON_THRESHOLD) {
    gsmSerial.print(F("flame and combustible gas detected"));
  } else if (flameDetected) {
    gsmSerial.print(F("flame detected"));
  } else {
    gsmSerial.print(F("combustible gas detected"));
  }
  gsmSerial.print(F(". Raw gas level: "));
  gsmSerial.print(gasLevel);
  gsmSerial.write(26);  // Ctrl+Z submits the SMS.

  return readGsmResponse("+CMGS:", 15000UL);
}

// ---------------------------------------------------------------------------
// User interface and alarm state machine
// ---------------------------------------------------------------------------
void displayNormal(uint16_t gasLevel) {
  lcd.setCursor(0, 0);
  lcd.print(F("Home safety     "));
  lcd.setCursor(0, 1);
  lcd.print(F("Gas level:"));
  lcd.print(gasLevel);
  lcd.print(F("    "));
}

void displayAlarm(uint16_t gasLevel, bool flameDetected) {
  lcd.setCursor(0, 0);
  if (flameDetected && gasLevel >= GAS_ALARM_ON_THRESHOLD) {
    lcd.print(F("GAS + FLAME!    "));
  } else if (flameDetected) {
    lcd.print(F("FLAME ALARM!    "));
  } else {
    lcd.print(F("GAS ALARM!      "));
  }

  lcd.setCursor(0, 1);
  lcd.print(F("Gas level:"));
  lcd.print(gasLevel);
  lcd.print(F("    "));
}

void updateAlarmState(uint16_t gasLevel, bool flameDetected) {
  const bool gasDetected = alarmActive
                             ? gasLevel >= GAS_ALARM_OFF_THRESHOLD
                             : gasLevel >= GAS_ALARM_ON_THRESHOLD;
  const bool hazardDetected = gasDetected || flameDetected;

  if (hazardDetected) {
    displayAlarm(gasLevel, flameDetected);

    if (!alarmActive) {
      alarmActive = true;
      setAlarmOutputs(true);
    }

    // Retry a failed notification without flooding the recipient. The local
    // alarm and fan remain active while the modem is unavailable.
    const unsigned long now = millis();
    const bool retryDue = lastSmsAttemptAt == 0 ||
                          now - lastSmsAttemptAt >= SMS_RETRY_INTERVAL_MS;
    const bool rateLimitClear = lastSmsSentAt == 0 ||
                                now - lastSmsSentAt >= SMS_MIN_INTERVAL_MS;
    if (!smsSentForCurrentAlarm && gsmReady && retryDue && rateLimitClear) {
      lastSmsAttemptAt = now;
      if (sendAlarmSms(gasLevel, flameDetected)) {
        smsSentForCurrentAlarm = true;
        lastSmsSentAt = now;
        Serial.println(F("SMS alert sent."));
      } else {
        Serial.println(F("SMS alert failed; local alarm remains active."));
      }
    }
  } else {
    if (alarmActive) {
      alarmActive = false;
      smsSentForCurrentAlarm = false;
      lastSmsAttemptAt = 0;
      setAlarmOutputs(false);
      Serial.println(F("Alarm cleared after sensor recovery."));
    }
    displayNormal(gasLevel);
  }
}

void setup() {
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);

  setAlarmOutputs(false);

  Serial.begin(115200);
  gsmSerial.begin(GSM_BAUD);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("Safety monitor"));
  lcd.setCursor(0, 1);
  lcd.print(F("Starting..."));

  gsmReady = initializeGsm();
  Serial.println(gsmReady ? F("GSM registered.")
                          : F("GSM unavailable; local safety remains active."));

  lcd.clear();
  lcd.print(F("Sensor warm-up"));
}

void loop() {
  const unsigned long now = millis();
  if (now - lastSampleAt < SAMPLE_PERIOD_MS) {
    return;
  }
  lastSampleAt = now;

  const uint16_t gasLevel = readAveragedGasLevel();
  const bool flameDetected = readConfirmedFlame();

  Serial.print(F("Gas raw="));
  Serial.print(gasLevel);
  Serial.print(F(" flame="));
  Serial.println(flameDetected ? F("DETECTED") : F("clear"));

  if (now < SENSOR_STABILIZE_MS && !flameDetected && !alarmActive) {
    lcd.setCursor(0, 0);
    lcd.print(F("Sensor warm-up  "));
    lcd.setCursor(0, 1);
    lcd.print(F("Gas level:"));
    lcd.print(gasLevel);
    lcd.print(F("    "));
    return;
  }

  // During MQ-5 stabilization, ignore its unconditioned gas value but retain
  // immediate flame protection.
  updateAlarmState(now < SENSOR_STABILIZE_MS ? 0 : gasLevel, flameDetected);
}
