#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <XPowersLib.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>

#define PROJECT_DEVICE_ID        "esp32-041f746cdda0"
#define PROJECT_FIRMWARE_VERSION "1.0.0"

// =============================================================================
// Module PMU -- AXP2101 (gestion d'alimentation T-Beam Q410)
//
// Gere les rails d'alimentation du SX1276 (ALDO2) et du GPS (ALDO3).
// Doit etre initialise avant LoRa et GPS. Wire doit etre demarre au prealable.
// =============================================================================

static const uint8_t PROJECT_PMU_SDA_PIN = 21;
static const uint8_t PROJECT_PMU_SCL_PIN = 22;

static XPowersAXP2101 g_pmu;
static bool           g_pmuReady = false;

static void projectPmuSetup() {
    if (!g_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PROJECT_PMU_SDA_PIN, PROJECT_PMU_SCL_PIN)) {
        Serial.println("[PMU] AXP2101 introuvable — brancher la batterie ou alimenter par USB.");
        g_pmuReady = false;
        return;
    }
    g_pmu.setALDO2Voltage(3300);
    g_pmu.enableALDO2();   // Rail SX1276 (LoRa)
    g_pmu.setALDO3Voltage(3300);
    g_pmu.enableALDO3();   // Rail module GPS
    delay(200);
    g_pmuReady = true;
    Serial.println("[PMU] AXP2101 initialise (ALDO2=LoRa 3.3V, ALDO3=GPS 3.3V).");
}

static float projectPmuGetBatteryVoltage() {
    if (!g_pmuReady) return 0.0f;
    return g_pmu.getBattVoltage() / 1000.0f;
}

static uint8_t projectPmuGetBatteryPercent() {
    if (!g_pmuReady) return 0;
    int pct = g_pmu.getBatteryPercent();
    return (pct < 0) ? 0 : (uint8_t)pct;
}

static bool projectPmuIsCharging() {
    if (!g_pmuReady) return false;
    return g_pmu.isCharging();
}

// =============================================================================
// Module LoRa -- SX1276 868 MHz (RadioLib)
//
// Brochage SPI specifique au T-Beam V1.2.
// SF7 : bon compromis portee/debit. Puissance 10 dBm (limite legale EU).
// Le rail ALDO2 doit etre active par le PMU avant cet init.
// =============================================================================

static const uint8_t  PROJECT_LORA_SCK_PIN  = 5;
static const uint8_t  PROJECT_LORA_MISO_PIN = 19;
static const uint8_t  PROJECT_LORA_MOSI_PIN = 27;
static const uint8_t  PROJECT_LORA_NSS_PIN  = 18;
static const uint8_t  PROJECT_LORA_RST_PIN  = 23;
static const uint8_t  PROJECT_LORA_DIO0_PIN = 26;
static const uint8_t  PROJECT_LORA_DIO1_PIN = 33;

static const float    PROJECT_LORA_FREQ_MHZ  = 868.0f;
static const float    PROJECT_LORA_BW_KHZ    = 125.0f;
static const uint8_t  PROJECT_LORA_SF        = 7;
static const uint8_t  PROJECT_LORA_CR        = 5;
static const int8_t   PROJECT_LORA_POWER_DBM = 10;

static SX1276 g_lora = new Module(PROJECT_LORA_NSS_PIN, PROJECT_LORA_DIO0_PIN,
                                   PROJECT_LORA_RST_PIN, PROJECT_LORA_DIO1_PIN);
static bool   g_loraReady = false;

static void projectLoraSetup() {
    SPI.begin(PROJECT_LORA_SCK_PIN, PROJECT_LORA_MISO_PIN, PROJECT_LORA_MOSI_PIN);

    int state = g_lora.begin(PROJECT_LORA_FREQ_MHZ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] Initialisation echouee (code %d)\n", state);
        g_loraReady = false;
        return;
    }
    g_lora.setOutputPower(PROJECT_LORA_POWER_DBM);
    g_lora.setBandwidth(PROJECT_LORA_BW_KHZ);
    g_lora.setSpreadingFactor(PROJECT_LORA_SF);
    g_lora.setCodingRate(PROJECT_LORA_CR);
    g_loraReady = true;
    Serial.printf("[LoRa] SX1276 initialise (%.1f MHz | SF%d | BW%.0f kHz | CR4/%d | %d dBm)\n",
                  PROJECT_LORA_FREQ_MHZ, PROJECT_LORA_SF, PROJECT_LORA_BW_KHZ,
                  PROJECT_LORA_CR, PROJECT_LORA_POWER_DBM);
}

// Transmet un payload texte par LoRa. Retourne vrai si OK.
static bool projectLoraSend(String payload) {
    if (!g_loraReady) return false;
    return g_lora.transmit(payload.c_str()) == RADIOLIB_ERR_NONE;
}

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
// Module BMP280 -- temperature et pression atmospherique (I2C)
// =============================================================================

static Adafruit_BMP280 g_bmp;
static bool            g_bmpReady = false;

struct ProjectBmpReading {
  float       tempC;        // Température en °C
  float       pressureHpa;  // Pression en hPa
  float       altitudeM;    // Altitude estimée en m (référence 1013.25 hPa au niveau de la mer)
  bool        ready;        // Faux si le capteur est absent ou en erreur
};

// Initialise le BMP280. Teste 0x76 puis 0x77 car la broche SDO varie selon les modules.
// Wire.begin() est appele dans setup() (bus partage avec AXP2101).
static void projectBmpSetup() {
  uint8_t addr = 0;
  if      (g_bmp.begin(0x76)) { addr = 0x76; }
  else if (g_bmp.begin(0x77)) { addr = 0x77; }
  else {
    Serial.println("[BMP280] Capteur introuvable (0x76 et 0x77) — verifier adresse et cablage.");
    g_bmpReady = false;
    return;
  }

  g_bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  g_bmpReady = true;
  Serial.printf("[BMP280] Capteur initialise (addr=0x%02X)\n", addr);
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

// =============================================================================
// Payload LoRa -- construction du message a transmettre a la gateway
//
// JSON compact envoye par LoRa. La gateway Heltec le recoit et le retransmet
// enrichi (RSSI, SNR, horodatage NTP) vers le backend HTTP.
// Les champs optionnels sont omis si le capteur n'est pas disponible.
// =============================================================================

static String projectBuildLoraPayload(
    ProjectGpsReading  gps,
    ProjectBmpReading  bmp,
    ProjectDustReading dust,
    float                     battVolt,
    uint8_t                   battPct,
    bool                      battCharging,
    uint32_t                  msgSeq)
{
    // message_id : device_id + uptime ms (fallback sans NTP, conforme a la spec)
    char msgId[48];
    snprintf(msgId, sizeof(msgId), "%s-%lu", PROJECT_DEVICE_ID, (unsigned long)millis());

    String json = "{";
    json += "\"id\":\""   + String(PROJECT_DEVICE_ID) + "\"";
    json += ",\"fw\":\""  + String(PROJECT_FIRMWARE_VERSION) + "\"";
    json += ",\"msg\":\"" + String(msgId) + "\"";
    json += ",\"seq\":"   + String(msgSeq);

    json += ",\"gps_ok\":" + String(gps.locationValid ? "true" : "false");
    json += ",\"sats\":"   + String(gps.satellites);
    if (gps.locationValid) {
        json += ",\"lat\":"  + String(gps.lat, 6);
        json += ",\"lng\":"  + String(gps.lng, 6);
    }
    if (gps.altValid) json += ",\"alt\":"  + String(gps.altM, 1);

    if (bmp.ready) {
        json += ",\"temp\":"  + String(bmp.tempC, 2);
        json += ",\"pres\":"  + String(bmp.pressureHpa, 2);
    }

    if (dust.ready) {
        json += ",\"pm25\":"  + String(dust.estimatedUgM3, 1);
        json += ",\"dust\":"  + String((unsigned int)dust.dustIndexPct);
    }

    if (battVolt > 0.0f) {
        json += ",\"batt_v\":"  + String(battVolt, 2);
        json += ",\"batt_p\":"  + String((unsigned int)battPct);
        json += ",\"batt_c\":"  + String(battCharging ? "true" : "false");
    }

    json += "}";
    return json;
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

  // Bus I2C partage entre AXP2101 et BMP280
  Wire.begin(PROJECT_PMU_SDA_PIN, PROJECT_PMU_SCL_PIN);

  // PMU en premier : active les rails d'alimentation LoRa (ALDO2) et GPS (ALDO3)
  projectPmuSetup();

  projectDustSetup();
  projectBmpSetup();
  projectGpsSetup();
  projectLoraSetup();
}

void loop() {
  static uint32_t s_msgSeq = 0;
  s_msgSeq++;

  const ProjectDustReading dust  = projectDustAcquireReading();
  const ProjectBmpReading  bmp   = projectBmpAcquireReading();
  const ProjectGpsReading  gps   = projectGpsAcquireReading();
  const float              battV = projectPmuGetBatteryVoltage();
  const uint8_t            battP = projectPmuGetBatteryPercent();
  const bool               battC = projectPmuIsCharging();
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

  // --- Batterie ---
  Serial.println("  [Batterie]");
  if (battV > 0.0f)
    Serial.printf("  %.2f V  %u%%  %s\n", battV, battP, battC ? "en charge" : "decharge");
  else
    Serial.println("  (PMU absent — alimentation USB)");

  Serial.println();

  // --- Envoi LoRa ---
  String payload = projectBuildLoraPayload(gps, bmp, dust, battV, battP, battC, s_msgSeq);
  bool   sent    = projectLoraSend(payload);

  Serial.println("  [LoRa]");
  Serial.printf("  Payload (%u oct) : %s\n", (unsigned int)payload.length(), payload.c_str());
  Serial.printf("  Envoi           : %s\n",  sent ? "OK" : "ECHEC");
  Serial.println();

  delay(4000);
}
