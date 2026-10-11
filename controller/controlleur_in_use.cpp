/*
  Simulateur de volant - Arduino Leonardo
  Version avec affichage dans le Serial Plotter (IDE Arduino > Outils > Traceur serie)
  Vitesse du moniteur/traceur : 115200 bauds

  PLOT_MODE 0 : valeurs brutes des potentiometres (0 a 1023)
  PLOT_MODE 1 : valeurs envoyees au PC en pourcentage
                (volant -100..+100, pedales 0..100)
*/

#include <Joystick.h>

#define PLOT_MODE 0

// --- Creation du joystick HID ---
Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_GAMEPAD,
  8, 0,
  true, true, true,
  true, false, false,
  false, false, false, false, false
);

// --- Broches potentiometres ---
const int potVolant = A0;
const int potAccel  = A3;
const int potFrein  = A1;
const int potEmbr   = A2;

// --- Broches boutons ---
const int buttonLeft  = 2;
const int buttonRight = 3;

// --- Broches boite H ---
const int gearPins[6] = {4, 5, 6, 7, 8, 9};

// =====================================================================
//                     REGLAGES DU VOLANT (modifiables)
// =====================================================================
// Relevez ces 3 valeurs avec le Serial Plotter en mode 0 (PLOT_MODE 0) :
//   centreVolant : valeur lue quand le volant est bien droit
//   volantMin    : valeur lue en butee gauche complete
//   volantMax    : valeur lue en butee droite complete
const int centreVolant = 512;
const int volantMin    = 0;
const int volantMax    = 1023;

// =====================================================================
//                    REGLAGES DES PEDALES (modifiables)
// =====================================================================
// Chaque pedale utilise 2 valeurs lues sur le potentiometre (0 a 1023) :
//
//   xxxMax = valeur lue quand la pedale est RELACHEE (au repos)
//   xxxMin = valeur lue quand la pedale est ENFONCEE A FOND
//
// ATTENTION : avec ce montage, la valeur DIMINUE quand on appuie.
//   Pedale relachee  -> valeur haute (ex : 1023)  -> sortie PC = 0 %
//   Pedale a fond    -> valeur basse (ex : 890)   -> sortie PC = 100 %
//
// COMMENT LES REGLER :
//   1. Mettez PLOT_MODE a 0 et ouvrez le Serial Plotter.
//   2. Pedale relachee : notez la valeur de la courbe -> c'est xxxMax.
//   3. Pedale enfoncee a fond : notez la valeur -> c'est xxxMin.
//   4. Gardez une petite marge (3 a 5 unites) pour atteindre bien 0 % et 100 %.
//   5. xxxMin doit TOUJOURS etre strictement inferieur a xxxMax.
//
// Si la pedale n'atteint jamais 100 %    -> augmentez xxxMin.
// Si la pedale reste a plus de 0 % au repos -> diminuez xxxMax.

// --- Accelerateur (broche A3) ---
const int accelMin = 890;    // pedale a fond
const int accelMax = 1023;   // pedale relachee

// --- Frein (broche A1) ---
const int freinMin = 890;    // pedale a fond
const int freinMax = 1023;   // pedale relachee

// --- Embrayage (broche A2) ---
const int embrMin  = 890;    // pedale a fond
const int embrMax  = 1023;   // pedale relachee

// =====================================================================
//                              LISSAGE
// =====================================================================
float volantSmooth = 0;
float accelSmooth  = 0;
float freinSmooth  = 0;
float embrSmooth   = 0;
const float alphaVolant = 0.30;  // 1.0 = aucun lissage, petit = plus lisse
const float alpha       = 0.15;  // pedales

int readSmooth(int pin) {
  int sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return sum / 8;
}

// Convertit la valeur lue (minVal..maxVal) en 0..32767 (inverse : bas = enfonce)
int mapPedale(int val, int minVal, int maxVal) {
  if (maxVal <= minVal) return 0;   // securite : evite la division par zero
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

  volantSmooth = readSmooth(potVolant);

  // IMPORTANT : aucun Serial.println de texte ici,
  // sinon le Serial Plotter affiche des courbes erronees.
}

void loop() {
  // --- Lecture ---
  int valVolant = readSmooth(potVolant);
  int valAccel  = readSmooth(potAccel);
  int valFrein  = readSmooth(potFrein);
  int valEmbr   = readSmooth(potEmbr);

  // --- Volant : sans zone morte, centre asymetrique ---
  volantSmooth = alphaVolant * valVolant + (1 - alphaVolant) * volantSmooth;
  float v = volantSmooth;

  float normVolant;
  if (v >= centreVolant) {
    normVolant = (v - centreVolant) / (float)(volantMax - centreVolant);
  } else {
    normVolant = (v - centreVolant) / (float)(centreVolant - volantMin);
  }
  if (normVolant > 1.0)  normVolant = 1.0;
  if (normVolant < -1.0) normVolant = -1.0;
  int volantMapped = (int)(normVolant * 32767.0);

  // --- Pedales ---
  int accelMapped = mapPedale(valAccel, accelMin, accelMax);
  int freinMapped = mapPedale(valFrein, freinMin, freinMax);
  int embrMapped  = mapPedale(valEmbr, embrMin, embrMax);

  accelSmooth = alpha * accelMapped + (1 - alpha) * accelSmooth;
  freinSmooth = alpha * freinMapped + (1 - alpha) * freinSmooth;
  embrSmooth  = alpha * embrMapped  + (1 - alpha) * embrSmooth;

  // --- Envoi joystick ---
  Joystick.setXAxis(volantMapped);
  Joystick.setYAxis((int)accelSmooth);
  Joystick.setZAxis((int)freinSmooth);
  Joystick.setRxAxis((int)embrSmooth);

  // --- Boutons ---
  Joystick.setButton(0, digitalRead(buttonLeft)  == LOW ? 1 : 0);
  Joystick.setButton(1, digitalRead(buttonRight) == LOW ? 1 : 0);

  // --- Boite H ---
  int gearDetected = 0;
  for (int i = 0; i < 6; i++) {
    if (digitalRead(gearPins[i]) == LOW) gearDetected = i + 1;
  }
  for (int i = 0; i < 6; i++) {
    Joystick.setButton(i + 2, (gearDetected == (i + 1)) ? 1 : 0);
  }

  // --- Sortie pour le Serial Plotter : "Nom:valeur,Nom:valeur" ---
#if PLOT_MODE == 0
  Serial.print("Volant:");   Serial.print(valVolant);
  Serial.print(",Accel:");   Serial.print(valAccel);
  Serial.print(",Frein:");   Serial.print(valFrein);
  Serial.print(",Embr:");    Serial.print(valEmbr);
  Serial.print(",Vitesse:"); Serial.print(gearDetected * 100);
  Serial.print(",Min:0,Max:1023");   // fixe l'echelle du graphique
  Serial.println();
#else
  Serial.print("Volant:");   Serial.print(normVolant * 100.0);
  Serial.print(",Accel:");   Serial.print(accelSmooth * 100.0 / 32767.0);
  Serial.print(",Frein:");   Serial.print(freinSmooth * 100.0 / 32767.0);
  Serial.print(",Embr:");    Serial.print(embrSmooth * 100.0 / 32767.0);
  Serial.print(",Vitesse:"); Serial.print(gearDetected * 15);
  Serial.print(",Min:-100,Max:100");
  Serial.println();
#endif

  delay(20); // ~50 Hz, confortable pour le traceur
}
