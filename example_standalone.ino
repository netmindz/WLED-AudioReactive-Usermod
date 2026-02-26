/*
   Example: Using AudioFilters and AGCController standalone

   This example demonstrates how to use the audio processing
   libraries independently of WLED.

   Hardware required:
   - ESP32 (or other Arduino-compatible board)
   - I2S MEMS microphone (e.g., INMP441, SPH0645)

   Connections:
   - I2S WS  (LRCLK) -> GPIO 15
   - I2S SD  (DOUT)  -> GPIO 32
   - I2S SCK (BCLK)  -> GPIO 14
   - GND -> GND
   - VDD -> 3.3V
*/

#include <Arduino.h>
#include "audio_filters.h"
#include "agc_controller.h"

// For ESP32 I2S
#ifdef ARDUINO_ARCH_ESP32
#include <driver/i2s.h>
#endif

// I2S Configuration
#define SAMPLE_RATE 18000
#define BLOCK_SIZE 512
#define I2S_WS_PIN 15
#define I2S_SD_PIN 32
#define I2S_SCK_PIN 14

// Audio processing objects
AudioFilters filters;
AGCController agc;

// Sample buffer
float samples[BLOCK_SIZE];

#ifdef ARDUINO_ARCH_ESP32
// Initialize I2S microphone
void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// Read samples from I2S microphone
void readSamples() {
    size_t bytes_read = 0;
    int32_t rawSamples[BLOCK_SIZE];

    i2s_read(I2S_NUM_0, rawSamples, BLOCK_SIZE * sizeof(int32_t),
             &bytes_read, portMAX_DELAY);

    // Convert to float and normalize
    for (int i = 0; i < BLOCK_SIZE; i++) {
        samples[i] = (float)rawSamples[i] / (float)INT32_MAX;
    }
}
#else
// Simulate samples for non-ESP32 boards
void readSamples() {
    // Generate test tone (440 Hz sine wave)
    static float phase = 0.0f;
    float freq = 440.0f;
    float phaseIncrement = 2.0f * PI * freq / SAMPLE_RATE;

    for (int i = 0; i < BLOCK_SIZE; i++) {
        samples[i] = sin(phase) * 0.5f;  // 50% amplitude
        phase += phaseIncrement;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }
}
#endif

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("AudioReactive Library Example");
    Serial.println("=============================");

    // Configure filters
    AudioFilters::Config filterConfig;
    filterConfig.filterMode = 2;  // DC blocker
    filterConfig.micQuality = 1;  // Good quality
    filters.configure(filterConfig);
    Serial.println("✓ AudioFilters configured (DC blocker)");

    // Configure AGC
    AGCController::Config agcConfig;
    agcConfig.preset = AGCController::NORMAL;
    agcConfig.squelch = 10.0f;
    agcConfig.sampleGain = 60.0f;
    agcConfig.inputLevel = 128;
    agcConfig.micQuality = 1;
    agcConfig.micLevelMethod = 1;  // Freeze mode
    agcConfig.fastPath = false;
    agc.configure(agcConfig);
    agc.setEnabled(true);
    Serial.println("✓ AGCController configured (Normal preset, AGC enabled)");

    #ifdef ARDUINO_ARCH_ESP32
    // Initialize I2S microphone
    initI2S();
    Serial.println("✓ I2S microphone initialized");
    #else
    Serial.println("✓ Test tone generator ready");
    #endif

    Serial.println("\nStarting audio processing...");
    Serial.println("Columns: Volume(AGC) | Volume(Raw) | Sensitivity | Pressure | Peak");
    Serial.println("-----------------------------------------------------------------------");
}

void loop() {
    // Read audio samples
    readSamples();

    // Apply DC blocker filter
    filters.applyFilter(BLOCK_SIZE, samples);

    // Process through AGC
    for (int i = 0; i < BLOCK_SIZE; i++) {
        // Convert to appropriate range for AGC
        float micSample = samples[i] * 32768.0f;  // Scale to int16 range
        agc.processSample(micSample);
    }

    // Get results
    float volumeAGC = agc.getSampleAGC();
    int16_t volumeRaw = agc.getSampleRaw();
    float sensitivity = agc.getSensitivity();
    float pressure = agc.estimatePressure(agc.getSampleReal(), 1);  // dmType=1 for I2S

    // Simple peak detection (you would use AudioProcessor for real peak detection)
    static float lastVolume = 0;
    bool peak = (volumeAGC > lastVolume * 1.3f) && (volumeAGC > 50);
    lastVolume = volumeAGC;

    // Print results every 100ms
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 100) {
        Serial.printf("%6.1f | %6d | %6.1f | %6.1f | %s\n",
                     volumeAGC,
                     volumeRaw,
                     sensitivity,
                     pressure,
                     peak ? "PEAK!" : "     ");
        lastPrint = millis();
    }

    // Optional: Show AGC gain multiplier occasionally
    static unsigned long lastGainPrint = 0;
    if (millis() - lastGainPrint > 5000) {
        float multiplier = agc.getMultiplier();
        Serial.printf("\n[AGC] Gain multiplier: %.3f (%.1f dB)\n\n",
                     multiplier, 20.0f * log10(multiplier));
        lastGainPrint = millis();
    }

    // Small delay to not overwhelm serial output
    delay(10);
}

