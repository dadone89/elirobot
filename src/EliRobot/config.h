// config.h
// Contains all constant, pin, and threshold definitions.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>  // Necessary for types like uint32_t, size_t, etc.

// Helper macros for min/max with different types
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// I2S audio pins
#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DATA_PIN 22

// PWM pins for servos
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

// Analog pins for buttons
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Analog pins for photoresistors
#define PHOTORES_PIN_0 36
#define PHOTORES_PIN_1 39

// I2S Configuration
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 8000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN 512
#define VOLUME 0.5f  // Audio playback volume

// Audio file paths in the LittleFS filesystem
#define PRESENTATION_AUDIO "/elirobot.wav"
#define MOD_SEQ_AUDIO "/Frecce_e_centrale.wav"
#define MOD_DANCE_AUDIO "/Adesso_si_balla.wav"
#define MOD_DIRECT_AUDIO "/Telecomando!.wav"
#define MOD_FOLLOW_AUDIO "/Inseguo la luce.wav"

// Movement duration in milliseconds in sequence mode
#define MOVE_DURATION 2000
// ADC value corresponding to maximum speed in follow mode
#define ADC_FOR_MAX_SPEED 500

// Definitions of robot operating modes
#define MODE_STANDBY 0
#define MODE_SEQUENCE 1
#define MODE_DIRECT 2
#define MODE_DANCE 3
#define MODE_FOLLOW 4

// Musical note definitions (frequencies in Hz)
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define TONE_DURATION_MS 400  // Default tone duration for key presses

// Indices for button decoding
#define BTN_NONE -1
#define BTN_C 0   // Center Button (connected to A0)
#define BTN_DL 1  // Down-Left Button (connected to A0)
#define BTN_UL 2  // Up-Left Button (connected to A0)
#define BTN_UR 3  // Up-Right Button (connected to A0)
#define BTN_DR 4  // Down-Right Button (connected to A0)
#define BTN_L 0   // Left Button (connected to A1)
#define BTN_U 1   // Up Button (connected to A1)
#define BTN_R 2   // Right Button (connected to A1)
#define BTN_D 3   // Down Button (connected to A1)

// Thresholds for decoding ADC values (12-bit ADC: 0-4095)
// These thresholds are specific to buttons connected to analog pins.
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// Array of string pointers for audio file names
const char *audioFiles[] = {
  PRESENTATION_AUDIO,
  MOD_SEQ_AUDIO,
  MOD_DANCE_AUDIO,
  MOD_DIRECT_AUDIO,
  MOD_FOLLOW_AUDIO
};

#endif  // CONFIG_H