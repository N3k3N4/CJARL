/* maj_utilisé le 02/01/2026 */
/* Pédales et axes configurés sur plages fixes pour Joystick */

#include <Joystick.h>

// ======================
// CREATION DU JOYSTICK HID
// ======================
Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_GAMEPAD,
  8, 0,             // 8 boutons max
  true, true, true, // X, Y, Z axes activés
  true, false, false,
  false, false, false, false, false
);

// ======================
// BROCHES POTENTIOMETRES
// ======================
const int potVolant = A0; // volant
const int potAccel  = A3; // accélérateur
const int potFrein  = A1; // frein
const int potEmbr   = A2; // embrayage / rotation X

// ======================
// BOUTONS
// ======================
const int buttonLeft  = 2;
const int buttonRight = 3;

// ======================
// BOITE H
// ======================
const int gearPins[6] = {4, 5, 6, 7, 8, 9};

// ======================
// VOLANT
// ======================
const int centreVolant = 512;

// ======================
// LISSAGE EMA
// ======================
float accelSmooth = 0;
float freinSmooth = 0;
float embrSmooth  = 0;
const float alpha = 0.15; // filtre exponentiel

// ======================
// PLAGES FIXES
// ======================
const int accelMin = 963;
const int accelMax = 1023;

const int freinMin = 890;
const int freinMax = 1023;

const int embrMin  = 890;
const int embrMax  = 1023;

// ======================
// LECTURE MOYENNE ANTI-BRUIT
// ======================
int readSmooth(int pin) {
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += analogRead(pin);
  return sum / 5;
}

// ======================
// MAPPING SECURISE DES PEDALES
// ======================
int mapPedale(int val, int minVal, int maxVal) {
  if (val > maxVal) val = maxVal;
  if (val < minVal) val = minVal;
  float ratio = (float)(maxVal - val) / (float)(maxVal - minVal);
  return ratio * 32767;
}

void setup() {
  Serial.begin(115200);

  Joystick.begin();
  Joystick.setXAxisRange(-32767, 32767);
  Joystick.setYAxisRange(0, 32767);
  Joystick.setZAxisRange(0, 32767);
  Joystick.setRxAxisRange(0, 32767);

  pinMode(buttonLeft, INPUT_PULLUP);
  pinMode(buttonRight, INPUT_PULLUP);

  for (int i = 0; i < 6; i++) pinMode(gearPins[i], INPUT_PULLUP);

  Serial.println("Configuration des pédales et axes fixée pour Joystick");
}

void loop() {
  int valVolant = readSmooth(potVolant);
  int valAccel  = readSmooth(potAccel);
  int valFrein  = readSmooth(potFrein);
  int valEmbr   = readSmooth(potEmbr);

  // ======================
  // VOLANT (X)
  // ======================
  float normVolant = (float)(valVolant - centreVolant) / 511.0;
  int volantMapped = normVolant * 32767.0;

  // ======================
  // MAPPING DES PEDALES SUR PLAGE FIXE + LISSAGE EMA
  // ======================
  int accelMapped = mapPedale(valAccel, accelMin, accelMax);  // Y
  int freinMapped = mapPedale(valFrein, freinMin, freinMax); // Z
  int embrMapped  = mapPedale(valEmbr, embrMin, embrMax);     // Rx

  accelSmooth = alpha * accelMapped + (1 - alpha) * accelSmooth;
  freinSmooth = alpha * freinMapped + (1 - alpha) * freinSmooth;
  embrSmooth  = alpha * embrMapped  + (1 - alpha) * embrSmooth;

  // ======================
  // ENVOI VERS JOYSTICK
  // ======================
  Joystick.setXAxis(volantMapped);       // Volant
  Joystick.setYAxis((int)accelSmooth);   // Accélérateur
  Joystick.setZAxis((int)freinSmooth);   // Frein
  Joystick.setRxAxis((int)embrSmooth);   // Embrayage / Rotation X

  // ======================
  // BOUTONS
  // ======================
  Joystick.setButton(0, !digitalRead(buttonLeft));  // Inversion logique car INPUT_PULLUP
  Joystick.setButton(1, !digitalRead(buttonRight));

  // ======================
  // DEBUG SERIAL MONITOR
  // ======================
  Serial.print("Volant : "); Serial.print(valVolant); Serial.print(" | ");
  Serial.print("Accel : ");  Serial.print(valAccel);  Serial.print(" | ");
  Serial.print("Frein : ");  Serial.print(valFrein);  Serial.print(" | ");
  Serial.print("Embr : ");   Serial.println(valEmbr);

  delay(10);
}
