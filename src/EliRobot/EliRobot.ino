// main.ino
// File principale del programma ESP32 Robot Controller.
// Contiene le funzioni setup() e loop() e gestisce il flusso generale.

#include <ESP32Servo.h> // Libreria per il controllo dei servo
#include <LittleFS.h>   // Libreria per la gestione del filesystem LittleFS
#include "config.h"     // Include le definizioni delle costanti e dei pin
#include "hardware_io.h" // Include le funzioni di controllo dei servi e gestione pulsanti
#include "audio.h"      // Include le funzioni di riproduzione audio e toni
#include "robot_modes.h" // Include le funzioni per la gestione delle modalità

// Instanze servo (definite qui e dichiarate 'extern' in hardware_io.h)
Servo myservo1;
Servo myservo2;

// Variabili per gestire stati precedenti dei pulsanti (gestite nel loop principale)
int lastButtonA0 = BTN_NONE;
int lastButtonA1 = BTN_NONE;

void setup() {
  Serial.begin(115200); // Inizializza la comunicazione seriale
  delay(1000); // Breve pausa per stabilizzare

  Serial.println("=== ESP32 Robot Controller + Audio Player ===");

  // Configurazione e attacco dei servi ai pin
  myservo1.attach(SERVO_PIN_1);
  myservo2.attach(SERVO_PIN_2);

  // Ferma i motori all'avvio
  stopMotors();

  // Inizializza il filesystem LittleFS
  if (!LittleFS.begin(false)) {
    Serial.println("Formattazione LittleFS...");
    LittleFS.format(); // Se l'inizializzazione fallisce, formatta il filesystem
    LittleFS.begin(false); // Riprova l'inizializzazione dopo la formattazione
  }

  // Inizializza il sottosistema I2S per l'audio
  if (!initializeI2S()) {
    Serial.println("Errore I2S - Audio disabilitato");
  } else {
    // Verifica l'esistenza dei file audio necessari
    wavFilesExists = true;
    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
      if (!checkAudioFileExists(audioFiles[i])) {
        Serial.printf("Errore: File '%s' non trovato!\n", audioFiles[i]);
        wavFilesExists = false; // Se un file manca, disabilita la riproduzione WAV
      } else {
        Serial.printf("File '%s' trovato e pronto.\n", audioFiles[i]);
      }
    }
  }
  Serial.println("Setup completato!");

  // Riproduci l'audio di presentazione se i file WAV esistono
  if (wavFilesExists) {
    delay(1000);
    playAudioFile(PRESENTATION_AUDIO);
  }
}

void loop() {
  // Leggi i valori analogici dai pin dei pulsanti
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  // Decodifica i valori analogici in stati dei pulsanti
  int currentButtonA0 = decodeButton(analog0, thresholdsA0);
  int currentButtonA1 = decodeButton(analog1, thresholdsA1);

  bool resetMode = false; // Flag per indicare un reset della modalità

  // Gestione del cambio di modalità tramite i pulsanti direzionali di A0 (escluso BTN_C)
  if (currentButtonA0 != BTN_NONE && lastButtonA0 != currentButtonA0 && currentButtonA0 != BTN_C) {
    switch (currentButtonA0) {
      case BTN_UR: // Pulsante Up-Right per la modalità Sequenza
        if (currentMode != MODE_SEQUENCE) {
          currentMode = MODE_SEQUENCE;
          resetSequence(); // Resetta la sequenza movimenti
          isPlayingSequence = false; // Ferma la riproduzione della sequenza
          stopMotors(); // Ferma i motori
          Serial.println("=== MODALITÀ SEQUENZA ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_SEQ_AUDIO); // Riproduci audio modalità sequenza
          }
        } else {
          resetMode = true; // Se la modalità è già attiva, prepara il reset
        }
        break;

      case BTN_UL: // Pulsante Up-Left per la modalità Telecomando
        if (currentMode != MODE_DIRECT) {
          currentMode = MODE_DIRECT;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== MODALITÀ TELECOMANDO ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DIRECT_AUDIO); // Riproduci audio modalità telecomando
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DL: // Pulsante Down-Left per la modalità Danza
        if (currentMode != MODE_DANCE) {
          currentMode = MODE_DANCE;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== MODALITÀ DANZA ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DANCE_AUDIO); // Riproduci audio modalità danza
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DR: // Pulsante Down-Right per la modalità Inseguimento
        if (currentMode != MODE_FOLLOW) {
          currentMode = MODE_FOLLOW;
          isPlayingSequence = false;
          stopMotors();
          firstEntryFollowMode = true; // Imposta il flag per la prima entrata nella modalità Follow
          Serial.println("=== MODALITÀ INSEGUIMENTO ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_FOLLOW_AUDIO); // Riproduci audio modalità inseguimento
          }
        } else {
          resetMode = true;
        }
        break;
    }

    // Se è stato premuto di nuovo il tasto della funzione precedente, esci dalla modalità corrente e vai in standby
    if (resetMode) {
      currentMode = MODE_STANDBY;
      isPlayingSequence = false;
      stopMotors();
      if (wavFilesExists) {
        delay(500);
        playAudioFile(PRESENTATION_AUDIO); // Riproduci audio di presentazione
      }
    }
  }

  // Gestione delle diverse modalità operative
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
      // In modalità standby, il robot non esegue azioni
      break;
  }

  // Salva lo stato attuale dei pulsanti per il prossimo ciclo
  lastButtonA0 = currentButtonA0;
  lastButtonA1 = currentButtonA1;

  // ========== GESTIONE AUDIO ==========
  // Continua a processare i chunk audio se un file è in riproduzione
  if (isPlayingAudio) {
    processAudioChunk();
  }

  delay(50); // Breve ritardo per evitare sovraccarico della CPU
}
