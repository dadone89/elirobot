#include <ESP32Servo.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include "driver/adc.h"  // Per le API ADC dell'ESP-IDF
#include <esp_task_wdt.h>

// Macro helper per min/max con tipi diversi
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// File audio
#define PRESENTAZIONE_AUDIO "/elirobot.wav"
#define ISTRUZIONI_AUDIO "/Frecce_e_centrale.wav"
#define START_SEQ_AUDIO "/Adesso_si_balla.wav"

// Pin analogici per ESP32 30-pin
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Pin PWM per i servo
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

#define MOVE_DURATION 2000  // Durata movimento in ms

// Note musicali
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392

// ========== CONFIGURAZIONE AUDIO ==========
#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DATA_PIN 22
#define I2S_NUM I2S_NUM_0

#define SAMPLE_RATE 8000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN 512
#define VOLUME 0.6f

const char *audioFiles[] = {
  PRESENTAZIONE_AUDIO,
  ISTRUZIONI_AUDIO,
  START_SEQ_AUDIO
};

// Soglie per ESP32 (12-bit ADC: 0-4095)
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// ========== CONFIGURAZIONE ROBOT ==========
Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

// Variabili per sequenza movimenti
char moveSequence[10];
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;
bool lastBtnU = false, lastBtnR = false, lastBtnD = false, lastBtnL = false, lastBtnC = false;
bool shouldPlayAudio = false;

static int16_t audio_buffer[DMA_BUF_LEN * 2];
static bool i2s_initialized = false;
bool wavFilesExists = false;
bool isPlayingAudio = false;
File currentAudioFile;
size_t currentBytesRead = 0;
size_t currentTotalBytes = 0;

// Header WAV semplificato
struct WAVHeader {
  char riff[4];
  uint32_t fileSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
};

WAVHeader currentHeader;

// ========== DICHIARAZIONI FUNZIONI ==========
void decodeButton(int value, const int thresholds[], bool *level0, bool *level1, bool *level2, bool *level3, bool *level4);
int getStableAnalogRead(int pin);
void addToSequence(char move);
void resetSequence();
void startPlayback();
void playSequence();
void moveForward();
void moveBackward();
void moveLeft();
void moveRight();
void stopMotors();
bool initializeI2S();
bool startWAVFile(const char *filename);
void processAudioChunk();
void stopAudioPlayback();
bool checkwavFilesExists();
int analogReadLegacy(uint8_t gpio_num);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== ESP32 Robot Controller + Audio Player ===");

  // Configurazione servo
  myservo1.attach(SERVO_PIN_1);
  myservo2.attach(SERVO_PIN_2);

  // Ferma motori
  myservo1.write(90);
  myservo2.write(90);

  // Inizializza filesystem
  if (!LittleFS.begin(false)) {
    Serial.println("Formattazione LittleFS...");
    LittleFS.format();
    LittleFS.begin(false);
  }

  // Inizializza I2S
  if (!initializeI2S()) {
    Serial.println("Errore I2S - Audio disabilitato");
  } else {
    // Verifica esistenza file
    bool wavFilesExists = true;  // Inizializziamo a true e diventa false se un file non c'è

    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
      const char *filename = audioFiles[i];
      if (!checkwavFilesExists(filename)) {
        Serial.printf("Errore: File '%s' non trovato!\n", filename);
        wavFilesExists = false;  // Almeno un file non esiste
      } else {
        Serial.printf("File '%s' trovato e pronto per la riproduzione.\n", filename);
      }
    }
  }
  Serial.println("Setup completato!");
}

void loop() {
  // ========== GESTIONE ROBOT ==========
  // Lettura analogica
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  decodeButton(analog0, thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
  decodeButton(analog1, thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);

  // Gestione sequenza
  if (isPlayingSequence) {
    playSequence();
  } else {
    // Registrazione movimenti
    if (btnU && !lastBtnU) {
      addToSequence('U');
      Serial.println("UP aggiunto");
    }
    if (btnD && !lastBtnD) {
      addToSequence('D');
      Serial.println("DOWN aggiunto");
    }
    if (btnL && !lastBtnL) {
      addToSequence('L');
      Serial.println("LEFT aggiunto");
    }
    if (btnR && !lastBtnR) {
      addToSequence('R');
      Serial.println("RIGHT aggiunto");
    }
    if (btnC && !lastBtnC) {
      if (sequenceIndex > 0) {
        Serial.println("Avvio riproduzione sequenza");
        startPlayback();
      } else {
        Serial.println("Nessuna sequenza registrata!");
      }
    }
  }

  // Salva stato precedente
  lastBtnU = btnU;
  lastBtnR = btnR;
  lastBtnD = btnD;
  lastBtnL = btnL;
  lastBtnC = btnC;

  // ========== GESTIONE AUDIO ==========
  if (i2s_initialized && wavFilesExists && shouldPlayAudio && !isPlayingAudio) {
    // Avvia riproduzione del file
    if (startWAVFile(START_SEQ_AUDIO)) {
      isPlayingAudio = true;
      shouldPlayAudio = false;  // Reset del flag
      Serial.printf("Avviata riproduzione: %s\n", START_SEQ_AUDIO);
    }
  }

  if (isPlayingAudio) {
    // Processa chunk audio corrente
    processAudioChunk();
  }

  delay(50);  // Piccolo delay per non sovraccaricare
}

// ========== FUNZIONI ROBOT ==========
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogReadLegacy(pin);
    delayMicroseconds(100);
  }
  return sum / 3;
}

void decodeButton(int value, const int thresholds[], bool *level0, bool *level1, bool *level2, bool *level3, bool *level4) {
  *level0 = *level1 = *level2 = *level3 = *level4 = false;

  if (value >= thresholds[0] && value < thresholds[1]) *level0 = true;
  else if (value >= thresholds[1] && value < thresholds[2]) *level1 = true;
  else if (value >= thresholds[2] && value < thresholds[3]) *level2 = true;
  else if (value >= thresholds[3] && value < thresholds[4]) *level3 = true;
  else if (value >= thresholds[4] && value <= thresholds[5]) *level4 = true;
}

void addToSequence(char move) {
  if (sequenceIndex < 9) {
    moveSequence[sequenceIndex] = move;
    sequenceIndex++;
    moveSequence[sequenceIndex] = '\0';
  } else {
    resetSequence();
  }
}

void resetSequence() {
  sequenceIndex = 0;
  moveSequence[0] = '\0';
}

void startPlayback() {
  isPlayingSequence = true;
  playIndex = 0;
  lastMoveTime = millis();

  // Attiva la riproduzione audio
  if (i2s_initialized && wavFilesExists) {
    shouldPlayAudio = true;
    Serial.println("Audio programmato per l'avvio");
  }
}

void playSequence() {
  unsigned long currentTime = millis();

  if (currentTime - lastMoveTime >= MOVE_DURATION) {
    if (playIndex < sequenceIndex) {
      char currentMove = moveSequence[playIndex];

      switch (currentMove) {
        case 'U': moveBackward(); break;
        case 'D': moveForward(); break;
        case 'L': moveLeft(); break;
        case 'R': moveRight(); break;
      }

      playIndex++;
      lastMoveTime = currentTime;
    } else {
      isPlayingSequence = false;
      stopMotors();
      resetSequence();
    }
  }
}

void moveForward() {
  myservo1.write(180);
  myservo2.write(0);
}

void moveBackward() {
  myservo1.write(0);
  myservo2.write(180);
}

void moveLeft() {
  myservo1.write(0);
  myservo2.write(0);
}

void moveRight() {
  myservo1.write(180);
  myservo2.write(180);
}

void stopMotors() {
  myservo1.write(90);
  myservo2.write(90);
}

// ========== FUNZIONI AUDIO ==========
bool initializeI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_DATA_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM, &pin_config) != ESP_OK) return false;
  if (i2s_start(I2S_NUM) != ESP_OK) return false;

  i2s_zero_dma_buffer(I2S_NUM);
  i2s_initialized = true;
  return true;
}

bool checkwavFilesExists(const char *filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.printf("File %s non trovato\n", filename);
    return false;
  }

  size_t fileSize = file.size();
  file.close();

  Serial.printf("File %s trovato, dimensione: %zu bytes\n", START_SEQ_AUDIO, fileSize);
  return fileSize > sizeof(WAVHeader);  // Verifica che il file sia abbastanza grande per contenere almeno l'header
}

bool startWAVFile(const char *filename) {
  if (currentAudioFile) {
    currentAudioFile.close();
  }

  currentAudioFile = LittleFS.open(filename, "r");
  if (!currentAudioFile) {
    Serial.printf("❌ Impossibile aprire file: %s\n", filename);
    return false;
  }

  // Leggi header WAV
  if (currentAudioFile.read((uint8_t *)&currentHeader, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
    Serial.println("❌ Errore lettura header WAV");
    currentAudioFile.close();
    return false;
  }

  // Verifica formato
  if (memcmp(currentHeader.riff, "RIFF", 4) != 0 || memcmp(currentHeader.wave, "WAVE", 4) != 0 || currentHeader.audioFormat != 1) {
    Serial.println("❌ Formato WAV non supportato (solo PCM)");
    currentAudioFile.close();
    return false;
  }

  // Posiziona all'inizio dei dati audio
  currentAudioFile.seek(sizeof(WAVHeader));

  currentTotalBytes = currentHeader.dataSize;
  currentBytesRead = 0;

  Serial.printf("🎵 Iniziata riproduzione: %s\n", filename);
  Serial.printf("📊 Sample Rate: %u Hz, Canali: %u, Bit depth: %u\n",
                currentHeader.sampleRate, currentHeader.numChannels, currentHeader.bitsPerSample);

  return true;
}

void processAudioChunk() {
  if (!currentAudioFile || currentBytesRead >= currentTotalBytes) {
    stopAudioPlayback();
    return;
  }

  // Calcola quanti byte leggere
  size_t bytesToRead = MIN((size_t)DMA_BUF_LEN * (currentHeader.bitsPerSample / 8),
                           currentTotalBytes - currentBytesRead);

  if (bytesToRead == 0) {
    stopAudioPlayback();
    return;
  }

  // Leggi chunk
  uint8_t *tempReadBuffer = (uint8_t *)audio_buffer;
  size_t actualRead = currentAudioFile.read(tempReadBuffer, bytesToRead);

  if (actualRead == 0) {
    stopAudioPlayback();
    return;
  }

  // Processa campioni
  size_t actualSamplesRead = actualRead / (currentHeader.bitsPerSample / 8);

  if (currentHeader.numChannels == 1) {
    // Mono -> Stereo con volume
    for (int i = actualSamplesRead - 1; i >= 0; i--) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      int16_t volumeAdjusted = (int16_t)(sample * VOLUME);

      audio_buffer[i * 2] = volumeAdjusted;
      audio_buffer[i * 2 + 1] = volumeAdjusted;
    }
    actualSamplesRead *= 2;
  } else {
    // Stereo con volume
    for (int i = 0; i < actualSamplesRead; i++) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      audio_buffer[i] = (int16_t)(sample * VOLUME);
    }
  }

  // Scrivi su I2S
  size_t bytes_written;
  size_t bytesToWrite = actualSamplesRead * sizeof(int16_t);
  esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWrite, &bytes_written, 0);

  if (err == ESP_OK) {
    currentBytesRead += actualRead;

    // Stampa progresso ogni 10KB
    if (currentBytesRead % 10240 == 0) {
      float progress = (float)currentBytesRead * 100.0 / currentTotalBytes;
      Serial.printf("📊 Progresso: %.1f%%\n", progress);
    }
  } else {
    Serial.printf("❌ Errore I2S write: %s\n", esp_err_to_name(err));
    stopAudioPlayback();
  }
}

void stopAudioPlayback() {
  if (currentAudioFile) {
    currentAudioFile.close();
  }

  isPlayingAudio = false;
  Serial.println("✅ Riproduzione completata");
}

int analogReadLegacy(uint8_t gpio_num) {
  // Cambiato il tipo di 'channel' a adc1_channel_t
  adc1_channel_t channel;
  adc_unit_t unit;  // Manteniamo 'unit' per completezza, anche se qui usiamo solo ADC1

  if (gpio_num == 34) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_6;  // GPIO34
  } else if (gpio_num == 35) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_7;  // GPIO35
  } else {
    return -1;  // Pin non supportato
  }

  // Configura la larghezza in bit (risoluzione) per ADC1
  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK) {
    return -1;
  }

  // Configura l'attenuazione fissa a 11dB (range 0-3.9V circa) per il canale ADC1
  if (adc1_config_channel_atten(channel, ADC_ATTEN_DB_11) != ESP_OK) {
    return -1;
  }

  // Effettua la lettura RAW dal canale ADC1
  int raw_value = adc1_get_raw(channel);

  return raw_value;
}