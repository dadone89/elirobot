#include <ESP32Servo.h>
#include <driver/i2s.h>

// --- Config I2S ---
#define I2S_PORT I2S_NUM_0
#define I2S_BCK_IO 22  
#define I2S_WS_IO 26
#define I2S_DO_IO 25
#define BEEP_FREQ 2000     // Hz
#define BEEP_DURATION 100  // ms
#define I2S_SAMPLE_RATE 44100

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

// Debug settings
#define DEBUG_MOVEMENTS false  // Imposta a true per abilitare i messaggi di debug dei movimenti
#define DEBUG_ANALOG false     // Imposta a true per vedere i valori analogici in tempo reale

// Soglie adattate per ESP32 (risoluzione ADC 12-bit: 0-4095)
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// Array per memorizzare la sequenza di movimenti
char moveSequence[10];  // Massimo 10 movimenti
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;
const unsigned long moveDuration = 1000;  // Durata di ogni movimento in ms

// Per evitare ripetizioni dei tasti
bool lastBtnU = false, lastBtnR = false, lastBtnD = false, lastBtnL = false, lastBtnC = false;

// Funzione beep via I2S (DAC esterno, onda quadra) - CORRETTA

void beepI2S(int freq = BEEP_FREQ, int duration = BEEP_DURATION) {
  int samples = I2S_SAMPLE_RATE * duration / 1000;
  int period = I2S_SAMPLE_RATE / freq;
  const int MAX_PERIOD = 64;
  if (period > MAX_PERIOD) period = MAX_PERIOD;
  Serial.print("I2S period: ");
  Serial.println(period);

  int16_t *buf = (int16_t *)malloc(period * 2 * sizeof(int16_t));
  if (!buf) {
    Serial.println("Buffer alloc failed!");
    return;
  }

  int16_t high = 8191;
  int16_t low = -8191;
  for (int i = 0; i < period / 2; ++i) {
    buf[2 * i] = high;
    buf[2 * i + 1] = high;
  }
  for (int i = period / 2; i < period; ++i) {
    buf[2 * i] = low;
    buf[2 * i + 1] = low;
  }

  size_t bytes_written;
  int sent = 0;
  while (sent < samples) {
    int to_write = min(period, samples - sent);
    i2s_write(I2S_PORT, buf, to_write * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
    sent += to_write;
  }
  free(buf);
}

void setup() {
  Serial.begin(115200);

  // Configurazione ADC per migliore stabilità
  analogReadResolution(12);        // Imposta risoluzione a 12-bit
  analogSetAttenuation(ADC_11db);  // Permette lettura fino a 3.3V

  // Configurazione servo con pin specifici
  myservo1.attach(SERVO_PIN_1);
  myservo2.attach(SERVO_PIN_2);

  // Ferma i motori all'avvio
  myservo1.write(90);
  myservo2.write(90);

  Serial.println("ESP32-30pin Sistema pronto! Registra i movimenti e premi C per riprodurli");
  Serial.println("Pin analogici: GPIO32, GPIO33 - Pin servo: GPIO25, GPIO26");
  Serial.println("Risoluzione ADC: 12-bit (0-4095)");
/*
  // --- I2S Init ---
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_IO,
    .ws_io_num = I2S_WS_IO,
    .data_out_num = I2S_DO_IO,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  */
}

void loop() {
  // Lettura valori analogici con media mobile per stabilità
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  decodeButton(analog0, thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
  decodeButton(analog1, thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);

  // Debug valori analogici e tasti rilevati se abilitato
  if (DEBUG_ANALOG) {
    Serial.print("A0(GPIO32): ");
    Serial.print(analog0);
    Serial.print(" | A1(GPIO33): ");
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
    //beepI2S();
    addToSequence('U');
    Serial.println("Movimento UP aggiunto alla sequenza");
  }
  if (btnD && !lastBtnD) {
    //beepI2S();
    addToSequence('D');
    Serial.println("Movimento DOWN aggiunto alla sequenza");
  }
  if (btnL && !lastBtnL) {
    //beepI2S();
    addToSequence('L');
    Serial.println("Movimento LEFT aggiunto alla sequenza");
  }
  if (btnR && !lastBtnR) {
    //beepI2S();
    addToSequence('R');
    Serial.println("Movimento RIGHT aggiunto alla sequenza");
  }
  if (btnC && !lastBtnC) {
    //beepI2S();
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

  if (currentTime - lastMoveTime >= moveDuration) {
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