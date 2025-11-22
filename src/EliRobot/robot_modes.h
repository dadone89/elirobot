// robot_modes.h
// Contains declarations and implementations of functions for managing the
// different operating modes of the robot.

#ifndef ROBOT_MODES_H
#define ROBOT_MODES_H

#include <Arduino.h>      // For functions like millis(), Serial, delay()
#include "config.h"       // For constant and pin definitions
#include "hardware_io.h"  // For servo movement and button management functions
#include "audio.h"        // For audio playback and tone functions
#include "occhio.h"       // Includes functions for eyes management

// Forward declarations of functions
void resetSequence();

// Declarations and definitions of global variables related to modes and movement sequence
char moveSequence[10];                    // Array to store the movement sequence
int sequenceIndex = 0;                    // Current index in the recording sequence
bool isPlayingSequence = false;           // Flag to indicate if the sequence is playing
int playIndex = 0;                        // Current index in the playback sequence
unsigned long lastMoveTime = 0;           // Timestamp of the last movement in the sequence
static bool firstEntryFollowMode = true;  // Flag for the first entry into Follow mode
int currentMode = MODE_STANDBY;           // Current operating mode of the robot

// ========== SEQUENCE UTILITY FUNCTIONS ==========

// Adds a movement to the sequence.
// The sequence has a maximum size of 9 movements (+1 for the null terminator).
void addToSequence(char move) {
  const int MAX_MOVES = 9; // Numero massimo di movimenti (dimensione array - 1)

  if (sequenceIndex < MAX_MOVES) {
    // Caso 1: La sequenza NON è ancora piena.
    moveSequence[sequenceIndex] = move; // Aggiungi il movimento
    sequenceIndex++;                    // Incrementa l'indice
  } else {
    // Caso 2: La sequenza è piena (sequenceIndex == MAX_MOVES).
    Serial.println("Sequence full, shifting elements.");

    // 1. Sposta tutti gli elementi a sinistra di una posizione.
    // L'elemento in moveSequence[0] viene sovrascritto (perso).
    // Usiamo un semplice loop per lo spostamento. In C++ si potrebbe usare std::memmove,
    // ma in Arduino un loop for è spesso più semplice e sicuro.
    for (int i = 0; i < MAX_MOVES - 1; i++) {
      moveSequence[i] = moveSequence[i + 1];
    }

    // 2. Aggiungi il nuovo movimento all'ultima posizione valida (indice MAX_MOVES - 1 = 8).
    moveSequence[MAX_MOVES - 1] = move;
    
    // NOTA: Non serve aggiornare sequenceIndex perché rimane a MAX_MOVES (9) 
    // e funge da indicatore che la sequenza è piena.
  }

  // Assicurati che la sequenza sia sempre terminata correttamente
  // L'indice 9 (moveSequence[9]) è sempre il terminatore nullo, 
  // perché MAX_MOVES è 9, e l'array è di dimensione 10.
  moveSequence[MAX_MOVES] = '\0';
}

// Resets the movement sequence.
void resetSequence() {
  sequenceIndex = 0;
  moveSequence[0] = '\0';  // Set the first character as null terminator
}

// Starts playback of the recorded sequence.
void startPlayback() {
  isPlayingSequence = true;  // Set the playing flag
  playIndex = 0;             // Start from the first movement
  lastMoveTime = millis();   // Record the start time
}

// Executes the next movement in the sequence.
void playSequence() {
  unsigned long currentTime = millis();

  // Check if enough time has passed for the next movement
  if (currentTime - lastMoveTime >= MOVE_DURATION) {
    if (playIndex < sequenceIndex) {
      char currentMove = moveSequence[playIndex];  // Get the current movement

      // Execute the corresponding movement
      switch (currentMove) {
        case 'U': moveBackward(); break;  // 'U' for Backward
        case 'D': moveForward(); break;   // 'D' for Forward
        case 'L': moveLeft(); break;      // 'L' for Left
        case 'R': moveRight(); break;     // 'R' for Right
      }

      playIndex++;                 // Move to the next movement
      lastMoveTime = currentTime;  // Update the last movement time
    } else {
      // The sequence has ended
      isPlayingSequence = false;  // Stop playback
      stopMotors();               // Stop motors
      resetSequence();            // Reset the sequence for a new recording
      delay(1000);                // Short pause
      if (wavFilesExists) {
        delay(1000);
        drawNormalImage(tft1, happy, 0);
        drawMirroredImage(tft2, happy, -20);
        playAudioFile(MOD_SEQ_AUDIO);  // Play sequence mode audio
      }
    }
  }
}

// ========== MODE MANAGEMENT FUNCTIONS ==========

// Handles the logic for Sequence mode.
// Allows recording movements and playing back the sequence.
void handleSequenceMode(int currentButtonA0, int currentButtonA1, int lastButtonA0, int lastButtonA1) {
  if (isPlayingSequence) {
    playSequence();  // If the sequence is playing, continue executing it
  } else {
    // Record movements with directional keys of A1
    if (currentButtonA1 == BTN_U && lastButtonA1 != BTN_U) {
      addToSequence('U');
      playTone(NOTE_C4, TONE_DURATION_MS);  // Play a tone for feedback
      Serial.println("UP added to sequence");
    }
    if (currentButtonA1 == BTN_D && lastButtonA1 != BTN_D) {
      addToSequence('D');
      playTone(NOTE_D4, TONE_DURATION_MS);  // Play a tone for feedback
      Serial.println("DOWN added to sequence");
    }
    if (currentButtonA1 == BTN_L && lastButtonA1 != BTN_L) {
      addToSequence('L');
      playTone(NOTE_E4, TONE_DURATION_MS);  // Play a tone for feedback
      Serial.println("LEFT added to sequence");
    }
    if (currentButtonA1 == BTN_R && lastButtonA1 != BTN_R) {
      addToSequence('R');
      playTone(NOTE_F4, TONE_DURATION_MS);  // Play a tone for feedback
      Serial.println("RIGHT added to sequence");
    }
    // BTN_C (Center of A0) is used to execute the recorded sequence
    if (currentButtonA0 == BTN_C && lastButtonA0 != BTN_C) {
      if (sequenceIndex > 0) {
        Serial.println("Starting sequence playback");
        playTone(NOTE_G4, TONE_DURATION_MS);  // Play a tone for feedback
        drawNormalImage(tft1, occhio, 0);
        drawMirroredImage(tft2, occhio, -20);
        startPlayback();                      // Start playback
      } else {
        Serial.println("No sequence recorded!");
      }
    }
  }
}

// Handles the logic for Remote Control mode (direct robot control).
void handleRemoteMode(int currentButtonA1, int lastButtonA1) {
  static unsigned long lastRemoteCommand = 0;  // Timestamp of the last received command
  unsigned long currentTime = millis();

  if (currentButtonA1 != BTN_NONE) {
    // Execute the movement corresponding to the pressed button
    switch (currentButtonA1) {
      case BTN_U:
        moveBackward();
        Serial.println("Remote: Backward");
        break;
      case BTN_D:
        moveForward();
        Serial.println("Remote: Forward");
        break;
      case BTN_L:
        moveLeft();
        Serial.println("Remote: Left");
        break;
      case BTN_R:
        moveRight();
        Serial.println("Remote: Right");
        break;
    }
    lastRemoteCommand = currentTime;  // Update the timestamp of the last command
  } else {
    // If no command for more than 100ms, stop motors to prevent unintended movements
    if (currentTime - lastRemoteCommand > 100) {
      stopMotors();
    }
  }
}

// Handles the logic for Dance mode.
// Executes a predefined sequence of movements to simulate a dance.
void handleDanceMode() {
  static unsigned long lastDanceMove = 0;  // Timestamp of the last dance step
  static int danceStep = 0;                // Current dance step index
  static bool danceInitialized = false;    // Flag for dance initialization
  unsigned long currentTime = millis();

  // Initialize dance only once per mode session
  if (!danceInitialized) {
    danceInitialized = true;
    danceStep = 0;
    lastDanceMove = currentTime;
    Serial.println("Dance mode initialization complete");
    return;  // Exit to allow audio to start
  }

  // Change movement every 800ms
  if (currentTime - lastDanceMove > 800) {
    switch (danceStep % 9) {  // Cycle through 9 dance steps
      case 0:
        moveRight();
        Serial.println("Dance: Right");
        break;
      case 1:
        moveRight();
        Serial.println("Dance: Right");
        break;
      case 2:
        moveForward();
        Serial.println("Dance: Forward");
        break;
      case 3:
        drawFlippedImage(tft1, happy, 0);
        drawMirroredAndFlippedImage(tft2, happy, -20);
        moveBackward();
        Serial.println("Dance: Backward");
        break;
      case 4:
        moveRight();
        Serial.println("Dance: Right");
        break;
      case 5:
        moveLeft();
        Serial.println("Dance: Left");
        break;
      case 6:
        moveLeft();
        Serial.println("Dance: Left");
        break;
      case 7:
        drawNormalImage(tft1, occhio, 0);
        drawMirroredImage(tft2, occhio, -20);
        moveLeft();
        Serial.println("Dance: Left");
        break;
      case 8:
        stopMotors();
        Serial.println("Dance: Pause");
        break;
    }
    danceStep++;                  // Move to the next step
    lastDanceMove = currentTime;  // Update the timestamp
  }

  // Initialization reset happens automatically when changing modes
}

// Handles the logic for Follow mode.
// The robot moves based on light detected by photoresistors.
void handleFollowMode() {
  // Static variables to store initial photoresistor values
  static int initialPhotores0 = -1;
  static int initialPhotores1 = -1;

  // If it's the first time entering this mode, take a snapshot
  if (firstEntryFollowMode) {
    initialPhotores0 = getStableAnalogRead(PHOTORES_PIN_0);
    initialPhotores1 = getStableAnalogRead(PHOTORES_PIN_1);
    Serial.printf("Follow Mode: Initial Snapshot - Photores0: %d, Photores1: %d\n", initialPhotores0, initialPhotores1);
    firstEntryFollowMode = false;  // Reset the flag after taking the snapshot
  }

  // Read current photoresistor values
  int currentPhotores0 = getStableAnalogRead(PHOTORES_PIN_0);
  int currentPhotores1 = getStableAnalogRead(PHOTORES_PIN_1);

  Serial.printf("PHOTORES_PIN_0 (current): %d, PHOTORES_PIN_1 (current): %d\n", currentPhotores0, currentPhotores1);

  // Map the photoresistor value to motor speed.
  // Assume 0 is maximum light (and thus maximum forward speed)
  // and initialPhotoresX is the "stop" point (or minimum speed).
  // For myservo1 (left motor): max light -> 180 (max forward), initialPhotores0 -> 90 (stop)
  // For myservo2 (right motor): max light -> 0 (max forward), initialPhotores1 -> 90 (stop)

  int speed0 = map(currentPhotores0, ADC_FOR_MAX_SPEED, initialPhotores0, 180, 90);
  // Limit speed for myservo1 between 90 (stop) and 180 (max forward)
  speed0 = constrain(speed0, 90, 180);

  int speed1 = map(currentPhotores1, ADC_FOR_MAX_SPEED, initialPhotores1, 0, 90);
  // Limit speed for myservo2 between 0 (max forward) and 90 (stop)
  speed1 = constrain(speed1, 0, 90);

  // Apply speeds to motors
  myservo1.write(speed0);
  myservo2.write(speed1);

  Serial.printf("Motor 1 Speed: %d, Motor 2 Speed: %d\n", speed0, speed1);
}

#endif  // ROBOT_MODES_H