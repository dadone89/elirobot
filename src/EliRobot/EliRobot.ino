#include <ESP32Servo.h>   // Library for servo control
#include <LittleFS.h>     // Library for LittleFS filesystem management
#include "config.h"       // Includes constant and pin definitions
#include "hardware_io.h"  // Includes servo control and button management functions
#include "audio.h"        // Includes audio playback and tone functions
#include "robot_modes.h"  // Includes functions for mode management

// Inclusione delle nuove librerie Adafruit per il display
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"  // Assicurati di aver installato Adafruit_GC9A01_Library
#include <SPI.h>               // Necessario per il bus SPI
#include "occhio.h"            // La tua immagine dell'occhio

// Pin CONDIVISI per entrambi i Display
#define SHARED_MOSI 23  // MOSI (Master Out Slave In)
#define SHARED_SCLK 18  // SCLK (Serial Clock)
#define SHARED_DC 2     // DC (Data/Command)

// Pin per Display 1
#define TFT1_CS 5   // Chip Select per Display 1
#define TFT1_RST 4  // RST per Display 1

// Pin per Display 2
#define TFT2_CS 13   // Chip Select per Display 2
#define TFT2_RST 27  // RST per Display 2 (o qualsiasi altro GPIO libero)

// Dimensioni del display GC9A01
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// I costruttori della Adafruit_GC9A01A per hardware SPI prendono solo CS, DC, RST.
// Implicamente useranno l'oggetto SPI globale (che è VSPI per ESP32 di default).
Adafruit_GC9A01A tft1(TFT1_CS, SHARED_DC, TFT1_RST);  // Display 1
Adafruit_GC9A01A tft2(TFT2_CS, SHARED_DC, TFT2_RST);  // Display 2

// Servo instances (defined here and declared 'extern' in hardware_io.h)
Servo myservo1;
Servo myservo2;

// Variables to manage previous button states (managed in the main loop)
int lastButtonA0 = BTN_NONE;
int lastButtonA1 = BTN_NONE;

// Funzione per disegnare un'immagine specchiata orizzontalmente
void drawMirroredImage(Adafruit_GFX &display, int x, int y, int w, int h, const uint16_t *data) {
  uint16_t *row_buffer = (uint16_t *)malloc(w * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Errore: Impossibile allocare buffer riga per mirroring.");
    return;
  }

  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      row_buffer[i] = data[j * w + i];
    }

    // Per disegnare una riga specchiata, ricrei una riga invertita e poi usi drawRGBBitmap.
    uint16_t *mirrored_row = (uint16_t *)malloc(w * sizeof(uint16_t));
    if (!mirrored_row) {
      Serial.println("Errore: Impossibile allocare buffer per riga specchiata.");
      free(row_buffer);
      return;
    }
    for (int i = 0; i < w; i++) {
      mirrored_row[i] = row_buffer[w - 1 - i];
    }

    display.drawRGBBitmap(x, y + j, mirrored_row, w, 1);  // Disegna una riga alla volta

    free(mirrored_row);
  }
  free(row_buffer);
}


void setup() {
  Serial.begin(115200);  // Initialize serial communication
  delay(1000);           // Short pause to stabilize

  Serial.println("=== ESP32 Robot Controller + Audio Player ===");

  // Inizializza il bus SPI globale con i pin condivisi.
  // IMPORTANTE: Questo configura i pin per l'oggetto 'SPI' globale (che su ESP32 è VSPI).
  // MISO è -1 perché il display non lo usa. SS è -1 perché i CS sono gestiti individualmente.
  SPI.begin(SHARED_SCLK, -1, SHARED_MOSI, -1);

  // --- CONFIGURAZIONE DISPLAY 1 (Occhio Sinistro) ---
  Serial.println("Inizializzazione Occhio 1...");
  tft1.begin();
  tft1.setRotation(0);
  tft1.fillScreen(0x0000);  // Nero

  int x_offset_1 = (TFT_WIDTH - image_width) / 2;
  int y_offset_1 = (TFT_HEIGHT - image_height) / 2;
  tft1.drawRGBBitmap(x_offset_1, y_offset_1, occhio, image_width, image_height);
  Serial.println("Occhio 1 disegnato.");

  // --- CONFIGURAZIONE DISPLAY 2 (Occhio Destro - specchiato) ---
  Serial.println("Inizializzazione Occhio 2...");
  tft2.begin();
  tft2.setRotation(0);
  tft2.fillScreen(0x0000);

  int x_offset_2 = (TFT_WIDTH - image_width) / 2;
  int y_offset_2 = (TFT_HEIGHT - image_height) / 2;

  drawMirroredImage(tft2, x_offset_2, y_offset_2, image_width, image_height, occhio);

  Serial.println("Occhio 2 (specchiato) disegnato.");
  Serial.println("Display inizializzati e occhi disegnati.");

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