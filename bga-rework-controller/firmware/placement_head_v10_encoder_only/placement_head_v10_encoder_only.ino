/*
 * SV560 In-Head Pickup — ESP32 controller v10 (ENCODER ONLY)
 *
 * ONLY the encoder is used - no separate POS button, no switches.
 * Simplest possible control for manual placement.
 *
 * CONTROLS:
 *   Encoder rotate         -> jog Z up/down (slow=fine, fast=coarse)
 *   Encoder tap            -> toggle vacuum on/off
 *   Encoder double-tap     -> go to ALIGN preset height
 *   Encoder hold           -> release (vent valve + pump off)
 *   Encoder long-hold (3s) -> re-zero at current position
 *
 * Manual placement (CN750 spring cushions contact):
 *   1. Encoder tap = vacuum on, pick the chip
 *   2. Encoder double-tap = go to ALIGN, align on split vision
 *   3. Encoder turn = jog down slowly, watch CN750 spring compress, stop
 *   4. Encoder hold = release
 *   5. Jog back up
 *
 * Startup: hold the encoder button for SELF-TEST.
 *
 * Libraries: AccelStepper, MAX6675 (Adafruit), U8g2
 */

#include <AccelStepper.h>
#include <max6675.h>
#include <U8g2lib.h>
#include <SPI.h>

// ---------------- PIN MAP ----------------
#define PIN_STEP        26
#define PIN_DIR         27
#define PIN_EN          14
#define PIN_PUMP        32
#define PIN_VALVE       33
#define PIN_VAC_ADC     34
#define PIN_TC_SCK      5
#define PIN_TC_CS       4
#define PIN_TC_SO       15
#define PIN_ENC_A       25
#define PIN_ENC_B       13   // D13 - D2 read B unreliably (both dirs went negative)
#define PIN_ENC_SW      12   // the ONLY button - the encoder push
#define PIN_OLED_CLK    18
#define PIN_OLED_MOSI   19
#define PIN_OLED_RES    21
#define PIN_OLED_DC     22
#define PIN_OLED_CS     23
// Free: D35, D2, D16, D17, VN(39), VP(36). NOTE: D13 is NOT free - it is encoder B.

// ---------------- Tuning ----------------
const float STEPS_PER_MM    = 400.0;
const float POS_ALIGN_MM    = 60.0;
const float Z_TRAVEL_LIMIT  = 100.0;
const float JOG_FINE_MM     = 0.05;
const float JOG_COARSE_MM   = 0.5;
const float TRAVEL_FEED_MMS = 6.0;
const float JOG_FEED_MMS    = 3.0;
const int   TEMP_LOCKOUT_C  = 60;
const int   VAC_OK_RAW      = 1400;
const uint32_t HOLD_MS      = 700;    // hold threshold
const uint32_t LONGHOLD_MS  = 3000;   // long-hold = re-zero
const uint32_t DTAP_MS      = 350;    // double-tap window

AccelStepper z(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);
MAX6675 tc(PIN_TC_SCK, PIN_TC_CS, PIN_TC_SO);
U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI oled(U8G2_R0, PIN_OLED_CLK, PIN_OLED_MOSI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);

bool vacOn = false;
volatile int32_t encDelta = 0;
volatile int32_t encTotal = 0;
volatile uint32_t lastEncMs = 0;

// Encoder ISR: interrupt on A's FALLING edge, read B for direction.
// - Longer debounce filters contact bounce (was flipping direction randomly)
// - Small settle delay lets B stabilize before we read it (B is unreliable
//   if read right at its transition point)
#define ENC_DEBOUNCE_US 4000   // ignore edges closer than 4ms apart (more filtering)
void IRAM_ATTR encISR() {
  static uint32_t lastUs = 0;
  uint32_t now = micros();
  if (now - lastUs < ENC_DEBOUNCE_US) return;   // too soon = bounce, ignore
  lastUs = now;
  // tiny settle so B is stable (not caught mid-transition)
  delayMicroseconds(50);
  // A went low. Read B to decide direction.
  if (digitalRead(PIN_ENC_B)) { encDelta++; encTotal++; }
  else                        { encDelta--; encTotal--; }
  lastEncMs = millis();
}




float zMM()        { return z.currentPosition() / STEPS_PER_MM; }
float heaterC()    { return tc.readCelsius(); }
bool  heaterCold() { return heaterC() < TEMP_LOCKOUT_C; }
void  pump(bool s) { vacOn = s; digitalWrite(PIN_PUMP, s); }
void  vent(bool s) { digitalWrite(PIN_VALVE, s); }
void  setFeed(float mms){ z.setMaxSpeed(mms*STEPS_PER_MM); z.setAcceleration(3000); }
void  goTo(float mm, float feed){
  setFeed(feed);
  z.moveTo((long)(constrain(mm, -Z_TRAVEL_LIMIT, Z_TRAVEL_LIMIT)*STEPS_PER_MM));
}

// Advanced button read: returns
//  0 = nothing, 1 = single tap, 2 = double tap, 3 = hold, 4 = long-hold
int readEncoderButton() {
  if (digitalRead(PIN_ENC_SW) != LOW) return 0;
  uint32_t t0 = millis();
  // wait through the press, timing it
  while (digitalRead(PIN_ENC_SW) == LOW) {
    if (millis() - t0 > LONGHOLD_MS) { while(digitalRead(PIN_ENC_SW)==LOW) delay(5); return 4; }
    delay(5);
  }
  uint32_t held = millis() - t0;
  if (held > HOLD_MS) return 3;   // hold

  // was a short tap - watch for a second tap (double)
  uint32_t t1 = millis();
  while (millis() - t1 < DTAP_MS) {
    if (digitalRead(PIN_ENC_SW) == LOW) {
      // second press detected
      while (digitalRead(PIN_ENC_SW) == LOW) delay(5);
      return 2;   // double tap
    }
    delay(5);
  }
  return 1;   // single tap
}

void doRelease() {
  if (z.isRunning()) return;
  vent(true); delay(120); pump(false); delay(300); vent(false);
}

// ---------- Diagnostics (hold encoder at startup) ----------
void runDiagnostics() {
  Serial.println("=== DIAGNOSTIC MODE ===");
  int page = 0; const int NUM_PAGES = 5;
  while (true) {
    int b = readEncoderButton();
    if (b == 3 || b == 4) { Serial.println("Exit diag."); return; }  // hold to exit
    if (b == 1) page = (page + 1) % NUM_PAGES;                        // tap = next

    oled.clearBuffer(); oled.setFont(u8g2_font_6x10_tr);
    char l[26];
    switch (page) {
      case 0:
        oled.setFont(u8g2_font_7x13B_tr); oled.drawStr(0,12,"DIAGNOSTICS");
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(0,28,"enc tap = next");
        oled.drawStr(0,40,"enc hold = exit");
        oled.drawStr(0,56,"Page 1/5"); break;
      case 1: {
        int v=analogRead(PIN_VAC_ADC);
        oled.drawStr(0,10,"VACUUM (2/5)");
        snprintf(l,sizeof(l),"ADC: %d",v); oled.drawStr(0,28,l);
        oled.drawStr(0,44,(v<50||v>4050)?"!! check 3V3":"suck=drops"); break; }
      case 2: {
        float t=heaterC();
        oled.drawStr(0,10,"THERMOCOUPLE(3/5)");
        if(isnan(t)) oled.drawStr(0,28,"nan !!check wire");
        else { snprintf(l,sizeof(l),"Temp: %.1f C",t); oled.drawStr(0,28,l);} break; }
      case 3:
        oled.drawStr(0,10,"ENCODER (4/5)");
        snprintf(l,sizeof(l),"Count: %ld",(long)encTotal); oled.drawStr(0,28,l);
        oled.drawStr(0,44,"turn = changes"); break;
      case 4:
        oled.drawStr(0,10,"PUMP/MOTOR (5/5)");
        oled.drawStr(0,26,"double-tap=test");
        snprintf(l,sizeof(l),"Z: %.2f",zMM()); oled.drawStr(0,44,l);
        if (b == 2) {
          oled.sendBuffer();
          Serial.println("pump 1s"); pump(true); delay(1000); pump(false); delay(300);
          Serial.println("valve 1s"); vent(true); delay(1000); vent(false); delay(300);
          Serial.println("jog+3"); goTo(zMM()+3,JOG_FEED_MMS); while(z.isRunning())z.run();
          delay(300); Serial.println("jog-3"); goTo(zMM()-3,JOG_FEED_MMS); while(z.isRunning())z.run();
        } break;
    }
    oled.sendBuffer();
    static uint32_t t=0;
    if(millis()-t>500){t=millis();
      Serial.printf("VAC=%d TEMP=%.1f ENC=%ld\n",analogRead(PIN_VAC_ADC),heaterC(),(long)encTotal);}
    delay(20);
  }
}

void drawOLED() {
  oled.clearBuffer(); oled.setFont(u8g2_font_7x13B_tr);
  oled.drawStr(0,12,"MANUAL");
  oled.setFont(u8g2_font_6x10_tr);
  char line[24];
  snprintf(line,sizeof(line),"Z: %.2f mm",zMM()); oled.drawStr(0,28,line);
  snprintf(line,sizeof(line),"Vac: %d",analogRead(PIN_VAC_ADC)); oled.drawStr(0,40,line);
  float t=heaterC();
  if(isnan(t)) snprintf(line,sizeof(line),"Temp: --");
  else snprintf(line,sizeof(line),"Temp: %.0f C",t);
  oled.drawStr(0,52,line);
  snprintf(line,sizeof(line),"%s",vacOn?"VACUUM ON":"vac off"); oled.drawStr(0,63,line);
  oled.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_EN,OUTPUT); digitalWrite(PIN_EN,LOW);
  pinMode(PIN_PUMP,OUTPUT); pinMode(PIN_VALVE,OUTPUT);
  pinMode(PIN_ENC_A,INPUT_PULLUP); pinMode(PIN_ENC_B,INPUT_PULLUP);
  pinMode(PIN_ENC_SW,INPUT_PULLUP);
  attachInterrupt(PIN_ENC_A, encISR, FALLING);   // interrupt on A's falling edge only
  // (B is read inside the ISR to determine direction - no interrupt needed on B)

  z.setCurrentPosition(0);

  oled.begin();
  oled.clearBuffer(); oled.setFont(u8g2_font_7x13B_tr);
  oled.drawStr(0,14,"v10 ENC-ONLY");
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(0,32,"Hold enc = SELFTEST");
  oled.drawStr(0,46,"Else = manual");
  oled.drawStr(0,60,"(2 sec...)");
  oled.sendBuffer();
  Serial.println("v10 encoder-only. Hold enc for self-test.");

  uint32_t t0=millis(); bool diag=false;
  while(millis()-t0<2000){ if(digitalRead(PIN_ENC_SW)==LOW){diag=true;break;} delay(10);}
  if(diag){
    oled.clearBuffer(); oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(0,30,"Release to enter"); oled.sendBuffer();
    while(digitalRead(PIN_ENC_SW)==LOW) delay(10);
    delay(200);
    runDiagnostics();
  }

  oled.clearBuffer(); oled.setFont(u8g2_font_7x13B_tr);
  oled.drawStr(0,20,"Manual ready");
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(0,38,"turn=jog tap=vac");
  oled.drawStr(0,52,"2tap=align hold=rel");
  oled.sendBuffer();
  Serial.println("Manual. turn=jog, tap=vac, 2tap=align, hold=release, long=rezero.");
}

void loop() {
  int b = readEncoderButton();
  if (b == 1)      pump(!vacOn);                    // tap = vacuum toggle
  else if (b == 2) {                                // double-tap = go to ALIGN
    if (!heaterCold()) Serial.println("BLOCKED: hot");
    else { goTo(POS_ALIGN_MM, TRAVEL_FEED_MMS); while(z.isRunning()) z.run(); }
  }
  else if (b == 3) doRelease();                     // hold = release
  else if (b == 4) { z.setCurrentPosition(0); Serial.println("rezeroed"); }  // long-hold = re-zero

  // Encoder rotate = jog
  if (encDelta != 0) {
    noInterrupts(); int32_t d = encDelta; encDelta = 0; uint32_t le = lastEncMs; interrupts();
    static uint32_t prev = 0;
    float step = (le - prev < 40) ? JOG_COARSE_MM : JOG_FINE_MM;
    prev = le;
    float tgt = zMM() + step * d;
    if (tgt < -2 && !heaterCold()) Serial.println("BLOCKED: hot");
    else goTo(tgt, JOG_FEED_MMS);
  }

  if (z.isRunning()) z.run();

  static uint32_t t = 0;
  if (millis() - t > 250) { t = millis(); drawOLED(); }
}
