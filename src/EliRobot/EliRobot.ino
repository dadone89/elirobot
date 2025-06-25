#include <ESP32Servo.h>
#include <LittleFS.h>
#include <driver/i2s.h>
//#include <vector>

// ========== CONFIGURAZIONE ROBOT (CORE 0) ==========
Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

// Pin analogici per ESP32 30-pin
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Pin PWM per i servo
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

#define MOVE_DURATION 2000  // Durata movimento in ms

// Soglie per ESP32 (12-bit ADC: 0-4095)
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// Note musicali
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392

// Variabili per sequenza movimenti
char moveSequence[10];
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;
bool lastBtnU = false, lastBtnR = false, lastBtnD = false, lastBtnL = false, lastBtnC = false;

// ========== CONFIGURAZIONE AUDIO (CORE 1) ==========
#define I2S_BCK_PIN     26
#define I2S_WS_PIN      25
#define I2S_DATA_PIN    22 
#define I2S_NUM         I2S_NUM_0

#define SAMPLE_RATE     8000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DMA_BUF_COUNT   4
#define DMA_BUF_LEN     512
#define VOLUME          0.3f

static int16_t audio_buffer[DMA_BUF_LEN * 2];
static bool i2s_initialized = false;
std::vector<String> wavFiles;
unsigned int currentFileIndex = 0;

// Header WAV semplificato
struct WAVHeader {
    char riff[4], wave[4], fmt[4], data[4];
    uint32_t fileSize, fmtSize, sampleRate, byteRate, dataSize;
    uint16_t audioFormat, numChannels, blockAlign, bitsPerSample;
};

// ========== TASK HANDLES ==========
TaskHandle_t RobotTask;
TaskHandle_t AudioTask;

// ========== DICHIARAZIONI FUNZIONI ==========
void robotTaskCode(void * parameter);
void audioTaskCode(void * parameter);
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
void playNote(int frequency, int duration = 200);
bool initializeI2S();
bool playWAVFile(const char* filename);
//std::vector<String> getWAVFiles();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== ESP32 Dual Core: Robot Controller + Audio Player ===");
  
  // Configurazione ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
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
  
  // Crea task per core 0 (Robot)
  xTaskCreatePinnedToCore(
    robotTaskCode,   // Funzione task
    "RobotTask",     // Nome task
    4096,            // Stack size
    NULL,            // Parametri
    1,               // Priorità
    &RobotTask,      // Handle task
    0                // Core 0
  );
  
  // Crea task per core 1 (Audio)
  xTaskCreatePinnedToCore(
    audioTaskCode,   // Funzione task
    "AudioTask",     // Nome task
    8192,            // Stack size maggiore per audio
    NULL,            // Parametri
    1,               // Priorità
    &AudioTask,      // Handle task
    1                // Core 1
  );
  
  Serial.println("Tasks creati sui due core!");
}

void loop() {
  // Loop principale vuoto - tutto gestito dai task
  delay(1000);
}

// ========== TASK ROBOT (CORE 0) ==========
void robotTaskCode(void * parameter) {
  Serial.println("Robot Task avviato su Core 0");
  
  for(;;) {
    // Lettura analogica
    int analog0 = getStableAnalogRead(BUTTONS_PIN_0);
    int analog1 = getStableAnalogRead(BUTTONS_PIN_1);
    
    decodeButton(analog0, thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
    decodeButton(analog1, thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);
    
    // Gestione sequenza
    if (isPlayingSequence) {
      playSequence();
      delay(50);
      continue;
    }
    
    // Registrazione movimenti
    if (btnU && !lastBtnU) {
      playNote(NOTE_C4);
      addToSequence('U');
      Serial.println("UP aggiunto");
    }
    if (btnD && !lastBtnD) {
      playNote(NOTE_D4);
      addToSequence('D');
      Serial.println("DOWN aggiunto");
    }
    if (btnL && !lastBtnL) {
      playNote(NOTE_E4);
      addToSequence('L');
      Serial.println("LEFT aggiunto");
    }
    if (btnR && !lastBtnR) {
      playNote(NOTE_F4);
      addToSequence('R');
      Serial.println("RIGHT aggiunto");
    }
    if (btnC && !lastBtnC) {
      playNote(NOTE_G4);
      if (sequenceIndex > 0) {
        Serial.println("Avvio riproduzione sequenza");
        startPlayback();
      } else {
        Serial.println("Nessuna sequenza registrata!");
      }
    }
    
    // Salva stato precedente
    lastBtnU = btnU; lastBtnR = btnR; lastBtnD = btnD; lastBtnL = btnL; lastBtnC = btnC;
    
    delay(50);
  }
}

// ========== TASK AUDIO (CORE 1) ==========
void audioTaskCode(void * parameter) {
  Serial.println("Audio Task avviato su Core 1");
  
  // Inizializza I2S
  if (!initializeI2S()) {
    Serial.println("Errore I2S - Audio Task terminato");
    vTaskDelete(NULL);
    return;
  }
  
  /*
  // Scansione file WAV
  wavFiles = getWAVFiles();
  if (wavFiles.empty()) {
    Serial.println("Nessun file WAV trovato - Audio Task in standby");
  }
  
  for(;;) {
    if (!wavFiles.empty()) {
      String currentFile = wavFiles[currentFileIndex];
      Serial.printf("Riproduco: %s\n", currentFile.c_str());
      
      playWAVFile(currentFile.c_str());
      
      currentFileIndex++;
      if (currentFileIndex >= wavFiles.size()) {
        currentFileIndex = 0;
      }
      
      delay(1000); // Pausa tra file
    } else {
      delay(5000); // Standby se nessun file
    }
  }*/
}

// ========== FUNZIONI ROBOT ==========
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogRead(pin);
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

// da implementare
void playNote(int frequency, int duration) {
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

bool playWAVFile(const char* filename) {
  if (!i2s_initialized) return false;
  
  File audioFile = LittleFS.open(filename, "r");
  if (!audioFile) return false;
  
  WAVHeader header;
  if (audioFile.read((uint8_t*)&header, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
    audioFile.close();
    return false;
  }
  
  // Verifica formato base
  if (memcmp(header.riff, "RIFF", 4) != 0 || 
      memcmp(header.wave, "WAVE", 4) != 0 ||
      header.audioFormat != 1) {
    audioFile.close();
    return false;
  }
  
  audioFile.seek(sizeof(WAVHeader));
  
  size_t totalBytes = header.dataSize;
  size_t bytesRead = 0;
  
  while (bytesRead < totalBytes && audioFile.available()) {
    size_t bytesToRead = min((size_t)DMA_BUF_LEN * 2, totalBytes - bytesRead);
    if (bytesToRead == 0) break;
    
    uint8_t* tempBuffer = (uint8_t*)audio_buffer;
    size_t actualRead = audioFile.read(tempBuffer, bytesToRead);
    if (actualRead == 0) break;
    
    size_t samples = actualRead / 2;
    
    // Applica volume e converti mono->stereo se necessario
    if (header.numChannels == 1) {
      for (int i = samples - 1; i >= 0; i--) {
        int16_t sample = ((int16_t*)tempBuffer)[i];
        int16_t volumeAdjusted = (int16_t)(sample * VOLUME);
        audio_buffer[i * 2] = volumeAdjusted;
        audio_buffer[i * 2 + 1] = volumeAdjusted;
      }
      samples *= 2;
    } else {
      for (int i = 0; i < samples; i++) {
        int16_t sample = ((int16_t*)tempBuffer)[i];
        audio_buffer[i] = (int16_t)(sample * VOLUME);
      }
    }
    
    size_t bytes_written;
    i2s_write(I2S_NUM, audio_buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    
    bytesRead += actualRead;
  }
  
  audioFile.close();
  return true;
}

/*
std::vector<String> getWAVFiles() {
  std::vector<String> files;
  
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) return files;
  
  File file = root.openNextFile();
  while (file) {
    String filename = String(file.name());
    if (filename.toLowerCase().endsWith(".wav")) {
      files.push_back("/" + filename);
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
  return files;
}*/