# Arduino Uno Automatic Air Conditioner Controller

An Arduino Uno project that reads room temperature and automatically turns a
**Mitsubishi Heavy Industries** split-type air conditioner ON/OFF via IR,
mimicking the original remote control.

Built and tested against:
- Indoor unit: **SRK25ZS-W** (Service Code: SRK25ZS-W/A)
- Remote: **RLA502A704A**

> This approach (capturing raw IR codes from your own remote, rather than
> relying on a pre-built protocol library) should work for **any** brand/model
> of AC remote, not just this specific one. See "Why raw IR capture" below.

---

## What you'll need

| Part | Notes |
|---|---|
| Arduino Uno | |
| LM35 temperature sensor | TO-92 "transistor-shaped" package, 3 pins |
| IR LED | A dedicated 940nm IR LED is ideal; a salvaged clear LED from an old remote can work too if you confirm it's actually IR (see Troubleshooting) |
| BC547 NPN transistor | Used as a driver to give the IR LED enough current |
| Resistors | 1x ~1kΩ (transistor base), 1x 68–100Ω (LED anode - lower value = more range, see Troubleshooting) |
| IR receiver module | e.g. **KY-022**, TSOP38238, VS1838B, or HX1838 — only needed temporarily, for capturing your remote's codes |
| Breadboard + jumper wires | |
| Your AC's original remote | Needed once, to capture the IR codes |

---

## Why raw IR capture (not a pre-built AC library)

Libraries like `IRremoteESP8266` include built-in protocol definitions for many
AC brands, including some Mitsubishi Heavy models. In testing, two problems
came up that make **raw capture the more reliable approach** for a DIY build:

1. **Model mismatch risk.** AC brands often have several sub-families (e.g.
   different Mitsubishi Heavy model lines) with slightly different signal
   formats. Unless your exact model/remote combo is listed as tested by the
   library, there's a real chance the built-in protocol won't match.
2. **AVR (Uno) compatibility problems.** `IRremoteESP8266` is written for
   ESP8266/ESP32 boards, which have a full C++ standard library. On a plain
   Arduino Uno, several of its files fail to compile (`<algorithm>`, `<set>`,
   `std::min`/`std::max` colliding with Arduino's own macros, C++17 features
   like `std::underlying_type` not supported by the Uno's older compiler).
   There is no clean fix for this short of switching to an ESP8266/ESP32
   board.

Capturing your **actual** remote's raw IR timing and replaying it sidesteps
both problems entirely, and works on a plain Uno with the lightweight
`IRremote` library (no AC-brand-specific code, no STL issues).

The trade-off: raw playback replays *exactly* what you captured (e.g. "Cool,
21°C, full fan"), not a freely adjustable command. If you want a different
temperature/fan setting, capture that specific combination separately.

---

## Step 1 — Wire up the LM35 temperature sensor

Hold the LM35 with its **flat face toward you**, pins pointing down:

```
LM35 Pin 1 (left)   -> Arduino 5V
LM35 Pin 2 (middle) -> Arduino A0
LM35 Pin 3 (right)  -> Arduino GND
```

No pull-up resistor needed (that's a DHT-sensor thing, not LM35).

## Step 2 — Wire up the IR LED driver circuit

```
Arduino D3        -> 1kΩ resistor -> BC547 Base
BC547 Emitter     -> GND
BC547 Collector   -> IR LED Cathode (-)
Arduino 5V        -> 68-100Ω resistor -> IR LED Anode (+)
```

Lower resistor = more current = more range, but stay under the BC547's
100mA collector rating. Started at 220Ω in early testing; dropped to 100Ω
for better range.

## Step 3 — Confirm your salvaged LED is actually IR (optional, if reused from an old remote)

1. Wire the LED into the driver circuit above.
2. Upload a simple sketch that blinks pin D3 HIGH/LOW every second.
3. Watch it through your **phone's rear camera** live preview in a dark room.
   - Naked eye sees it blink → it's a normal visible LED, not IR.
   - Naked eye sees nothing, but the phone screen shows a faint purple/white
     glow blinking → confirmed IR.
4. Note: modern phones (especially newer iPhones) have stronger IR-cut
   filters and may show a fainter glow than older phones — inconclusive
   isn't the same as "not IR."

## Step 4 — Temporarily wire an IR receiver to capture your remote's codes

Using a KY-022 module (pinout varies by board — check silkscreen labels):

```
KY-022 S (signal) -> Arduino D4
KY-022 middle (5V) -> Arduino 5V
KY-022 - (GND)      -> Arduino GND
```

⚠️ Double-check your specific board's pin order before wiring — swapping
power and ground can damage the receiver.

## Step 5 — Install the IRremote library

In Arduino IDE: **Sketch → Include Library → Manage Libraries**, search
`IRremote`, install the one by **Armin Joachimsmeyer / Arduino-IRremote**.

Do **not** use `IRremoteESP8266` on a plain Uno — see "Why raw IR capture"
above for why it won't compile cleanly on AVR.

## Step 6 — Capture your remote's ON and OFF codes

1. **File → Examples → IRremote → SimpleReceiver**
2. Find this commented-out line and uncomment it (needed because AC remotes
   send much longer signals than typical TV remotes):
   ```cpp
   #define RAW_BUFFER_LENGTH  750
   ```
3. Set the receive pin to match your wiring. In this version of the example,
   the pin is set inside `PinDefinitionsAndMore.h`, which runs *after* any
   `#define` placed before the includes — the reliable way to override it is
   to `#undef` and redefine it **after** both includes:
   ```cpp
   #include "PinDefinitionsAndMore.h"
   #include <IRremote.hpp>

   #undef IR_RECEIVE_PIN
   #define IR_RECEIVE_PIN 4
   ```
4. Upload, open Serial Monitor at **115200 baud**.
5. **On your real remote**, set the mode/temperature/fan speed you want the
   Arduino to eventually turn the AC ON to (e.g. Cool, 24°C). Whatever is
   showing on the remote's screen is what gets captured — these AC remotes
   transmit their entire current state, not just "power button pressed."
6. Point the remote at the receiver (5–10cm away), press **Power ON**.
7. Copy the full output block from Serial Monitor, especially the line:
   ```cpp
   uint32_t tRawData[]={0x......, 0x......, ...};
   ```
   and the accompanying `IrSender.sendPulseDistanceWidthFromArray(...)` line
   (it tells you the exact timing parameters: carrier frequency, header
   mark/space, and one/zero mark/space durations — these vary slightly by
   protocol and you'll need them for sending).
8. Repeat the same process for **Power OFF**.

## Step 7 — Final combined sketch

See [`ac_temp_controller_final.ino`](./ac_temp_controller_final.ino) in this
repo. Key things to customize for your own build:

- `irDataOn[]` / `irDataOff[]` — replace with **your own** captured raw
  arrays from Step 6.
- The `sendPulseDistanceWidthFromArray(...)` parameters (carrier freq,
  header/one/zero timings) — copy these from your own captured output, they
  may differ slightly from the values used here.
- `TEMP_ON` / `TEMP_OFF` — your desired hysteresis thresholds (this build
  uses 24°C / 21°C).
- `TEMP_CALIBRATION_OFFSET` — see Calibration below.

### How the control logic works

- Reads temperature every `READ_INTERVAL_MS` (default 1 second).
- If temp rises to/above `TEMP_ON` and the AC is tracked as OFF → sends ON.
- If temp falls to/below `TEMP_OFF` and the AC is tracked as ON → sends OFF.
- Between the two thresholds: no action (hysteresis dead zone, prevents
  rapid on/off flickering right at one single threshold).
- Won't send more than one command per `MIN_CMD_INTERVAL_MS` (default 60s),
  to avoid spamming the AC from noisy/bouncing readings.
- Each ON/OFF command is sent **4 times** with a short gap between repeats
  (`IR_REPEAT_COUNT` / `IR_REPEAT_GAP_MS`), to improve reliability if your
  LED's range is marginal.
- Every `REASSERT_INTERVAL_MS` (default 5 minutes), if the Arduino still
  thinks the AC should be on, it re-sends the ON command even without a
  fresh threshold crossing. This recovers automatically if someone turns
  the AC off using the **original remote** — the Arduino has no way to
  detect that happened, so it just periodically re-asserts the state it
  wants.

---

## Calibration

LM35 accuracy depends heavily on how close your Arduino's 5V rail is to an
exact 5.00V — USB power commonly sags to 4.8–5.1V, which directly skews every
reading by a degree or more.

To calibrate:
1. Place a real thermometer (any basic room/kitchen thermometer) right next
   to the LM35 for a few minutes.
2. Compare its reading to what the Arduino reports (check the Serial Monitor
   debug output).
3. Adjust `TEMP_CALIBRATION_OFFSET` in the sketch to close the gap:
   ```cpp
   const float TEMP_CALIBRATION_OFFSET = 4.0; // adjust to your own sensor
   ```
   A flat offset like this works fine as long as the error is consistent
   across the temperature range. If the gap changes noticeably at different
   temperatures, the error may be a scaling issue rather than a simple
   offset — worth re-checking the 5V assumption in that case.

Phones do **not** have a built-in ambient temperature sensor — don't use a
"room temperature" app as your reference; those either show outdoor weather
data or a fake number.

---

## Troubleshooting

**`fatal error: DHT.h: No such file or directory`**
This project doesn't use DHT11/DHT.h at all — it uses LM35 (simple analog
read, no library needed). If you're adapting this for an actual DHT11
instead, install "DHT sensor library" by Adafruit + "Adafruit Unified
Sensor" via Library Manager.

**`fatal error: ir_MitsubishiHeavy.h` / `<set>` / `<algorithm>` / macro
redefinition errors when compiling `IRremoteESP8266`**
This library doesn't compile cleanly on a plain Arduino Uno — see "Why raw
IR capture" above. Don't try to patch around it; switch to the `IRremote`
library and the raw-capture approach in this repo instead (or move to an
ESP8266/ESP32 board if you specifically want the brand-protocol library).

**Sensor readings failing / "Failed to read from DHT sensor!"**
Make sure you actually have an LM35, not a DHT11 — they look different but
it's an easy mix-up if you're working from a mixed parts bin. LM35 is in a
black TO-92 "transistor" package with 3 legs; DHT11 has a rectangular
blue/white body with a visible grille.

**LM35 suddenly reads ~0°C**
Almost always a loose wire, not a sensor fault — LM35 output collapsing
toward 0V looks like this. Check for a jostled breadboard connection,
especially if the reading dropped right when you moved something near the
sensor.

**AC doesn't respond to ON/OFF signal**
- Confirm your raw capture actually completed (232 bits in this build's
  case — a short/truncated capture usually means `RAW_BUFFER_LENGTH` wasn't
  increased, see Step 6).
- Check IR LED range/aim — see the resistor/current notes in Step 2.
- Confirm the LED is genuinely IR, not a repurposed visible LED (Step 3).
- Try the increased `IR_REPEAT_COUNT` in the final sketch.

**Range is too short**
- Lower the LED's series resistor (down to ~68–100Ω, don't exceed the
  driver transistor's current rating).
- Consider a dedicated high-power IR LED (e.g. TSAL6200) instead of a
  salvaged one.
- Multiple IR LEDs in parallel (each with its own resistor) increases
  output.
- Aim matters — point directly at the AC's actual receiver window.

---

## Safety notes

- Don't drive the IR LED with more current than your BC547 (100mA max
  collector current) or the LED itself can handle.
- Get GND/VCC polarity right on the IR receiver module before powering on —
  reversed power can damage it.
- This project only sends IR commands; it has no feedback from the AC, so it
  can't confirm a command was actually received. Build in some tolerance for
  missed commands (repeats, periodic reassertion) rather than assuming every
  send succeeds.
