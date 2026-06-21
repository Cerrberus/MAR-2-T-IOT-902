#include "dust.h"
#include <Arduino.h>
#include <math.h>

// =============================================================================
// Module Poussiere -- Sharp GP2Y1010AU0F (analogique)
//
// Mesure differentielle OFF/ON : soustrait le bruit ambiant (lumiere, offset ADC)
// en comparant la lecture LED eteinte a la lecture LED allumee.
// Attenuation ADC_11db requise pour couvrir la plage 0-3.3 V du capteur.
// Hardware : resistor 150 ohm + condo 220 uF sur Vcc (broche 1) du capteur.
// =============================================================================

static const uint8_t AOUT_PIN = 36;
static const uint8_t LED_PIN  = 13;

// Timing Sharp GP2Y1010AU0F (µs)
static const uint16_t LED_PULSE_US    = 280;
static const uint16_t SAMPLE_HOLD_US  = 40;
static const uint16_t LED_OFF_SETTLE_US = 9680;

// Parametres d'acquisition
static const uint8_t ADC_SAMPLES      = 16;   // lectures ADC moyennees par echantillon
static const uint8_t PAIRS_PER_MEASURE = 48;   // paires OFF/ON par mesure
static const uint8_t CALIBRATION_SAMPLES = 8;  // mesures pour etablir la baseline
static const uint8_t WARMUP_STREAK    = 3;     // mesures valides consecutives avant ready

// Mise a l'echelle
static const float FULL_SCALE_ADC  = 8.0f;    // delta ADC = 100 % du niveau
static const float FULL_SCALE_UG_M3 = 500.0f; // PM2.5 max estime a pleine echelle (µg/m³)

// Seuils de validite du signal
static const float MIN_LED_ON_ADC = 2.0f;     // ON minimum pour signal present
static const float MIN_DELTA_ADC  = 1.0f;     // |delta| minimum acceptable

// Lissage : montee bridee + descente exponentielle
static const float MAX_RISE_ADC  = 0.3f;      // montee max par cycle (ADC)
static const float DECAY_FACTOR  = 0.90f;     // coefficient de descente (0=rapide, 1=jamais)

// Etat interne du module
static uint8_t s_calibrationCount = 0;
static float   s_baselineDelta    = 0.0f;
static float   s_smoothedDelta    = 0.0f;
static uint8_t s_smoothedPct      = 0;
static uint8_t s_validStreak      = 0;

// Buffer circulaire pour la moyenne glissante
static const uint8_t WINDOW_SIZE = 10;
static float   s_offBuf[WINDOW_SIZE] = {};
static float   s_onBuf[WINDOW_SIZE]  = {};
static uint8_t s_bufHead  = 0;
static uint8_t s_bufCount = 0;

// -----------------------------------------------------------------------------

float dustBaselineDelta() {
    return s_baselineDelta;
}

static uint16_t readAdcAverage() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(AOUT_PIN);
    }
    return static_cast<uint16_t>(sum / ADC_SAMPLES);
}

static void pushSlidingWindow(float offRaw, float onRaw, float &offOut, float &onOut) {
    s_offBuf[s_bufHead] = offRaw;
    s_onBuf[s_bufHead]  = onRaw;
    s_bufHead = (s_bufHead + 1) % WINDOW_SIZE;
    if (s_bufCount < WINDOW_SIZE) s_bufCount++;

    float sumOff = 0.0f, sumOn = 0.0f;
    for (uint8_t i = 0; i < s_bufCount; i++) {
        sumOff += s_offBuf[i];
        sumOn  += s_onBuf[i];
    }
    offOut = sumOff / s_bufCount;
    onOut  = sumOn  / s_bufCount;
}

static const char *labelFromLevel(uint8_t pct) {
    if (pct < 10) return "none";
    if (pct < 35) return "low";
    if (pct < 65) return "moderate";
    if (pct < 90) return "high";
    return "very_high";
}

// -----------------------------------------------------------------------------

DustReading dustRead() {
    DustReading r = {};
    uint32_t offSum = 0;
    uint32_t onSum  = 0;

    for (uint8_t i = 0; i < PAIRS_PER_MEASURE; i++) {
        digitalWrite(LED_PIN, LOW);
        delayMicroseconds(LED_OFF_SETTLE_US);
        offSum += readAdcAverage();

        digitalWrite(LED_PIN, HIGH);
        delayMicroseconds(LED_PULSE_US);
        onSum += readAdcAverage();
        delayMicroseconds(SAMPLE_HOLD_US);
        digitalWrite(LED_PIN, LOW);
    }

    float rawOff = (float)offSum / PAIRS_PER_MEASURE;
    float rawOn  = (float)onSum  / PAIRS_PER_MEASURE;
    pushSlidingWindow(rawOff, rawOn, r.ledOffAvg, r.ledOnAvg);
    r.rawDelta = r.ledOnAvg - r.ledOffAvg;

    r.signalValid = (r.ledOnAvg >= MIN_LED_ON_ADC) ||
                    (fabs(r.rawDelta) >= MIN_DELTA_ADC);

    if (s_validStreak >= WARMUP_STREAK && s_calibrationCount < CALIBRATION_SAMPLES) {
        if (s_calibrationCount == 0) {
            s_baselineDelta = r.rawDelta;
        } else {
            s_baselineDelta = (s_baselineDelta * s_calibrationCount + r.rawDelta) /
                              (s_calibrationCount + 1);
        }
        s_calibrationCount++;
    }

    if (s_calibrationCount >= CALIBRATION_SAMPLES && r.rawDelta <= s_baselineDelta) {
        s_baselineDelta = s_baselineDelta * 0.995f + r.rawDelta * 0.005f;
    }

    r.smoothedDelta = r.rawDelta - s_baselineDelta;
    if (r.smoothedDelta < 0.0f) r.smoothedDelta = 0.0f;

    if (r.smoothedDelta > s_smoothedDelta) {
        float rise = r.smoothedDelta - s_smoothedDelta;
        if (rise > MAX_RISE_ADC) rise = MAX_RISE_ADC;
        s_smoothedDelta += rise;
    } else {
        s_smoothedDelta = s_smoothedDelta * DECAY_FACTOR +
                          r.smoothedDelta * (1.0f - DECAY_FACTOR);
    }

    if (s_smoothedDelta < 0.02f) s_smoothedDelta = 0.0f;

    r.smoothedDelta = s_smoothedDelta;

    float pct = 0.0f;
    if (FULL_SCALE_ADC > 0.0f) {
        pct = (r.smoothedDelta * 100.0f) / FULL_SCALE_ADC;
    }
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    s_smoothedPct  = (uint8_t)(pct + 0.5f);
    r.pm25UgM3     = (pct * FULL_SCALE_UG_M3) / 100.0f;
    r.levelPct     = s_smoothedPct;
    r.label        = labelFromLevel(r.levelPct);

    if (!r.signalValid) {
        s_validStreak = 0;
        r.state = "no_signal";
        r.ready = false;
        return r;
    }

    if (s_validStreak < 255) s_validStreak++;

    if (s_validStreak < WARMUP_STREAK) {
        r.state = "warmup";
        r.ready = false;
        return r;
    }

    if (s_calibrationCount < CALIBRATION_SAMPLES) {
        r.state = "calibration";
        r.ready = false;
        return r;
    }

    r.state = "ready";
    r.ready = true;
    return r;
}

void dustSetup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

#if defined(ESP32)
    analogReadResolution(12);
    analogSetPinAttenuation(AOUT_PIN, ADC_11db);     // Plage 0-3.3 V, compatible GP2Y1010
#endif

    s_calibrationCount = 0;
    s_baselineDelta    = 0.0f;
    s_smoothedDelta    = 0.0f;
    s_smoothedPct      = 0;
    s_validStreak      = 0;

    Serial.printf("[Dust] Module initialise (AOUT=%d, LED=%d)\n", AOUT_PIN, LED_PIN);
}

void dustAppendJson(String &json) {
    const DustReading r = dustRead();

    json += ",\"dust_ready\":"               ; json += r.ready ? "true" : "false";
    json += ",\"dust_state\":\""             ; json += r.state;          json += "\"";
    json += ",\"dust_led_off\":"             ; json += String(r.ledOffAvg,    2);
    json += ",\"dust_led_on\":"              ; json += String(r.ledOnAvg,     2);
    json += ",\"dust_raw_delta\":"           ; json += String(r.rawDelta,     2);
    json += ",\"dust_baseline\":"            ; json += String(s_baselineDelta,2);
    json += ",\"dust_smoothed_delta\":"      ; json += String(r.smoothedDelta,2);
    json += ",\"dust_level_pct\":"           ; json += String((unsigned int)r.levelPct);
    json += ",\"dust_label\":\""             ; json += r.label;          json += "\"";
    json += ",\"dust_pm25_ug_m3\":"          ; json += String(r.pm25UgM3, 1);
}
