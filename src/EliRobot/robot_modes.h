// robot_modes.h
// Contiene le dichiarazioni e implementazioni delle funzioni per la gestione delle
// diverse modalità operative del robot.

#ifndef ROBOT_MODES_H
#define ROBOT_MODES_H

#include <Arduino.h>        // Per funzioni come millis(), Serial, delay()
#include "config.h"         // Per le definizioni delle costanti e dei pin
#include "hardware_io.h"    // Per le funzioni di movimento dei servi e gestione pulsanti
#include "audio.h"          // Per le funzioni di riproduzione audio e toni

// Dichiarazioni anticipate delle funzioni
void resetSequence();

// Dichiarazioni e definizioni delle variabili globali relative alle modalità e alla sequenza movimenti
char moveSequence[10];     // Array per memorizzare la sequenza di movimenti
int sequenceIndex = 0;         // Indice corrente nella sequenza di registrazione
bool isPlayingSequence = false;    // Flag per indicare se la sequenza è in riproduzione
int playIndex = 0;             // Indice corrente nella sequenza di riproduzione
unsigned long lastMoveTime = 0; // Timestamp dell'ultimo movimento nella sequenza
static bool firstEntryFollowMode = true; // Flag per la prima entrata nella modalità Follow
int currentMode = MODE_STANDBY;           // Modalità operativa corrente del robot

// ========== FUNZIONI DI UTILITÀ PER LA SEQUENZA ==========

// Aggiunge un movimento alla sequenza.
// La sequenza ha una dimensione massima di 9 movimenti (+1 per il terminatore null).
void addToSequence(char move) {
  if (sequenceIndex < 9) {
    moveSequence[sequenceIndex] = move; // Aggiunge il movimento
    sequenceIndex++;                    // Incrementa l'indice
    moveSequence[sequenceIndex] = '\0'; // Termina la stringa
  } else {
    // Se la sequenza è piena, la resetta per ricominciare
    Serial.println("Sequenza piena, resetto.");
    resetSequence();
    addToSequence(move); // Aggiunge il movimento dopo il reset
  }
}

// Resetta la sequenza di movimenti.
void resetSequence() {
  sequenceIndex = 0;
  moveSequence[0] = '\0'; // Imposta il primo carattere come terminatore null
}

// Avvia la riproduzione della sequenza registrata.
void startPlayback() {
  isPlayingSequence = true; // Imposta il flag di riproduzione
  playIndex = 0;            // Inizia dal primo movimento
  lastMoveTime = millis();  // Registra il tempo di avvio
}

// Esegue il prossimo movimento nella sequenza.
void playSequence() {
  unsigned long currentTime = millis();

  // Controlla se è trascorso il tempo sufficiente per il prossimo movimento
  if (currentTime - lastMoveTime >= MOVE_DURATION) {
    if (playIndex < sequenceIndex) {
      char currentMove = moveSequence[playIndex]; // Ottiene il movimento corrente

      // Esegue il movimento corrispondente
      switch (currentMove) {
        case 'U': moveBackward(); break; // 'U' per Indietro
        case 'D': moveForward(); break;  // 'D' per Avanti
        case 'L': moveLeft(); break;     // 'L' per Sinistra
        case 'R': moveRight(); break;    // 'R' per Destra
      }

      playIndex++;       // Passa al prossimo movimento
      lastMoveTime = currentTime; // Aggiorna il tempo dell'ultimo movimento
    } else {
      // La sequenza è terminata
      isPlayingSequence = false; // Ferma la riproduzione
      stopMotors();              // Ferma i motori
      resetSequence();           // Resetta la sequenza per una nuova registrazione
      delay(1000); // Breve pausa
      if (wavFilesExists) {
        delay(1000);
        playAudioFile(MOD_SEQ_AUDIO); // Riproduce l'audio della modalità sequenza
      }
    }
  }
}

// ========== FUNZIONI DI GESTIONE DELLE MODALITÀ ==========

// Gestisce la logica della modalità Sequenza.
// Permette la registrazione di movimenti e la riproduzione della sequenza.
void handleSequenceMode(int currentButtonA0, int currentButtonA1, int lastButtonA0, int lastButtonA1) {
  if (isPlayingSequence) {
    playSequence(); // Se la sequenza è in riproduzione, continua a eseguirla
  } else {
    // Registrazione movimenti con i tasti direzionali di A1
    if (currentButtonA1 == BTN_U && lastButtonA1 != BTN_U) {
      addToSequence('U');
      playTone(NOTE_C4, TONE_DURATION_MS); // Suona un tono per feedback
      Serial.println("UP aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_D && lastButtonA1 != BTN_D) {
      addToSequence('D');
      playTone(NOTE_D4, TONE_DURATION_MS); // Suona un tono per feedback
      Serial.println("DOWN aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_L && lastButtonA1 != BTN_L) {
      addToSequence('L');
      playTone(NOTE_E4, TONE_DURATION_MS); // Suona un tono per feedback
      Serial.println("LEFT aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_R && lastButtonA1 != BTN_R) {
      addToSequence('R');
      playTone(NOTE_F4, TONE_DURATION_MS); // Suona un tono per feedback
      Serial.println("RIGHT aggiunto alla sequenza");
    }

    // BTN_C (Centrale di A0) serve per eseguire la sequenza registrata
    if (currentButtonA0 == BTN_C && lastButtonA0 != BTN_C) {
      if (sequenceIndex > 0) {
        Serial.println("Avvio riproduzione sequenza");
        playTone(NOTE_G4, TONE_DURATION_MS); // Suona un tono per feedback
        startPlayback(); // Avvia la riproduzione
      } else {
        Serial.println("Nessuna sequenza registrata!");
      }
    }
  }
}

// Gestisce la logica della modalità Telecomando (controllo diretto del robot).
void handleRemoteMode(int currentButtonA1, int lastButtonA1) {
  static unsigned long lastRemoteCommand = 0; // Timestamp dell'ultimo comando ricevuto
  unsigned long currentTime = millis();

  if (currentButtonA1 != BTN_NONE) {
    // Esegue il movimento corrispondente al pulsante premuto
    switch (currentButtonA1) {
      case BTN_U:
        moveBackward();
        Serial.println("Telecomando: Indietro");
        break;
      case BTN_D:
        moveForward();
        Serial.println("Telecomando: Avanti");
        break;
      case BTN_L:
        moveLeft();
        Serial.println("Telecomando: Sinistra");
        break;
      case BTN_R:
        moveRight();
        Serial.println("Telecomando: Destra");
        break;
    }
    lastRemoteCommand = currentTime; // Aggiorna il timestamp dell'ultimo comando
  } else {
    // Se non c'è comando da più di 100ms, ferma i motori per evitare movimenti indesiderati
    if (currentTime - lastRemoteCommand > 100) {
      stopMotors();
    }
  }
}

// Gestisce la logica della modalità Danza.
// Esegue una sequenza predefinita di movimenti per simulare una danza.
void handleDanceMode() {
  static unsigned long lastDanceMove = 0; // Timestamp dell'ultimo passo di danza
  static int danceStep = 0;               // Indice del passo di danza corrente
  static bool danceInitialized = false;   // Flag per l'inizializzazione della danza
  unsigned long currentTime = millis();

  // Inizializza la danza solo una volta per sessione di modalità
  if (!danceInitialized) {
    danceInitialized = true;
    danceStep = 0;
    lastDanceMove = currentTime;
    Serial.println("Inizializzazione modalità danza completata");
    return;   // Esci per dare tempo all'audio di partire
  }

  // Cambia movimento ogni 800ms
  if (currentTime - lastDanceMove > 800) {
    switch (danceStep % 9) { // Cicla attraverso 9 passi di danza
      case 0:
        moveRight();
        Serial.println("Danza: Destra");
        break;
      case 1:
        moveRight();
        Serial.println("Danza: Destra");
        break;
      case 2:
        moveForward();
        Serial.println("Danza: Avanti");
        break;
      case 3:
        moveBackward();
        Serial.println("Danza: Indietro");
        break;
      case 4:
        moveRight();
        Serial.println("Danza: Destra");
        break;
      case 5:
        moveLeft();
        Serial.println("Danza: Sinistra");
        break;
      case 6:
        moveLeft();
        Serial.println("Danza: Sinistra");
        break;
      case 7:
        moveLeft();
        Serial.println("Danza: Sinistra");
        break;
      case 8:
        stopMotors();
        Serial.println("Danza: Pausa");
        break;
    }
    danceStep++;           // Passa al passo successivo
    lastDanceMove = currentTime; // Aggiorna il timestamp
  }

  // Il reset dell'inizializzazione avviene automaticamente quando si cambia modalità
}

// Gestisce la logica della modalità Inseguimento.
// Il robot si muove in base alla luce rilevata dalle fotoresistenze.
void handleFollowMode() {
  // Variabili statiche per memorizzare i valori iniziali delle fotoresistenze
  static int initialPhotores0 = -1;
  static int initialPhotores1 = -1;

  // Se è la prima volta che si entra in questa modalità, scatta un'istantanea
  if (firstEntryFollowMode) {
    initialPhotores0 = getStableAnalogRead(PHOTORES_PIN_0);
    initialPhotores1 = getStableAnalogRead(PHOTORES_PIN_1);
    Serial.printf("Modalità Inseguimento: Snapshot iniziale - Photores0: %d, Photores1: %d\n", initialPhotores0, initialPhotores1);
    firstEntryFollowMode = false;   // Resetta il flag dopo aver scattato l'istantanea
  }

  // Leggi i valori attuali delle fotoresistenze
  int currentPhotores0 = getStableAnalogRead(PHOTORES_PIN_0);
  int currentPhotores1 = getStableAnalogRead(PHOTORES_PIN_1);

  Serial.printf("PHOTORES_PIN_0 (attuale): %d, PHOTORES_PIN_1 (attuale): %d\n", currentPhotores0, currentPhotores1);

  // Mappa il valore della fotoresistenza alla velocità del motore.
  // Assumiamo che 0 sia la massima luce (e quindi massima velocità in avanti)
  // e initialPhotoresX sia il punto di "stop" (o velocità minima).
  // Per myservo1 (motore sinistro): max luce -> 180 (avanti max), initialPhotores0 -> 90 (stop)
  // Per myservo2 (motore destro): max luce -> 0 (avanti max), initialPhotores1 -> 90 (stop)

  int speed0 = map(currentPhotores0, ADC_FOR_MAX_SPEED, initialPhotores0, 180, 90);
  // Limita la velocità per myservo1 tra 90 (stop) e 180 (avanti max)
  speed0 = constrain(speed0, 90, 180);

  int speed1 = map(currentPhotores1, ADC_FOR_MAX_SPEED, initialPhotores1, 0, 90);
  // Limita la velocità per myservo2 tra 0 (avanti max) e 90 (stop)
  speed1 = constrain(speed1, 0, 90);

  // Applica le velocità ai motori
  myservo1.write(speed0);
  myservo2.write(speed1);

  Serial.printf("Velocità Motore 1: %d, Velocità Motore 2: %d\n", speed0, speed1);
}

#endif // ROBOT_MODES_H
