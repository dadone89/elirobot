#include <ESP32Servo.h>

Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

// Pin analogici per ESP32 30-pin (pin ADC disponibili)
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Pin PWM per i servo (GPIO con supporto PWM)
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

// Pin per il buzzer (aggiungi questa definizione)
#define BUZZER_PIN 25

// Debug settings
#define DEBUG_MOVEMENTS false  // Imposta a true per abilitare i messaggi di debug dei movimenti
#define DEBUG_ANALOG false     // Imposta a true per vedere i valori analogici in tempo reale

#define MOVE_DURATION 2000  // Durata di ogni movimento in ms

// Soglie adattate per ESP32 (risoluzione ADC 12-bit: 0-4095)
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// Definizione delle note musicali (frequenze in Hz)
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
#define NOTE_C6 1047
#define REST 0

// Array per memorizzare la sequenza di movimenti
char moveSequence[10];  // Massimo 10 movimenti
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;

// Per evitare ripetizioni dei tasti
bool lastBtnU = false, lastBtnR = false, lastBtnD = false, lastBtnL = false, lastBtnC = false;

// Dichiarazioni delle funzioni
void playNote(int frequency, int duration = 200);
int getStableAnalogRead(int pin);
void addToSequence(char move);
void printSequence();
void resetSequence();
void startPlayback();
void playSequence();
void moveForward();
void moveBackward();
void moveLeft();
void moveRight();
void stopMotors();
void decodeButton(int value, const int thresholds[], bool *level0, bool *level1, bool *level2, bool *level3, bool *level4);

void setup() {
  Serial.begin(115200);

  // Configurazione ADC per migliore stabilità
  analogReadResolution(12);        // Imposta risoluzione a 12-bit
  analogSetAttenuation(ADC_11db);  // Permette lettura fino a 3.3V

  // Configurazione servo con pin specifici
  myservo1.attach(SERVO_PIN_1);
  myservo2.attach(SERVO_PIN_2);

  // Configurazione pin buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // Ferma i motori all'avvio
  myservo1.write(90);
  myservo2.write(90);

  Serial.println("ESP32-30pin Sistema pronto! Registra i movimenti e premi C per riprodurli");
}

void loop() {
  // Lettura valori analogici con media mobile per stabilità
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  decodeButton(analog0, thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
  decodeButton(analog1, thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);

  // Debug valori analogici e tasti rilevati se abilitato
  if (DEBUG_ANALOG) {
    Serial.print("A0(GPIO34): ");
    Serial.print(analog0);
    Serial.print(" | A1(GPIO35): ");
    Serial.print(analog1);
    Serial.print(" | Tasti: ");

    // Mostra i tasti attivi
    bool anyButton = false;
    if (btnU) {
      Serial.print("U ");
      anyButton = true;
    }
    if (btnD) {
      Serial.print("D ");
      anyButton = true;
    }
    if (btnL) {
      Serial.print("L ");
      anyButton = true;
    }
    if (btnR) {
      Serial.print("R ");
      anyButton = true;
    }
    if (btnC) {
      Serial.print("C ");
      anyButton = true;
    }
    if (btnUL) {
      Serial.print("UL ");
      anyButton = true;
    }
    if (btnUR) {
      Serial.print("UR ");
      anyButton = true;
    }
    if (btnDL) {
      Serial.print("DL ");
      anyButton = true;
    }
    if (btnDR) {
      Serial.print("DR ");
      anyButton = true;
    }

    if (!anyButton) Serial.print("Nessuno");
    Serial.println();
  }

  // Se stiamo riproducendo la sequenza
  if (isPlayingSequence) {
    playSequence();
    return;
  }

  // Registrazione dei movimenti (solo sui fronti di salita)
  if (btnU && !lastBtnU) {
    playNote(NOTE_C4);
    addToSequence('U');
    Serial.println("Movimento UP aggiunto alla sequenza");
  }
  if (btnD && !lastBtnD) {
    playNote(NOTE_D4);
    addToSequence('D');
    Serial.println("Movimento DOWN aggiunto alla sequenza");
  }
  if (btnL && !lastBtnL) {
    playNote(NOTE_E4);
    addToSequence('L');
    Serial.println("Movimento LEFT aggiunto alla sequenza");
  }
  if (btnR && !lastBtnR) {
    playNote(NOTE_F4);
    addToSequence('R');
    Serial.println("Movimento RIGHT aggiunto alla sequenza");
  }
  if (btnC && !lastBtnC) {
    playNote(NOTE_G4);
    if (sequenceIndex > 0) {
      Serial.println("Avvio riproduzione sequenza:");
      printSequence();
      startPlayback();
    } else {
      Serial.println("Nessuna sequenza registrata!");
    }
  }

  // Salva lo stato precedente dei tasti
  lastBtnU = btnU;
  lastBtnR = btnR;
  lastBtnD = btnD;
  lastBtnL = btnL;
  lastBtnC = btnC;

  delay(50);
}

// Funzione per lettura analogica stabile (media di 3 letture)
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return sum / 3;
}

void addToSequence(char move) {
  if (sequenceIndex < 9) {  // Lascia spazio per il terminatore
    moveSequence[sequenceIndex] = move;
    sequenceIndex++;
    moveSequence[sequenceIndex] = '\0';  // Terminatore stringa
  } else {
    Serial.println("Sequenza piena! Resetto...");
    resetSequence();
  }
}

void printSequence() {
  Serial.print("Sequenza registrata: ");
  for (int i = 0; i < sequenceIndex; i++) {
    Serial.print(moveSequence[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void resetSequence() {
  sequenceIndex = 0;
  moveSequence[0] = '\0';
  Serial.println("Sequenza resettata");
}

void startPlayback() {
  isPlayingSequence = true;
  playIndex = 0;
  lastMoveTime = millis();
}

void playSequence() {
  unsigned long currentTime = millis();

  if (currentTime - lastMoveTime >= MOVE_DURATION) {
    // Controlla se ci sono ancora movimenti da eseguire
    if (playIndex < sequenceIndex) {
      // Esegui il movimento corrente
      char currentMove = moveSequence[playIndex];
      Serial.print("Eseguendo movimento: ");
      Serial.println(currentMove);

      switch (currentMove) {
        case 'U':
          moveBackward();
          break;
        case 'D':
          moveForward();
          break;
        case 'L':
          moveLeft();
          break;
        case 'R':
          moveRight();
          break;
      }

      playIndex++;
      lastMoveTime = currentTime;
    } else {
      // Sequenza completata - questo controllo ora viene fatto DOPO l'esecuzione
      Serial.println("Sequenza completata!");
      isPlayingSequence = false;
      stopMotors();
      resetSequence();  // Resetta per una nuova sequenza
    }
  }
}

void moveForward() {
  if (DEBUG_MOVEMENTS) Serial.println("DEBUG: Movimento AVANTI");
  myservo1.write(180);  // Motore 1 avanti
  myservo2.write(0);    // Motore 2 avanti
}

void moveBackward() {
  if (DEBUG_MOVEMENTS) Serial.println("DEBUG: Movimento INDIETRO");
  myservo1.write(0);    // Motore 1 indietro
  myservo2.write(180);  // Motore 2 indietro
}

void moveLeft() {
  if (DEBUG_MOVEMENTS) Serial.println("DEBUG: Movimento SINISTRA");
  myservo1.write(0);  // Motore 1 indietro
  myservo2.write(0);  // Motore 2 avanti
}

void moveRight() {
  if (DEBUG_MOVEMENTS) Serial.println("DEBUG: Movimento DESTRA");
  myservo1.write(180);  // Motore 1 avanti
  myservo2.write(180);  // Motore 2 indietro
}

void stopMotors() {
  myservo1.write(90);  // Stop motore 1
  myservo2.write(90);  // Stop motore 2
}

// Funzione che decodifica il valore in base alle soglie
void decodeButton(int value, const int thresholds[], bool *level0, bool *level1, bool *level2, bool *level3, bool *level4) {
  *level0 = false;
  *level1 = false;
  *level2 = false;
  *level3 = false;
  *level4 = false;

  if (value >= thresholds[0] && value < thresholds[1]) {
    *level0 = true;
  } else if (value >= thresholds[1] && value < thresholds[2]) {
    *level1 = true;
  } else if (value >= thresholds[2] && value < thresholds[3]) {
    *level2 = true;
  } else if (value >= thresholds[3] && value < thresholds[4]) {
    *level3 = true;
  } else if (value >= thresholds[4] && value <= thresholds[5]) {
    *level4 = true;
  }
}

// Funzione per suonare una nota
void playNote(int frequency, int duration) {
  if (frequency == REST) {
    delay(duration);
  } else {
    tone(BUZZER_PIN, frequency, duration);
    delay(duration * 1.1);  // Piccola pausa tra le note
  }
}