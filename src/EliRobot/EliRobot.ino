// main.ino
// Main file for the ESP32 Robot Controller program.
// Contains the setup() and loop() functions and manages the general flow.

#include <ESP32Servo.h>   // Library for servo control
#include <LittleFS.h>     // Library for LittleFS filesystem management
#include "config.h"       // Includes constant and pin definitions
#include "hardware_io.h"  // Includes servo control and button management functions
#include "audio.h"        // Includes audio playback and tone functions
#include "robot_modes.h"  // Includes functions for mode management

#define TFT_MOSI 23     // Automatically assigned with ESP8266 if not defined
#define TFT_SCLK 18     // Automatically assigned with ESP8266 if not defined
#define TFT_CS 5        // Chip select control pin
#define TFT_DC 2        // Data Command control pin
#define TFT_RST 4       // Reset pin (could connect to NodeMCU RST, see next line)
#define TFT_WIDTH 240   // Larghezza del display GC9A01
#define TFT_HEIGHT 240  // Altezza del display GC9A01
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#include "occhio.h"  // Include il tuo file di intestazione per l'immagine

// Servo instances (defined here and declared 'extern' in hardware_io.h)
Servo myservo1;
Servo myservo2;

// Variables to manage previous button states (managed in the main loop)
int lastButtonA0 = BTN_NONE;
int lastButtonA1 = BTN_NONE;

void setup() {
  Serial.begin(115200);  // Initialize serial communication
  delay(1000);           // Short pause to stabilize

  Serial.println("=== ESP32 Robot Controller + Audio Player ===");


  // Inizializza il display
  tft.init();
  // Imposta l'orientamento del display (0-3). Prova diverse rotazioni se l'immagine appare ruotata.
  tft.setRotation(0);
  // Pulisci lo schermo, utile per iniziare
  tft.fillScreen(TFT_BLACK);  // Riempi lo schermo di nero

  Serial.println("Display inizializzato. Disegno l'immagine...");

  // Calcola le coordinate per centrare l'immagine (se l'immagine è più piccola del display)
  int x_offset = (TFT_WIDTH - image_width) / 2;
  int y_offset = (TFT_HEIGHT - image_height) / 2;

  // Disegna l'immagine. Ora i riferimenti all'immagine e alle sue dimensioni
  // vengono da occhio.h (che a sua volta si collega a occhio.c)
  tft.pushImage(x_offset, y_offset, image_width, image_height, occhio);

  Serial.println("Immagine dell'occhio disegnata sul display.");


  // Configure and attach servos to pins
  myservo1.attach(SERVO_PIN_1);
  myservo2.attach(SERVO_PIN_2);

  // Stop motors on startup
  stopMotors();

  // Initialize the LittleFS filesystem
  if (!LittleFS.begin(false)) {
    Serial.println("Formatting LittleFS...");
    LittleFS.format();      // If initialization fails, format the filesystem
    LittleFS.begin(false);  // Retry initialization after formatting
  }

  // Initialize the I2S subsystem for audio
  if (!initializeI2S()) {
    Serial.println("I2S Error - Audio disabled");
  } else {
    // Check for the existence of necessary audio files
    wavFilesExists = true;
    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
      if (!checkAudioFileExists(audioFiles[i])) {
        Serial.printf("Error: File '%s' not found!\n", audioFiles[i]);
        wavFilesExists = false;  // If a file is missing, disable WAV playback
      } else {
        Serial.printf("File '%s' found and ready.\n", audioFiles[i]);
      }
    }
  }
  Serial.println("Setup complete!");

  // Play presentation audio if WAV files exist
  if (wavFilesExists) {
    delay(1000);
    playAudioFile(PRESENTATION_AUDIO);
  }
}

void loop() {
  // Read analog values from button pins
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  // Decode analog values into button states
  int currentButtonA0 = decodeButton(analog0, thresholdsA0);
  int currentButtonA1 = decodeButton(analog1, thresholdsA1);

  bool resetMode = false;  // Flag to indicate a mode reset

  // Handle mode changes via directional buttons of A0 (excluding BTN_C)
  if (currentButtonA0 != BTN_NONE && lastButtonA0 != currentButtonA0 && currentButtonA0 != BTN_C) {
    switch (currentButtonA0) {
      case BTN_UR:  // Up-Right button for Sequence mode
        if (currentMode != MODE_SEQUENCE) {
          currentMode = MODE_SEQUENCE;
          resetSequence();            // Reset movement sequence
          isPlayingSequence = false;  // Stop sequence playback
          stopMotors();               // Stop motors
          Serial.println("=== SEQUENCE MODE ACTIVATED ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_SEQ_AUDIO);  // Play sequence mode audio
          }
        } else {
          resetMode = true;  // If mode is already active, prepare for reset
        }
        break;

      case BTN_UL:  // Up-Left button for Remote Control mode
        if (currentMode != MODE_DIRECT) {
          currentMode = MODE_DIRECT;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== REMOTE CONTROL MODE ACTIVATED ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DIRECT_AUDIO);  // Play remote control mode audio
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DL:  // Down-Left button for Dance mode
        if (currentMode != MODE_DANCE) {
          currentMode = MODE_DANCE;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== DANCE MODE ACTIVATED ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DANCE_AUDIO);  // Play dance mode audio
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DR:  // Down-Right button for Follow mode
        if (currentMode != MODE_FOLLOW) {
          currentMode = MODE_FOLLOW;
          isPlayingSequence = false;
          stopMotors();
          firstEntryFollowMode = true;  // Set flag for first entry into Follow mode
          Serial.println("=== FOLLOW MODE ACTIVATED ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_FOLLOW_AUDIO);  // Play follow mode audio
          }
        } else {
          resetMode = true;
        }
        break;
    }

    // If the previous function button was pressed again, exit the current mode and go to standby
    if (resetMode) {
      currentMode = MODE_STANDBY;
      isPlayingSequence = false;
      stopMotors();
      if (wavFilesExists) {
        delay(500);
        playAudioFile(PRESENTATION_AUDIO);  // Play presentation audio
      }
    }
  }

  // Handle different operating modes
  switch (currentMode) {
    case MODE_SEQUENCE:
      handleSequenceMode(currentButtonA0, currentButtonA1, lastButtonA0, lastButtonA1);
      break;

    case MODE_DIRECT:
      handleRemoteMode(currentButtonA1, lastButtonA1);
      break;

    case MODE_DANCE:
      handleDanceMode();
      break;

    case MODE_FOLLOW:
      handleFollowMode();
      break;

    case MODE_STANDBY:
    default:
      // In standby mode, the robot performs no actions
      break;
  }

  // Save current button states for the next cycle
  lastButtonA0 = currentButtonA0;
  lastButtonA1 = currentButtonA1;

  // ========== AUDIO MANAGEMENT ==========
  // Continue processing audio chunks if a file is playing
  if (isPlayingAudio) {
    processAudioChunk();
  }

  delay(50);  // Short delay to prevent CPU overload
}