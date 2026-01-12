/* maj_utilisé le 02/01/2026 */
/* Pédales fixées avec plages personnalisées + boutons + boîte H */

#include <Joystick.h>

// --- Création du joystick HID ---
Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_GAMEPAD,
  8, 0,             // 8 boutons au total : 2 standards + 6 vitesses
  true, true, true, // X, Y, Z axes activés
  true, false, false,
  false, false, false, false, false
);

// --- Broches potentiomètres ---
const int potVolant = A0; // Volant
const int potAccel  = A3; // Accélérateur
const int potFrein  = A1; // Frein
const int potEmbr   = A2; // Embrayage / rotation X

// --- Broches boutons standards ---
const int buttonLeft  = 2; // bouton joystick n°0
const int buttonRight = 3; // bouton joystick n°1

// --- Broches boîte H (6 boutons) ---
const int gearPins[6] = {4, 5, 6, 7, 8, 9};

// --- Valeur centrale du volant ---
const int centreVolant = 512;

// --- Lissage EMA ---
float accelSmooth = 0;
float freinSmooth = 0;
float embrSmooth  = 0;
const float alpha = 0.15;

// --- Plages fixes ---
const int accelMin = 890;
const int accelMax = 1023;

const int freinMin = 890;
const int freinMax = 1023;

const int embrMin  = 890;
const int embrMax  = 1023;

// --- Lecture moyenne anti-bruit ---
int readSmooth(int pin) {
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += analogRead(pin);
  return sum / 5;
}

// --- Mapping pédales vers joystick ---
int mapPedale(int val, int minVal, int maxVal) {
  if (val > maxVal) val = maxVal;
  if (val < minVal) val = minVal;
  float ratio = (float)(maxVal - val) / (float)(maxVal - minVal);
  return ratio * 32767;
}

void setup() {
  Serial.begin(115200);

  Joystick.begin();
  Joystick.setXAxisRange(-32767, 32767); // Volant
  Joystick.setYAxisRange(0, 32767);      // Accélérateur
  Joystick.setZAxisRange(0, 32767);      // Frein
  Joystick.setRxAxisRange(0, 32767);     // Embrayage / rotation X

  // --- Initialisation boutons ---
  pinMode(buttonLeft, INPUT_PULLUP);
  pinMode(buttonRight, INPUT_PULLUP);

  // --- Initialisation boîte H ---
  for (int i = 0; i < 6; i++) pinMode(gearPins[i], INPUT_PULLUP);

  Serial.println("Configuration pédales + boutons + boîte H prête");
}

void loop() {
  // --- Lecture des potentiomètres ---
  int valVolant = readSmooth(potVolant);
  int valAccel  = readSmooth(potAccel);
  int valFrein  = readSmooth(potFrein);
  int valEmbr   = readSmooth(potEmbr);

  // --- Volant (X) avec limitation ±2% autour du centre ---
  float normVolant = (float)(valVolant - centreVolant) / 511.0;
  if (abs(normVolant) < 0.02) normVolant = 0;
  if (normVolant > 1.0) normVolant = 1.0;
  if (normVolant < -1.0) normVolant = -1.0;
  float sign = (normVolant >= 0) ? 1.0 : -1.0;
  float expo = 1.0; // exponentiel linéaire
  float volantMapped = sign * pow(abs(normVolant), expo) * 32767.0;

  // --- Mapping des pédales + lissage EMA ---
  int accelMapped = mapPedale(valAccel, accelMin, accelMax);
  int freinMapped = mapPedale(valFrein, freinMin, freinMax);
  int embrMapped  = mapPedale(valEmbr, embrMin, embrMax);

  accelSmooth = alpha * accelMapped + (1 - alpha) * accelSmooth;
  freinSmooth = alpha * freinMapped + (1 - alpha) * freinSmooth;
  embrSmooth  = alpha * embrMapped  + (1 - alpha) * embrSmooth;

  // --- Envoi vers le joystick ---
  Joystick.setXAxis((int)volantMapped);
  Joystick.setYAxis((int)accelSmooth);
  Joystick.setZAxis((int)freinSmooth);
  Joystick.setRxAxis((int)embrSmooth);

  // --- Boutons standards ---
  Joystick.setButton(0, digitalRead(buttonLeft)  == LOW ? 1 : 0);
  Joystick.setButton(1, digitalRead(buttonRight) == LOW ? 1 : 0);

  // --- Lecture boîte H (6 vitesses) ---
  int gearDetected = 0;
  for (int i = 0; i < 6; i++) {
    if (digitalRead(gearPins[i]) == LOW) gearDetected = i + 1;
  }

  // --- Mise à jour des boutons de vitesses ---
  for (int i = 0; i < 6; i++) {
    Joystick.setButton(i + 2, (gearDetected == (i + 1)) ? 1 : 0);
  }

  // --- Debug série ---
  Serial.print("Volant : "); Serial.print(valVolant); Serial.print(" | ");
  Serial.print("Accel : ");  Serial.print(valAccel);  Serial.print(" | ");
  Serial.print("Frein : ");  Serial.print(valFrein);  Serial.print(" | ");
  Serial.print("Embr : ");   Serial.print(valEmbr);   Serial.print(" | ");
  if (gearDetected == 0) Serial.println("Point mort (N)");
  else {
    Serial.print("Vitesse : "); Serial.println(gearDetected);
  }

  delay(10); // ~100 Hz
}
