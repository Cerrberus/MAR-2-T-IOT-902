#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>

// --- Broches ---
static const uint8_t PROJECT_DUST_AOUT_PIN = 36;  // Sortie analogique du capteur (VP / ADC1)
static const uint8_t PROJECT_DUST_ILED_PIN = 13;  // Commande LED infrarouge du capteur

// --- Timing de la séquence de mesure (µs) ---
// Séquence Sharp GP2Y1010AU0F : LED ON 280µs, échantillon, OFF 9680µs
static const uint16_t PROJECT_DUST_LED_PULSE_US    = 280;   // Durée d'impulsion LED
static const uint16_t PROJECT_DUST_POST_SAMPLE_US  = 40;    // Attente après échantillon ON avant extinction
static const uint16_t PROJECT_DUST_OFF_SETTLE_US   = 9680;  // Temps de repos LED éteinte

// --- Paramètres d'acquisition ---
static const uint8_t  PROJECT_DUST_ANALOG_READS          = 4;     // Lectures ADC moyennées par échantillon
static const uint8_t  PROJECT_DUST_PAIR_COUNT             = 48;    // Paires OFF/ON par mesure (réduit le bruit)
static const uint8_t  PROJECT_DUST_CALIBRATION_REPORTS   = 8;     // Mesures initiales pour établir la baseline
static const uint8_t  PROJECT_DUST_VALID_STREAK_REQUIRED = 3;     // Mesures valides consécutives avant d'accepter

// --- Mise à l'échelle ---
static const float PROJECT_DUST_INDEX_FULL_SCALE_ADC    = 8.0f;   // Delta ADC relatif = 100% du niveau
static const float PROJECT_DUST_ESTIMATED_RANGE_UG_M3   = 500.0f; // PM2.5 max estimé à pleine échelle (µg/m³)

// --- Seuils de validité du signal ---
static const float PROJECT_DUST_MIN_SIGNAL_ON_ADC    = 2.0f; // ON raw minimum pour considérer le signal présent
static const float PROJECT_DUST_MIN_SIGNAL_DELTA_ADC = 1.0f; // |delta| minimum acceptable

// --- Lissage de l'affichage ---
// La montée est bridée à MAX_RISE par cycle pour éviter les pics instantanés.
// La descente suit une moyenne exponentielle avec coefficient DECAY.
static const float PROJECT_DUST_DISPLAY_MAX_RISE_ADC = 1.0f;  // Montée max par cycle (ADC)
static const float PROJECT_DUST_DISPLAY_DECAY        = 0.75f; // Coefficient de descente (0=rapide, 1=jamais)

// --- État global du module poussière ---
static uint8_t g_projectDustCalibrationReports = 0;
static float   g_projectDustBaselineDelta      = 0.0f;
static float   g_projectDustSmoothedDelta      = 0.0f;
static uint8_t g_projectDustSmoothedIndex      = 0;
static uint8_t g_projectDustValidSignalStreak  = 0;

// --- Buffer circulaire pour la moyenne glissante sur 10 lectures ---
static const uint8_t PROJECT_DUST_AVG_WINDOW = 10;
static float   g_dustOffHistory[PROJECT_DUST_AVG_WINDOW] = {};
static float   g_dustOnHistory[PROJECT_DUST_AVG_WINDOW]  = {};
static uint8_t g_dustAvgHead  = 0; // Prochain emplacement à écrire
static uint8_t g_dustAvgCount = 0; // Nombre de lectures valides dans le buffer

// Résultat d'une mesure complète
struct ProjectDustReading {
  float      offAvg;         // Moyenne ADC LED éteinte
  float      onAvg;          // Moyenne ADC LED allumée
  float      delta;          // onAvg - offAvg (brut)
  float      relativeDelta;  // delta - baseline, lissé, >= 0
  float      estimatedUgM3;  // PM2.5 estimé en µg/m³
  uint8_t    dustIndexPct;   // Indice de poussière 0-100 %
  const char *label;         // Libellé qualitatif
  const char *state;         // État du module (stabilisation / calibration / pret / aucun_signal)
  bool       signalOk;       // Vrai si le signal ADC dépasse les seuils minimaux
  bool       ready;          // Vrai si la mesure est exploitable
};

// Moyenne de PROJECT_DUST_ANALOG_READS lectures ADC consécutives pour réduire le bruit électronique
static uint16_t projectDustReadShortAverage() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < PROJECT_DUST_ANALOG_READS; i++) {
    sum += analogRead(PROJECT_DUST_AOUT_PIN);
  }
  return static_cast<uint16_t>(sum / PROJECT_DUST_ANALOG_READS);
}

// Pousse offAvg/onAvg dans le buffer et retourne leurs moyennes glissantes
static void projectDustPushAvg(float offRaw, float onRaw, float &offAvgOut, float &onAvgOut) {
  g_dustOffHistory[g_dustAvgHead] = offRaw;
  g_dustOnHistory[g_dustAvgHead]  = onRaw;
  g_dustAvgHead = (g_dustAvgHead + 1) % PROJECT_DUST_AVG_WINDOW;
  if (g_dustAvgCount < PROJECT_DUST_AVG_WINDOW) g_dustAvgCount++;

  float sumOff = 0.0f, sumOn = 0.0f;
  for (uint8_t i = 0; i < g_dustAvgCount; i++) {
    sumOff += g_dustOffHistory[i];
    sumOn  += g_dustOnHistory[i];
  }
  offAvgOut = sumOff / g_dustAvgCount;
  onAvgOut  = sumOn  / g_dustAvgCount;
}

// Retourne un libellé qualitatif selon l'indice en pourcentage
static const char *projectDustLabelFromPct(uint8_t pct) {
  if (pct < 10) return "silence";
  if (pct < 35) return "faible";
  if (pct < 65) return "moyen";
  if (pct < 90) return "fort";
  return "tres fort";
}

// Effectue une mesure complète : PROJECT_DUST_PAIR_COUNT paires OFF/ON,
// calcule la baseline incrémentale, lisse le delta, et détermine l'état.
static ProjectDustReading projectDustAcquireReading() {
  ProjectDustReading reading = {};
  uint32_t offSum = 0;
  uint32_t onSum  = 0;

  // Accumulation des paires OFF/ON selon le timing Sharp GP2Y1010AU0F
  for (uint8_t i = 0; i < PROJECT_DUST_PAIR_COUNT; i++) {
    digitalWrite(PROJECT_DUST_ILED_PIN, LOW);
    delayMicroseconds(PROJECT_DUST_OFF_SETTLE_US);
    offSum += projectDustReadShortAverage();

    digitalWrite(PROJECT_DUST_ILED_PIN, HIGH);
    delayMicroseconds(PROJECT_DUST_LED_PULSE_US);
    onSum += projectDustReadShortAverage();
    delayMicroseconds(PROJECT_DUST_POST_SAMPLE_US);
    digitalWrite(PROJECT_DUST_ILED_PIN, LOW);
  }

  // Valeurs brutes de cette lecture, puis lissage sur la fenêtre glissante
  float rawOff = (float)offSum / PROJECT_DUST_PAIR_COUNT;
  float rawOn  = (float)onSum  / PROJECT_DUST_PAIR_COUNT;
  projectDustPushAvg(rawOff, rawOn, reading.offAvg, reading.onAvg);
  reading.delta = reading.onAvg - reading.offAvg;

  // Signal valide si le photodetecteur reçoit suffisamment de lumière IR
  reading.signalOk = (reading.onAvg >= PROJECT_DUST_MIN_SIGNAL_ON_ADC) ||
                     (fabs(reading.delta) >= PROJECT_DUST_MIN_SIGNAL_DELTA_ADC);

  // Moyenne incrémentale de la baseline sur les N premières mesures valides
  if (g_projectDustCalibrationReports < PROJECT_DUST_CALIBRATION_REPORTS) {
    if (g_projectDustCalibrationReports == 0) {
      g_projectDustBaselineDelta = reading.delta;
    } else {
      g_projectDustBaselineDelta =
          (g_projectDustBaselineDelta * g_projectDustCalibrationReports + reading.delta) /
          (g_projectDustCalibrationReports + 1);
    }
    g_projectDustCalibrationReports++;
  }

  // Delta relatif à la baseline (plancher à 0 : on ne détecte pas un air plus propre)
  reading.relativeDelta = reading.delta - g_projectDustBaselineDelta;
  if (reading.relativeDelta < 0.0f) {
    reading.relativeDelta = 0.0f;
  }

  // Lissage : montée bridée + descente exponentielle
  if (reading.relativeDelta > g_projectDustSmoothedDelta) {
    float rise = reading.relativeDelta - g_projectDustSmoothedDelta;
    if (rise > PROJECT_DUST_DISPLAY_MAX_RISE_ADC) {
      rise = PROJECT_DUST_DISPLAY_MAX_RISE_ADC;
    }
    g_projectDustSmoothedDelta += rise;
  } else {
    g_projectDustSmoothedDelta = g_projectDustSmoothedDelta * PROJECT_DUST_DISPLAY_DECAY +
                                 reading.relativeDelta * (1.0f - PROJECT_DUST_DISPLAY_DECAY);
  }

  // Seuil plancher pour éviter un affichage qui ne revient jamais à zéro
  if (g_projectDustSmoothedDelta < 0.02f) {
    g_projectDustSmoothedDelta = 0.0f;
  }

  reading.relativeDelta = g_projectDustSmoothedDelta;

  // Conversion en pourcentage 0-100 puis en PM2.5 estimé
  float pct = 0.0f;
  if (PROJECT_DUST_INDEX_FULL_SCALE_ADC > 0.0f) {
    pct = (reading.relativeDelta * 100.0f) / PROJECT_DUST_INDEX_FULL_SCALE_ADC;
  }
  if (pct < 0.0f)   pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;

  g_projectDustSmoothedIndex  = (uint8_t)(pct + 0.5f);
  reading.estimatedUgM3       = (pct * PROJECT_DUST_ESTIMATED_RANGE_UG_M3) / 100.0f;
  reading.dustIndexPct        = g_projectDustSmoothedIndex;
  reading.label               = projectDustLabelFromPct(reading.dustIndexPct);

  // Machine d'état : aucun_signal → stabilisation → calibration → pret
  if (!reading.signalOk) {
    g_projectDustValidSignalStreak = 0;
    reading.state = "aucun_signal";
    reading.ready = false;
    return reading;
  }

  if (g_projectDustValidSignalStreak < 255) {
    g_projectDustValidSignalStreak++;
  }

  if (g_projectDustValidSignalStreak < PROJECT_DUST_VALID_STREAK_REQUIRED) {
    reading.state = "stabilisation";
    reading.ready = false;
    return reading;
  }

  if (g_projectDustCalibrationReports < PROJECT_DUST_CALIBRATION_REPORTS) {
    reading.state = "calibration";
    reading.ready = false;
    return reading;
  }

  reading.state = "pret";
  reading.ready = true;
  return reading;
}

// Configure les broches et l'ADC, remet l'état global à zéro
static void projectDustSetup() {
  pinMode(PROJECT_DUST_ILED_PIN, OUTPUT);
  digitalWrite(PROJECT_DUST_ILED_PIN, LOW);

#if defined(ESP32)
  analogReadResolution(12);                                      // ADC 12 bits (0-4095)
  analogSetPinAttenuation(PROJECT_DUST_AOUT_PIN, ADC_0db);      // Plage 0-1.1 V, sensibilité max
#endif

  g_projectDustCalibrationReports = 0;
  g_projectDustBaselineDelta      = 0.0f;
  g_projectDustSmoothedDelta      = 0.0f;
  g_projectDustSmoothedIndex      = 0;
  g_projectDustValidSignalStreak  = 0;

  Serial.printf("[Project Dust] Module initialise (AOUT=%d, ILED=%d)\n",
                PROJECT_DUST_AOUT_PIN, PROJECT_DUST_ILED_PIN);
  Serial.println("[Project Dust] Mesure relative OFF/ON active avec sortie en indice et PM2.5 estime.");
}

// Sérialise une mesure dans un objet JSON partiel (sans accolades englobantes)
static void projectDustAppendJson(String &json) {
  const ProjectDustReading reading = projectDustAcquireReading();

  json += ",\"dust_ready\":"              ; json += reading.ready ? "true" : "false";
  json += ",\"dust_state\":\""            ; json += reading.state;            json += "\"";
  json += ",\"dust_off_raw\":"            ; json += String(reading.offAvg, 2);
  json += ",\"dust_on_raw\":"             ; json += String(reading.onAvg,  2);
  json += ",\"dust_delta\":"              ; json += String(reading.delta,   2);
  json += ",\"dust_base\":"               ; json += String(g_projectDustBaselineDelta, 2);
  json += ",\"dust_relative\":"           ; json += String(reading.relativeDelta, 2);
  json += ",\"dust_level_pct\":"          ; json += String((unsigned int)reading.dustIndexPct);
  json += ",\"dust_label\":\""            ; json += reading.label;            json += "\"";
  json += ",\"dust_pm25_estimated_ug_m3\":"; json += String(reading.estimatedUgM3, 1);
}

// =============================================================================
// Module BMP280 — température et pression atmosphérique (I2C)
// =============================================================================

static const uint8_t  PROJECT_BMP_SDA_PIN = 21;
static const uint8_t  PROJECT_BMP_SCL_PIN = 22;
static const uint8_t  PROJECT_BMP_I2C_ADDR = 0x76; // SDO à GND → 0x76, SDO à VCC → 0x77

static Adafruit_BMP280 g_bmp;
static bool            g_bmpReady = false; // Faux si le capteur n'a pas répondu à l'init

struct ProjectBmpReading {
  float       tempC;        // Température en °C
  float       pressureHpa;  // Pression en hPa
  float       altitudeM;    // Altitude estimée en m (référence 1013.25 hPa au niveau de la mer)
  bool        ready;        // Faux si le capteur est absent ou en erreur
};

// Initialise le bus I2C et le BMP280 ; marque g_bmpReady selon le résultat
static void projectBmpSetup() {
  Wire.begin(PROJECT_BMP_SDA_PIN, PROJECT_BMP_SCL_PIN);

  if (!g_bmp.begin(PROJECT_BMP_I2C_ADDR)) {
    Serial.println("[BMP280] Capteur introuvable — verifier adresse et cablage.");
    g_bmpReady = false;
    return;
  }

  // Réglages recommandés par Adafruit pour une utilisation en intérieur
  g_bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,   // température
                    Adafruit_BMP280::SAMPLING_X16,  // pression
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  g_bmpReady = true;

  Serial.printf("[BMP280] Capteur initialise (SDA=%d, SCL=%d, addr=0x%02X)\n",
                PROJECT_BMP_SDA_PIN, PROJECT_BMP_SCL_PIN, PROJECT_BMP_I2C_ADDR);
}

// Retourne une lecture BMP280 ; ready=false si le capteur n'est pas disponible
static ProjectBmpReading projectBmpAcquireReading() {
  ProjectBmpReading r = {};
  if (!g_bmpReady) {
    r.ready = false;
    return r;
  }
  r.tempC       = g_bmp.readTemperature();
  r.pressureHpa = g_bmp.readPressure() / 100.0f; // Pa → hPa
  r.altitudeM   = g_bmp.readAltitude(1013.25f);
  r.ready       = true;
  return r;
}

// =============================================================================
// Module GPS — NEO-6M/M8N via UART (HardwareSerial 1)
// =============================================================================

static const uint8_t  PROJECT_GPS_RX_PIN  = 34;   // ESP32 reçoit les trames NMEA du GPS
static const uint8_t  PROJECT_GPS_TX_PIN  = 12;   // ESP32 envoie des commandes au GPS (rarement utilisé)
static const uint32_t PROJECT_GPS_BAUD    = 9600;
static const uint32_t PROJECT_GPS_FEED_MS = 400;  // Durée de lecture du flux NMEA par cycle (ms)

static TinyGPSPlus    g_gps;
static HardwareSerial g_gpsSerial(1); // UART1 de l'ESP32

struct ProjectGpsReading {
  double   lat;          // Latitude en degrés décimaux (+ = Nord)
  double   lng;          // Longitude en degrés décimaux (+ = Est)
  double   altM;         // Altitude GPS en mètres (ellipsoïde WGS84)
  double   speedKmh;     // Vitesse sol en km/h
  double   hdop;         // Précision horizontale (< 1 excellent, > 5 mauvais)
  uint8_t  satellites;   // Nombre de satellites en vue
  bool     locationValid; // Vrai si lat/lng ont été reçus et sont récents
  bool     altValid;
  bool     speedValid;
};

// Lit le flux NMEA pendant PROJECT_GPS_FEED_MS ms et retourne l'état courant du parseur
static ProjectGpsReading projectGpsAcquireReading() {
  uint32_t deadline = millis() + PROJECT_GPS_FEED_MS;
  while (millis() < deadline) {
    while (g_gpsSerial.available()) {
      g_gps.encode(g_gpsSerial.read());
    }
  }

  ProjectGpsReading r = {};
  r.locationValid = g_gps.location.isValid() && g_gps.location.age() < 2000;
  r.altValid      = g_gps.altitude.isValid() && g_gps.altitude.age() < 2000;
  r.speedValid    = g_gps.speed.isValid()    && g_gps.speed.age()    < 2000;
  r.satellites    = (uint8_t)g_gps.satellites.value();
  r.hdop          = g_gps.hdop.hdop();

  if (r.locationValid) {
    r.lat = g_gps.location.lat();
    r.lng = g_gps.location.lng();
  }
  if (r.altValid)   r.altM     = g_gps.altitude.meters();
  if (r.speedValid) r.speedKmh = g_gps.speed.kmph();

  return r;
}

// Initialise l'UART GPS
static void projectGpsSetup() {
  g_gpsSerial.begin(PROJECT_GPS_BAUD, SERIAL_8N1,
                    PROJECT_GPS_RX_PIN, PROJECT_GPS_TX_PIN);
  Serial.printf("[GPS] UART initialise (RX=%d, TX=%d, %lu baud)\n",
                PROJECT_GPS_RX_PIN, PROJECT_GPS_TX_PIN, PROJECT_GPS_BAUD);
}

// Affiche une barre de progression ASCII sur 20 caractères : [####----------------]
static void printBar(uint8_t pct) {
  const uint8_t width  = 20;
  uint8_t       filled = (pct * width + 50) / 100;
  Serial.print("[");
  for (uint8_t i = 0; i < width; i++) {
    Serial.print(i < filled ? "#" : "-");
  }
  Serial.print("]");
}

void setup() {
  Serial.begin(115200);
  projectDustSetup();
  projectBmpSetup();
  projectGpsSetup();
}

void loop() {
  const ProjectDustReading dust = projectDustAcquireReading();
  const ProjectBmpReading  bmp  = projectBmpAcquireReading();
  const ProjectGpsReading  gps  = projectGpsAcquireReading();
  uint32_t ts = millis();

  Serial.println("========================================");
  Serial.printf("  t = %lu ms\n", ts);
  Serial.println("========================================");

  // --- GPS ---
  Serial.println("  [GPS]");
  Serial.printf("  Satellites : %u   HDOP: %.1f\n", gps.satellites, gps.hdop);
  if (gps.locationValid) {
    Serial.printf("  Position   : %.6f, %.6f\n", gps.lat, gps.lng);
  } else {
    Serial.println("  Position   : (pas de fix)");
  }
  if (gps.altValid)   Serial.printf("  Altitude   : %.1f m\n",   gps.altM);
  if (gps.speedValid) Serial.printf("  Vitesse    : %.1f km/h\n", gps.speedKmh);

  Serial.println();

  // --- BMP280 ---
  Serial.println("  [BMP280]");
  if (bmp.ready) {
    Serial.printf("  Temp     : %.2f degC\n",   bmp.tempC);
    Serial.printf("  Pression : %.2f hPa\n",    bmp.pressureHpa);
    Serial.printf("  Altitude : %.1f m\n",       bmp.altitudeM);
  } else {
    Serial.println("  (capteur absent ou non initialise)");
  }

  Serial.println();

  // --- Poussière ---
  Serial.printf("  [Poussiere]  etat: %s\n", dust.state);
  Serial.printf("  OFF raw  : %7.2f ADC\n", dust.offAvg);
  Serial.printf("  ON  raw  : %7.2f ADC\n", dust.onAvg);
  Serial.printf("  Delta    : %+7.2f ADC  (base: %.2f)\n", dust.delta, g_projectDustBaselineDelta);
  Serial.printf("  Relatif  : %+7.2f ADC\n", dust.relativeDelta);
  Serial.println();
  if (dust.ready) {
    Serial.printf("  Niveau   : %3u%%  %s  ", dust.dustIndexPct, dust.label);
    printBar(dust.dustIndexPct);
    Serial.println();
    Serial.printf("  PM2.5 ~  : %.1f ug/m3\n", dust.estimatedUgM3);
  } else {
    Serial.println("  (en attente de donnees valides)");
  }

  Serial.println();
}
