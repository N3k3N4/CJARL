/* maj_utilisé le 25/09/2025 */
/* Version : boutons joystick standard + boite H + limitation volant */

#include <Joystick.h>

// --- Création du joystick HID ---
// 8 boutons au total : 2 standards + 6 vitesses
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID,
                   JOYSTICK_TYPE_GAMEPAD,
                   8, 0,             // <<< 8 boutons activés
                   true, true, true, // activer X, Y, Z
                   false, false, false,
                   false, false, false, false, false);

// --- Broches potentiomètres ---
const int potVolant = A0;   // Volant
const int potAccel  = A1;   // Accélérateur
const int potFrein  = A2;   // Frein

// --- Broches boutons simples ---
const int buttonLeft  = 2;   // Bouton joystick n°0
const int buttonRight = 3;   // Bouton joystick n°1

// --- Broches boîte H (6 boutons) ---
const int gearPins[6] = {4, 5, 6, 7, 8, 9}; // 1 -> 6

// --- Plages réelles pour les pédales ---
const int accelMin = 550;
const int accelMax = 860;
const int freinMin = 660;
const int freinMax = 380; // inversé

// --- Valeur centrale réelle du volant ---
const int centreVolant = 512;

// --- Variables pour lissage exponentiel ---
float accelSmooth  = 0;
float freinSmooth  = 0;
const float alpha = 0.1; // coefficient de lissage pour pédales

// --- Fonction lecture moyenne ---
int readSmooth(int pin) {
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += analogRead(pin);
  return sum / 5;
}

// --- Mapping personnalisé ---
int mapCustom(int val, int inMin, int inMax, int outMin, int outMax) {
  float ratio = (float)(val - inMin) / (float)(inMax - inMin);
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  return outMin + ratio * (outMax - outMin);
}

void setup() {
  Joystick.begin();
  Joystick.setXAxisRange(-32767, 32767); // Volant
  Joystick.setYAxisRange(0, 32767);      // Accélérateur
  Joystick.setZAxisRange(0, 32767);      // Frein

  // --- Initialisation des boutons standards ---
  pinMode(buttonLeft, INPUT_PULLUP);
  pinMode(buttonRight, INPUT_PULLUP);

  // --- Initialisation des boutons de vitesses ---
  for (int i = 0; i < 6; i++) {
    pinMode(gearPins[i], INPUT_PULLUP);
  }

  Serial.begin(9600);
}

void loop() {
  // --- Lecture des potentiomètres ---
  int valVolant = readSmooth(potVolant);
  int valAccel  = readSmooth(potAccel);
  int valFrein  = readSmooth(potFrein);

  // --- Volant avec limitation explicite ---
  float normVolant = (float)(valVolant - centreVolant) / 511.0; // [-1 .. +1]

  // --- Zone morte autour du centre ±2% ---
  if (abs(normVolant) < 0.02) normVolant = 0;

  // --- Limitation explicite des extrêmes ---
  if (normVolant > 1.0) normVolant = 1.0;
  if (normVolant < -1.0) normVolant = -1.0;

  float sign = (normVolant >= 0) ? 1.0 : -1.0;
  float expo = 1.0; // exponentiel linéaire, changer >1 pour plus de sensibilité au centre
  float volantMapped = sign * pow(abs(normVolant), expo) * 32767.0;

  // --- Pédales ---
  int accelMapped = mapCustom(valAccel, accelMin, accelMax, 0, 32767);
  int freinMapped = mapCustom(valFrein, freinMin, freinMax, 0, 32767);

  accelSmooth = alpha * accelMapped + (1 - alpha) * accelSmooth;
  freinSmooth = alpha * freinMapped + (1 - alpha) * freinSmooth;

  Joystick.setXAxis((int)volantMapped);
  Joystick.setYAxis((int)accelSmooth);
  Joystick.setZAxis((int)freinSmooth);

  // --- Boutons standards ---
  Joystick.setButton(0, digitalRead(buttonLeft)  == LOW ? 1 : 0);
  Joystick.setButton(1, digitalRead(buttonRight) == LOW ? 1 : 0);

  // --- Lecture des vitesses ---
  int gearDetected = 0;
  for (int i = 0; i < 6; i++) {
    if (digitalRead(gearPins[i]) == LOW) {
      gearDetected = i + 1;
    }
  }

  // --- Debug série ---
  if (gearDetected == 0) {
    Serial.println("Point mort (N)");
  } else {
    Serial.print("Vitesse : ");
    Serial.println(gearDetected);
  }

  // --- Mise à jour des boutons de vitesses ---
  for (int i = 0; i < 6; i++) {
    Joystick.setButton(i + 2, (gearDetected == (i + 1)) ? 1 : 0);
  }

  delay(10); // ~100 Hz
}
