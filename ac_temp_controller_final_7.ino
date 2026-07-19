/*
 * Automatic Air Conditioner Controller - FINAL VERSION
 * Arduino Uno + LM35 + IR LED (via BC547) -> Mitsubishi Heavy Industries AC
 *
 * Sensor: LM35 analog temperature sensor (NOT DHT11 - no library needed)
 *   LM35 Pin 1 (left, flat side facing you) -> 5V
 *   LM35 Pin 2 (middle)                      -> A0
 *   LM35 Pin 3 (right)                       -> GND
 *
 * IR: raw captured codes from the user's actual Mitsubishi Heavy remote
 * (captured via KY-022 IR receiver module + Arduino-IRremote library),
 * guaranteeing compatibility with this exact AC unit (SRK25ZS-W).
 *
 * Captured commands:
 *   ON  -> sets AC to: Cool mode, 21C, full fan speed
 *   OFF -> turns AC off
 *
 * Library required: IRremote (by Armin Joachimsmeyer / Arduino-IRremote)
 * No DHT library needed.
 */

// ---------- IR send pin configuration ----------
#define IR_SEND_PIN 3   // Same pin your BC547 + IR LED driver is wired to
#include <IRremote.hpp>

// ---------- LM35 configuration ----------
#define LM35_PIN A0

// ---------- Temperature thresholds (hysteresis) ----------
const float TEMP_ON  = 24.0;  // Turn AC ON when temp rises above this
const float TEMP_OFF = 21.0;  // Turn AC OFF when temp falls below this
// NOTE: AC will cool to 21C / full fan, since that's what was captured.
// To change this, re-capture the ON code with the remote set to your
// desired temp/fan speed and replace irDataOn[] below.

// ---------- Timing ----------
const unsigned long READ_INTERVAL_MS = 1000;      // How often to read temperature
const unsigned long MIN_CMD_INTERVAL_MS = 60000;  // Don't resend IR more than once/min
const unsigned long REASSERT_INTERVAL_MS = 100000; // 5 min: re-send ON periodically while
                                                    // still above OFF threshold, in case
                                                    // someone turned the AC off with the
                                                    // original remote and our tracked
                                                    // state (acIsOn) is now out of sync.

bool acIsOn = false;
unsigned long lastRead = 0;
unsigned long lastCommand = 0;

// ---------- Captured raw IR codes (Mitsubishi Heavy, 232 bits, LSB first) ----------
// Power ON, Cool, 21C, full fan
uint32_t irDataOn[] = {0x1AC3AE52, 0xFB09F6E5, 0xDF04FB04, 0xF728D720,
                        0xFF807F08, 0xFF00FF00, 0xBF38C700, 0x40};

// Power OFF
uint32_t irDataOff[] = {0x1AC3AE52, 0xFB01FEE5, 0xDF04FB04, 0xF728D720,
                         0xFF807F08, 0xFF00FF00, 0xBF3AC500, 0x40};

// ---------- Calibration ----------
// If your LM35 reads consistently too low/high compared to a real thermometer,
// adjust this offset. Currently set to +4.0 based on user's comparison.
const float TEMP_CALIBRATION_OFFSET = 4.0;

float readTemperature() {
  int reading = analogRead(LM35_PIN);
  float voltage = reading * (5.0 / 1023.0);
  float tempC = voltage * 100.0;  // LM35: 10mV per degree C
  tempC += TEMP_CALIBRATION_OFFSET;
  Serial.print(F("  [debug] raw ADC: "));
  Serial.print(reading);
  Serial.print(F("  voltage: "));
  Serial.println(voltage, 3);
  return tempC;
}

// How many times to repeat each IR send, and the gap between repeats.
// Helps overcome weak range / marginal reception.
const uint8_t IR_REPEAT_COUNT = 4;
const unsigned long IR_REPEAT_GAP_MS = 100;

void sendAcOn() {
  for (uint8_t i = 0; i < IR_REPEAT_COUNT; i++) {
    IrSender.sendPulseDistanceWidthFromArray(38, 3200, 1500, 350, 1200, 350, 450,
                                              &irDataOn[0], 232,
                                              PROTOCOL_IS_LSB_FIRST, 0, 0);
    delay(IR_REPEAT_GAP_MS);
  }
  Serial.println(F(">> Sent AC ON (Cool, 21C, full fan) x4"));
}

void sendAcOff() {
  for (uint8_t i = 0; i < IR_REPEAT_COUNT; i++) {
    IrSender.sendPulseDistanceWidthFromArray(38, 3250, 1500, 350, 1200, 350, 450,
                                              &irDataOff[0], 232,
                                              PROTOCOL_IS_LSB_FIRST, 0, 0);
    delay(IR_REPEAT_GAP_MS);
  }
  Serial.println(F(">> Sent AC OFF x4"));
}

void setup() {
  Serial.begin(115200);
  IrSender.begin(IR_SEND_PIN);

  Serial.println(F("AC temperature controller starting..."));
  Serial.print(F("ON threshold: "));  Serial.print(TEMP_ON);  Serial.println(F(" C"));
  Serial.print(F("OFF threshold: ")); Serial.print(TEMP_OFF); Serial.println(F(" C"));
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead >= READ_INTERVAL_MS) {
    lastRead = now;

    float temp = readTemperature();

    Serial.print(F("Temperature: "));
    Serial.print(temp);
    Serial.println(F(" C"));

    bool cooldownOver = (now - lastCommand >= MIN_CMD_INTERVAL_MS) || (lastCommand == 0);

    if (!acIsOn && temp >= TEMP_ON && cooldownOver) {
      sendAcOn();
      acIsOn = true;
      lastCommand = now;
    } else if (acIsOn && temp <= TEMP_OFF && cooldownOver) {
      sendAcOff();
      acIsOn = false;
      lastCommand = now;
    } else if (acIsOn && temp > TEMP_OFF && (now - lastCommand >= REASSERT_INTERVAL_MS)) {
      // Still want cooling, and enough time has passed - re-send ON in case the
      // AC was turned off manually (e.g. with the original remote) since our
      // last command, so it gets switched back on.
      sendAcOn();
      lastCommand = now;
    }
    // Between TEMP_OFF and TEMP_ON: no threshold-crossing action (hysteresis band),
    // but the periodic reassert above still applies if acIsOn is true.
  }
}
