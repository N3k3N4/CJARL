/*
  Simulateur de volant - Arduino Leonardo
  Version v5 : compensation de la zone morte au centre APRES conversion joystick
  Serial Plotter : IDE Arduino > Outils > Traceur serie (115200 bauds)

  PLOT_MODE 0 : valeurs brutes des potentiometres (0 a 1023)
  PLOT_MODE 1 : valeurs envoyees au PC en pourcentage
                (volant -100..+100, pedales 0..100)

  POURQUOI CETTE VERSION
  Le systeme qui lit la manette (Linux jstest, Windows, jeu...) applique souvent
  SA PROPRE zone morte autour de 0 : tant que la valeur joystick reste dans
  cette zone, il affiche 0. On a alors l'impression que le volant "reste
  bloque" au centre plus longtemps qu'ailleurs.
  Solution : faire SAUTER la valeur envoyee par-dessus cette zone, de facon
  que le systeme la supprime et que le passage au centre dure le meme temps
  que sur le reste de la course.
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
// Relevez ces 3 valeurs avec le Serial Plotter en mode 0 :
//   centreVolant : valeur lue quand le volant est bien droit
//   volantMin    : valeur lue en butee gauche complete
//   volantMax    : valeur lue en butee droite complete
const int centreVolant = 512;
const int volantMin    = 0;
const int volantMax    = 1023;

// =====================================================================
//       COMPENSATION DE LA ZONE MORTE (valeur apres conversion joystick)
// =====================================================================
// Echelle joystick : -32767 .. 0 .. +32767
//
// ANTI_DZ_DEBUT : valeur envoyee des que le volant quitte le centre.
//                 Elle doit etre egale a la zone morte du systeme.
//                 0 = desactive (conversion lineaire pure).
//                 Linux (jstest) : environ 4095 (valeur par defaut ici)
//                 Jeu avec zone morte de X % : X/100 * 32767
//                   (ex : 10 % -> 3277, 5 % -> 1638)
//
// ANTI_DZ_FIN   : marge laissee en butee. Le systeme sature avant la butee
//                 quand il applique sa zone morte aux deux extremites
//                 (Linux jstest : 4095). Mettez 0 si vos butees ne sont pas
//                 atteintes a cause de ce reglage au contraire.
//
// REGLAGE FIN :
//  - le volant reste encore trop longtemps a 0 -> AUGMENTEZ ANTI_DZ_DEBUT
//  - un saut brusque apparait autour du centre -> DIMINUEZ ANTI_DZ_DEBUT
//  - butee atteinte trop tot (saturation avant la fin de course) -> AUGMENTEZ
//    ANTI_DZ_FIN ; butee jamais atteinte -> DIMINUEZ ANTI_DZ_FIN
const float ANTI_DZ_DEBUT = 4095;
const float ANTI_DZ_FIN   = 4095;

// Tolerance autour du centre (en fraction de course) : evite que le bruit du
// potentiometre fasse sauter la valeur entre -DEBUT, 0 et +DEBUT quand le
// volant ne bouge pas. 0.0015 = environ 1 unite sur 1023.
// Utilisee seulement si ANTI_DZ_DEBUT > 0.
const float TOLERANCE_CENTRE = 0.0015;

// =====================================================================
//                    REGLAGES DES PEDALES (modifiables)
// =====================================================================
//   xxxMax = valeur lue quand la pedale est RELACHEE (au repos)
//   xxxMin = valeur lue quand la pedale est ENFONCEE A FOND
// La valeur DIMINUE quand on appuie :
//   relachee -> valeur haute (ex : 1023) -> sortie PC = 0 %
//   a fond   -> valeur basse (ex : 890)  -> sortie PC = 100 %
// xxxMin doit TOUJOURS etre strictement inferieur a xxxMax.
// Pedale n'atteint pas 100 %       -> augmentez xxxMin.
// Pedale reste > 0 % au repos      -> diminuez xxxMax.

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
const float alphaVolant = 0.40;  // 1.0 = aucun lissage, petit = plus lisse
const float alpha       = 0.15;  // pedales

// Lecture moyenne en decimal
float readSmoothF(int pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return sum / 8.0;
}

int readSmooth(int pin) {
  return (int)(readSmoothF(pin) + 0.5);
}

int mapPedale(int val, int minVal, int maxVal) {
  if (maxVal <= minVal) return 0;
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

  volantSmooth = readSmoothF(potVolant);

  // Aucun Serial.println de texte ici : le Serial Plotter afficherait des erreurs.
}

void loop() {
  // --- Lecture ---
  float volantBrut = readSmoothF(potVolant);
  int valVolant = (int)(volantBrut + 0.5);
  int valAccel  = readSmooth(potAccel);
  int valFrein  = readSmooth(potFrein);
  int valEmbr   = readSmooth(potEmbr);

  // --- Volant : lissage puis normalisation -1..+1 (centre asymetrique) ---
  volantSmooth = alphaVolant * volantBrut + (1 - alphaVolant) * volantSmooth;
  float v = volantSmooth;

  float normVolant;
  if (v >= centreVolant) {
    normVolant = (v - centreVolant) / (float)(volantMax - centreVolant);
  } else {
    normVolant = (v - centreVolant) / (float)(centreVolant - volantMin);
  }
  if (normVolant > 1.0)  normVolant = 1.0;
  if (normVolant < -1.0) normVolant = -1.0;

  // --- Conversion en valeur joystick ---
  float joyVolant;
  if (ANTI_DZ_DEBUT <= 0 && ANTI_DZ_FIN <= 0) {
    // Lineaire pur : aucune compensation
    joyVolant = normVolant * 32767.0;
  } else {
    float absN = fabs(normVolant);
    if (absN < TOLERANCE_CENTRE) {
      joyVolant = 0;                                   // repos exact au centre
    } else {
      float debut = ANTI_DZ_DEBUT;                     // premiere valeur envoyee
      float fin   = 32767.0 - ANTI_DZ_FIN;             // derniere valeur envoyee
      float signe = (normVolant > 0) ? 1.0 : -1.0;
      joyVolant = signe * (debut + absN * (fin - debut));
    }
  }
  int volantMapped = (int)joyVolant;

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
  Serial.print(",Min:0,Max:1023");
  Serial.println();
#else
  Serial.print("Volant:");   Serial.print(volantMapped * 100.0 / 32767.0);
  Serial.print(",Accel:");   Serial.print(accelSmooth * 100.0 / 32767.0);
  Serial.print(",Frein:");   Serial.print(freinSmooth * 100.0 / 32767.0);
  Serial.print(",Embr:");    Serial.print(embrSmooth * 100.0 / 32767.0);
  Serial.print(",Vitesse:"); Serial.print(gearDetected * 15);
  Serial.print(",Min:-100,Max:100");
  Serial.println();
#endif

  delay(20);
}
