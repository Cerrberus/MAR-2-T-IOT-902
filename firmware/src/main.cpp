#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <driver/i2s.h>

// ===== BME280 =====
Adafruit_BME280 bme;

// ===== I2S MICRO (TES PINS) =====
#define I2S_WS  1   // LRCL
#define I2S_SD  2   // DOUT
#define I2S_SCK 3   // BCLK

#define I2S_PORT I2S_NUM_0
#define BUFFER_SIZE 512
int32_t samples[BUFFER_SIZE];

void setupI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 4,
        .dma_buf_len = 256
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("TEST");

    // ===== I2C (TES PINS) =====
    Wire.begin(6, 7);

    // ===== BME280 =====
    if (!bme.begin(0x76)) {
        Serial.println("BME280 non detecte !");
    } else {
        Serial.println("BME280 OK");
    }

    // ===== I2S =====
    setupI2S();
    Serial.println("Micro I2S OK");
}

// ===== LOOP =====
void loop() {

    // ===== BME280 =====
    Serial.println("---- BME280 ----");

    Serial.print("Temp: ");
    Serial.print(bme.readTemperature());
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.print("Pressure: ");
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    // ===== MICRO =====
    size_t bytes_read;
    i2s_read(I2S_PORT, samples, sizeof(samples), &bytes_read, portMAX_DELAY);

    int32_t avg = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        avg += abs(samples[i]);
    }
    avg /= BUFFER_SIZE;

    Serial.print("Son: ");
    Serial.println(avg);

    Serial.println("----------------------\n");

    delay(2000);
}