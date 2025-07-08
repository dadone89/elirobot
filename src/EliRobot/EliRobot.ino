#include <ESP32Servo.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include "driver/adc.h"
#include <esp_task_wdt.h>

// Macro helper per min/max con tipi diversi
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Pin audio I2S
#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DATA_PIN 22

// Pin PWM per i servo
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

// Pin analogici per pulsanti
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Pin analogici per fotoresistenze
#define PHOTORES_PIN_0 36
#define PHOTORES_PIN_1 39

// Configurazione I2S
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 8000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN 512
#define VOLUME 0.5f

// File audio
#define PRESENTATION_AUDIO "/elirobot.wav"
#define MOD_SEQ_AUDIO "/Frecce_e_centrale.wav"
#define MOD_DANCE_AUDIO "/Adesso_si_balla.wav"
#define MOD_DIRECT_AUDIO "/Telecomando!.wav"
#define MOD_FOLLOW_AUDIO "/Inseguo la luce.wav"

// Durata movimento in ms in modalità sequence
#define MOVE_DURATION 2000
// Adc corrispondente a massima velocità in modalità follow
#define ADC_FOR_MAX_SPEED 500

// Modalità di funzionamento
#define MODE_STANDBY 0
#define MODE_SEQUENCE 1
#define MODE_DIRECT 2
#define MODE_DANCE 3
#define MODE_FOLLOW 4

// Note musicali
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392

// Indici pulsanti
#define BTN_NONE -1
#define BTN_C 0   // Centrale (A0)
#define BTN_DL 1  // Down-Left (A0)
#define BTN_UL 2  // Up-Left (A0)
#define BTN_UR 3  // Up-Right (A0)
#define BTN_DR 4  // Down-Right (A0)
#define BTN_L 0   // Left (A1)
#define BTN_U 1   // Up (A1)
#define BTN_R 2   // Right (A1)
#define BTN_D 3   // Down (A1)

// Soglie per ESP32 (12-bit ADC: 0-4095)
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

const char *audioFiles[] = {
  PRESENTATION_AUDIO,
  MOD_SEQ_AUDIO,
  MOD_DANCE_AUDIO,
  MOD_DIRECT_AUDIO,
  MOD_FOLLOW_AUDIO
};

// Instanze servo
Servo myservo1;
Servo myservo2;

// Variabili per sequenza movimenti
char moveSequence[10];
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;

// Variabili per gestire stati precedenti dei pulsanti
int lastButtonA0 = BTN_NONE;
int lastButtonA1 = BTN_NONE;

static bool firstEntryFollowMode = true;  // Indica se è la prima volta che si entra nella modalità Follow
int currentMode = MODE_STANDBY;

// Variabili per audio I2S
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

// Dichiarazione funzioni
int decodeButton(int value, const int thresholds[]);
int getStableAnalogRead(int pin);
void addToSequence(char move);
void resetSequence();
void startPlayback();
void playSequence();
void handleSequenceMode(int currentButtonA0, int currentButtonA1, int lastButtonA0, int lastButtonA1);
void handleRemoteMode(int currentButtonA1, int lastButtonA1);
void handleDanceMode();
void handleFollowMode();
void moveForward();
void moveBackward();
void moveLeft();
void moveRight();
void stopMotors();
bool initializeI2S();
bool playAudioFile(const char *filename);
void processAudioChunk();
void stopAudioPlayback();
bool checkAudioFileExists(const char *filename);
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
    wavFilesExists = true;
    for (int i = 0; i < sizeof(audioFiles) / sizeof(audioFiles[0]); i++) {
      if (!checkAudioFileExists(audioFiles[i])) {
        Serial.printf("Errore: File '%s' non trovato!\n", audioFiles[i]);
        wavFilesExists = false;
      } else {
        Serial.printf("File '%s' trovato e pronto.\n", audioFiles[i]);
      }
    }
  }
  Serial.println("Setup completato!");

  if (wavFilesExists) {
    delay(1000);
    playAudioFile(PRESENTATION_AUDIO);
  }
}

void loop() {
  // leggi analogica per pulsanti
  int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
  int analog1 = getStableAnalogRead(BUTTONS_PIN_1);

  int currentButtonA0 = decodeButton(analog0, thresholdsA0);
  int currentButtonA1 = decodeButton(analog1, thresholdsA1);

  bool resetMode = false;

  // Gestione cambio modalità (solo con i 4 tasti direzionali di A0, non BTN_C)
  if (currentButtonA0 != BTN_NONE && lastButtonA0 != currentButtonA0 && currentButtonA0 != BTN_C) {
    switch (currentButtonA0) {
      case BTN_UR:
        if (currentMode != MODE_SEQUENCE) {
          currentMode = MODE_SEQUENCE;
          resetSequence();
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== MODALITÀ SEQUENZA ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_SEQ_AUDIO);
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_UL:
        if (currentMode != MODE_DIRECT) {
          currentMode = MODE_DIRECT;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== MODALITÀ TELECOMANDO ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DIRECT_AUDIO);
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DL:
        if (currentMode != MODE_DANCE) {
          currentMode = MODE_DANCE;
          isPlayingSequence = false;
          stopMotors();
          Serial.println("=== MODALITÀ DANZA ATTIVATA ===");
          // Riproduci audio di avvio danza immediatamente quando si attiva la modalità
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_DANCE_AUDIO);
          }
        } else {
          resetMode = true;
        }
        break;

      case BTN_DR:
        if (currentMode != MODE_FOLLOW) {
          currentMode = MODE_FOLLOW;
          isPlayingSequence = false;
          stopMotors();
          firstEntryFollowMode = true;
          Serial.println("=== MODALITÀ INSEGUIMENTO ATTIVATA ===");
          if (wavFilesExists) {
            delay(500);
            playAudioFile(MOD_FOLLOW_AUDIO);
          }
        } else {
          resetMode = true;
        }
        break;
    }

    // se è stato premuto di nuovo il tasto della funzione precedente, esci
    if (resetMode) {
      currentMode = MODE_STANDBY;
      isPlayingSequence = false;
      stopMotors();
      if (wavFilesExists) {
        delay(500);
        playAudioFile(PRESENTATION_AUDIO);
      }
    }
  }

  // Gestione delle diverse modalità
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
      // In standby, il robot non fa nulla
      break;
  }

  // Salva stato precedente
  lastButtonA0 = currentButtonA0;
  lastButtonA1 = currentButtonA1;

  // ========== GESTIONE AUDIO ==========
  if (isPlayingAudio) {
    processAudioChunk();
  }

  delay(50);
}

// ========== FUNZIONE DECODIFICA PULSANTI MODIFICATA ==========
int decodeButton(int value, const int thresholds[]) {
  if (value >= thresholds[0] && value < thresholds[1]) return 0;
  else if (value >= thresholds[1] && value < thresholds[2]) return 1;
  else if (value >= thresholds[2] && value < thresholds[3]) return 2;
  else if (value >= thresholds[3] && value < thresholds[4]) return 3;
  else if (value >= thresholds[4] && value <= thresholds[5]) return 4;
  else return BTN_NONE;
}

// ========== FUNZIONE AUDIO SEMPLIFICATA ==========
bool playAudioFile(const char *filename) {
  // Se c'è già un audio in riproduzione, fermalo
  if (isPlayingAudio) {
    stopAudioPlayback();
  }

  // Verifica se l'audio è inizializzato e i file esistono
  if (!i2s_initialized || !wavFilesExists) {
    Serial.println("❌ Audio non disponibile");
    return false;
  }

  // Apri il file
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
  isPlayingAudio = true;

  Serial.printf("🎵 Riproduzione: %s\n", filename);
  return true;
}

// ========== GESTORI MODALITÀ ==========
void handleSequenceMode(int currentButtonA0, int currentButtonA1, int lastButtonA0, int lastButtonA1) {
  if (isPlayingSequence) {
    playSequence();
  } else {
    // Registrazione movimenti con i tasti direzionali di A1
    if (currentButtonA1 == BTN_U && lastButtonA1 != BTN_U) {
      addToSequence('U');
      Serial.println("UP aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_D && lastButtonA1 != BTN_D) {
      addToSequence('D');
      Serial.println("DOWN aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_L && lastButtonA1 != BTN_L) {
      addToSequence('L');
      Serial.println("LEFT aggiunto alla sequenza");
    }
    if (currentButtonA1 == BTN_R && lastButtonA1 != BTN_R) {
      addToSequence('R');
      Serial.println("RIGHT aggiunto alla sequenza");
    }

    // BTN_C serve per eseguire la sequenza registrata
    if (currentButtonA0 == BTN_C && lastButtonA0 != BTN_C) {
      if (sequenceIndex > 0) {
        Serial.println("Avvio riproduzione sequenza");
        startPlayback();
      } else {
        Serial.println("Nessuna sequenza registrata!");
      }
    }
  }
}

void handleRemoteMode(int currentButtonA1, int lastButtonA1) {
  // Modalità telecomando: controllo diretto del robot
  static unsigned long lastRemoteCommand = 0;
  unsigned long currentTime = millis();

  if (currentButtonA1 != BTN_NONE) {
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
    lastRemoteCommand = currentTime;
  } else {
    // Se non c'è comando da più di 100ms, ferma i motori
    if (currentTime - lastRemoteCommand > 100) {
      stopMotors();
    }
  }
}

void handleDanceMode() {
  static unsigned long lastDanceMove = 0;
  static int danceStep = 0;
  static bool danceInitialized = false;
  unsigned long currentTime = millis();

  // Inizializza la danza solo una volta per sessione di modalità
  if (!danceInitialized) {
    danceInitialized = true;
    danceStep = 0;
    lastDanceMove = currentTime;
    Serial.println("Inizializzazione modalità danza completata");
    return;  // Esci per dare tempo all'audio di partire
  }

  // Cambia movimento ogni 800ms
  if (currentTime - lastDanceMove > 800) {
    switch (danceStep % 9) {
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
    danceStep++;
    lastDanceMove = currentTime;
  }

  // Reset dell'inizializzazione quando si esce dalla modalità danza
  // (questo sarà gestito automaticamente quando si cambia modalità)
  if (currentMode != MODE_DANCE) {
    danceInitialized = false;
  }
}

void handleFollowMode() {
  // Variabili statiche per memorizzare i valori iniziali delle fotoresistenze
  static int initialPhotores0 = -1;
  static int initialPhotores1 = -1;

  // Se è la prima volta che si entra in questa modalità, scatta un'istantanea
  if (firstEntryFollowMode) {
    initialPhotores0 = getStableAnalogRead(PHOTORES_PIN_0);
    initialPhotores1 = getStableAnalogRead(PHOTORES_PIN_1);
    Serial.printf("Modalità Inseguimento: Snapshot iniziale - Photores0: %d, Photores1: %d\n", initialPhotores0, initialPhotores1);
    firstEntryFollowMode = false;  // Resetta il flag dopo aver scattato l'istantanea
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

// ========== RESTO DELLE FUNZIONI  ==========
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogReadLegacy(pin);
    delayMicroseconds(100);
  }
  return sum / 3;
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
      delay(1000);
      if (wavFilesExists) {
        delay(1000);
        playAudioFile(MOD_SEQ_AUDIO);
      }
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

bool checkAudioFileExists(const char *filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.printf("File %s non trovato\n", filename);
    return false;
  }

  size_t fileSize = file.size();
  file.close();

  Serial.printf("File %s trovato, dimensione: %zu bytes\n", filename, fileSize);
  return fileSize > sizeof(WAVHeader);
}

void processAudioChunk() {
  if (!currentAudioFile || currentBytesRead >= currentTotalBytes) {
    stopAudioPlayback();
    return;
  }

  size_t bytesToRead = MIN((size_t)DMA_BUF_LEN * (currentHeader.bitsPerSample / 8),
                           currentTotalBytes - currentBytesRead);

  if (bytesToRead == 0) {
    stopAudioPlayback();
    return;
  }

  uint8_t *tempReadBuffer = (uint8_t *)audio_buffer;
  size_t actualRead = currentAudioFile.read(tempReadBuffer, bytesToRead);

  if (actualRead == 0) {
    stopAudioPlayback();
    return;
  }

  size_t actualSamplesRead = actualRead / (currentHeader.bitsPerSample / 8);

  if (currentHeader.numChannels == 1) {
    for (int i = actualSamplesRead - 1; i >= 0; i--) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      int16_t volumeAdjusted = (int16_t)(sample * VOLUME);

      audio_buffer[i * 2] = volumeAdjusted;
      audio_buffer[i * 2 + 1] = volumeAdjusted;
    }
    actualSamplesRead *= 2;
  } else {
    for (int i = 0; i < actualSamplesRead; i++) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      audio_buffer[i] = (int16_t)(sample * VOLUME);
    }
  }

  size_t bytes_written;
  size_t bytesToWrite = actualSamplesRead * sizeof(int16_t);
  esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWrite, &bytes_written, 0);

  if (err == ESP_OK) {
    currentBytesRead += actualRead;
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
  adc1_channel_t channel;
  adc_unit_t unit;

  if (gpio_num == 34) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_6;
  } else if (gpio_num == 35) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_7;
  } else if (gpio_num == 36) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_0;
  } else if (gpio_num == 39) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_3;
  } else {
    return -1;
  }

  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK) {
    return -1;
  }

  if (adc1_config_channel_atten(channel, ADC_ATTEN_DB_11) != ESP_OK) {
    return -1;
  }

  int raw_value = adc1_get_raw(channel);
  return raw_value;
}