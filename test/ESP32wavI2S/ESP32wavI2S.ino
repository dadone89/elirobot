#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <esp_task_wdt.h>

// Macro helper per min/max con tipi diversi
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Configurazione pin I2S
#define I2S_BCK_PIN     26    // Bit Clock
#define I2S_WS_PIN      25    // Word Select (LR Clock)
#define I2S_DATA_PIN    22    // Data Out
#define I2S_NUM         I2S_NUM_0

// Configurazione audio
#define SAMPLE_RATE     8000 //44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CHANNELS        1
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     1024

#define VOLUME          0.2f //volume al 50%

// Buffer per audio
static int16_t audio_buffer[DMA_BUF_LEN * 2]; // Stereo
static bool i2s_initialized = false;

// Struttura WAV header migliorata
struct WAVHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // 16 for PCM
    uint16_t audioFormat;   // 1 for PCM
    uint16_t numChannels;   // 1 for mono, 2 for stereo
    uint32_t sampleRate;    // Sample rate
    uint32_t byteRate;      // Byte rate
    uint16_t blockAlign;    // Block align
    uint16_t bitsPerSample; // Bits per sample
    char data[4];           // "data"
    uint32_t dataSize;      // Data size
};

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== ESP32 I2S Audio Player con Volume Fisso 50% ===");
    
    // Configura il Task Watchdog
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,
        .trigger_panic = false
    };
    esp_task_wdt_reconfigure(&wdt_config);
    
    // Inizializzazione LittleFS
    if (!initializeLittleFS()) {
        Serial.println("❌ Errore fatale: impossibile inizializzare LittleFS");
        Serial.println("🔄 Continuo senza filesystem per testare I2S...");
    }
    
    // Inizializzazione I2S
    if (!initializeI2S()) {
        Serial.println("❌ Errore fatale: impossibile inizializzare I2S");
        while(1) delay(1000);
    }
    
    Serial.println("✅ Inizializzazione completata con successo!");
    Serial.println("🔊 Volume fisso impostato al 50%");
    
    // Test di riproduzione
    playTestTone();
}

void loop() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 10000) { // Ogni 10 secondi
        Serial.println("\n=== LOOP DEBUG ===");
        esp_task_wdt_reset();
        lastUpdate = millis();
        
        // Lista file audio disponibili
        listAudioFiles();
        
        // Analizza il file WAV prima di riprodurlo
        analyzeWAVFile("/aaa.wav");
        
        // Riproduci il file
        playWAVFile("/aaa.wav");
        
        // Health check
        checkSystemHealth();
    }
    
    delay(100);
}

// Funzione per analizzare header WAV
bool analyzeWAVFile(const char* filename) {
    File audioFile = LittleFS.open(filename, "r");
    if (!audioFile) {
        Serial.printf("❌ File non trovato: %s\n", filename);
        return false;
    }
    
    Serial.printf("\n🔍 ANALISI FILE WAV: %s\n", filename);
    Serial.printf("Dimensione totale file: %zu bytes\n", audioFile.size());
    
    if (audioFile.size() < sizeof(WAVHeader)) {
        Serial.println("❌ File troppo piccolo per essere un WAV valido");
        audioFile.close();
        return false;
    }
    
    WAVHeader header;
    size_t bytesRead = audioFile.read((uint8_t*)&header, sizeof(WAVHeader));
    
    if (bytesRead != sizeof(WAVHeader)) {
        Serial.printf("❌ Errore lettura header (letti %zu bytes su %zu)\n", bytesRead, sizeof(WAVHeader));
        audioFile.close();
        return false;
    }
    
    // Debug raw header
    Serial.println("📋 Header RAW:");
    Serial.printf("  RIFF: %c%c%c%c\n", header.riff[0], header.riff[1], header.riff[2], header.riff[3]);
    Serial.printf("  File Size: %u\n", header.fileSize);
    Serial.printf("  WAVE: %c%c%c%c\n", header.wave[0], header.wave[1], header.wave[2], header.wave[3]);
    Serial.printf("  fmt: %c%c%c%c\n", header.fmt[0], header.fmt[1], header.fmt[2], header.fmt[3]);
    Serial.printf("  fmt Size: %u\n", header.fmtSize);
    Serial.printf("  Audio Format: %u\n", header.audioFormat);
    Serial.printf("  Channels: %u\n", header.numChannels);
    Serial.printf("  Sample Rate: %u Hz\n", header.sampleRate);
    Serial.printf("  Byte Rate: %u\n", header.byteRate);
    Serial.printf("  Block Align: %u\n", header.blockAlign);
    Serial.printf("  Bits per Sample: %u\n", header.bitsPerSample);
    Serial.printf("  DATA: %c%c%c%c\n", header.data[0], header.data[1], header.data[2], header.data[3]);
    Serial.printf("  Data Size: %u bytes\n", header.dataSize);
    
    // Verifica validità
    bool isValid = true;
    
    if (memcmp(header.riff, "RIFF", 4) != 0) {
        Serial.println("❌ Signature RIFF non valida");
        isValid = false;
    }
    
    if (memcmp(header.wave, "WAVE", 4) != 0) {
        Serial.println("❌ Signature WAVE non valida");
        isValid = false;
    }
    
    if (memcmp(header.fmt, "fmt ", 4) != 0) {
        Serial.println("❌ Chunk fmt non valido");
        isValid = false;
    }
    
    if (header.audioFormat != 1) {
        Serial.printf("⚠️  Formato audio: %u (dovrebbe essere 1 per PCM)\n", header.audioFormat);
        isValid = false;
    }
    
    if (header.bitsPerSample != 16) {
        Serial.printf("⚠️  Bit per sample: %u (configurato per 16-bit)\n", header.bitsPerSample);
    }
    
    if (header.sampleRate != SAMPLE_RATE) {
        Serial.printf("⚠️  Sample rate: %u Hz (configurato per %d Hz)\n", header.sampleRate, SAMPLE_RATE);
    }
    
    if (memcmp(header.data, "data", 4) != 0) {
        Serial.println("❌ Chunk data non trovato");
        isValid = false;
    }
    
    // Calcola durata
    if (header.byteRate > 0) {
        float duration = (float)header.dataSize / header.byteRate;
        Serial.printf("📊 Durata calcolata: %.2f secondi\n", duration);
    }
    
    // Verifica coerenza file size
    uint32_t expectedFileSize = header.dataSize + 36; // Header WAV standard è 44 bytes, dataSize + 36
    if (audioFile.size() != expectedFileSize + 8) { // +8 per RIFF header
        Serial.printf("⚠️  File size inconsistente: %zu vs atteso %u\n", 
                     audioFile.size(), expectedFileSize + 8);
    }
    
    Serial.printf("✅ File WAV %s\n", isValid ? "VALIDO" : "NON VALIDO");
    
    audioFile.close();
    return isValid;
}

// Funzione migliorata per riprodurre file WAV con volume fisso al 50%
bool playWAVFile(const char* filename) {
    if (!i2s_initialized) {
        Serial.println("❌ I2S non inizializzato");
        return false;
    }
    
    File audioFile = LittleFS.open(filename, "r");
    if (!audioFile) {
        Serial.printf("❌ Impossibile aprire file: %s\n", filename);
        return false;
    }
    
    Serial.printf("\n🎵 RIPRODUZIONE: %s (Volume: 50%%)\n", filename);
    
    // Leggi e verifica header
    WAVHeader header;
    if (audioFile.read((uint8_t*)&header, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
        Serial.println("❌ Errore lettura header WAV");
        audioFile.close();
        return false;
    }
    
    // Verifica formato
    if (memcmp(header.riff, "RIFF", 4) != 0 || 
        memcmp(header.wave, "WAVE", 4) != 0 ||
        header.audioFormat != 1) {
        Serial.println("❌ Formato WAV non supportato");
        audioFile.close();
        return false;
    }
    
    Serial.printf("📊 Configurazione audio:\n");
    Serial.printf("  Sample Rate: %u Hz (ESP32: %d Hz)\n", header.sampleRate, SAMPLE_RATE);
    Serial.printf("  Canali: %u (ESP32: stereo)\n", header.numChannels);
    Serial.printf("  Bit depth: %u bit\n", header.bitsPerSample);
    Serial.printf("  Dati audio: %u bytes\n", header.dataSize);
    
    // Posiziona dopo l'header
    audioFile.seek(sizeof(WAVHeader));
    
    size_t totalBytes = header.dataSize;
    size_t bytesRead = 0;
    size_t samplesProcessed = 0;
    
    Serial.println("▶️  Inizio riproduzione...");
    
    while (bytesRead < totalBytes && audioFile.available()) {
        // Calcola quanti sample leggere
        size_t samplesToRead = MIN((size_t)DMA_BUF_LEN, (totalBytes - bytesRead) / (header.bitsPerSample / 8));
        
        if (samplesToRead == 0) break;
        
        // Leggi i dati audio
        size_t bytesToRead = samplesToRead * (header.bitsPerSample / 8);
        uint8_t* readBuffer = (uint8_t*)audio_buffer;
        size_t actualRead = audioFile.read(readBuffer, bytesToRead);
        
        if (actualRead == 0) break;
        
        // Converti i dati in base al formato del file
        size_t actualSamples = actualRead / (header.bitsPerSample / 8);
        
        if (header.numChannels == 1) {
            // Mono -> Stereo: duplica ogni sample CON VOLUME AL 50%
            for (int i = actualSamples - 1; i >= 0; i--) {
                int16_t sample = ((int16_t*)readBuffer)[i];
                
                // APPLICA VOLUME AL 50% (moltiplica per 0.5)
                int16_t volumeAdjusted = (int16_t)(sample * VOLUME);
                
                audio_buffer[i * 2] = volumeAdjusted;     // Left
                audio_buffer[i * 2 + 1] = volumeAdjusted; // Right
            }
            actualSamples *= 2; // Ora abbiamo il doppio dei sample (stereo)
        } else {
            // Se è già stereo, applica volume al 50% anche qui
            for (int i = 0; i < actualSamples; i++) {
                int16_t sample = ((int16_t*)readBuffer)[i];
                // APPLICA VOLUME AL 50%
                audio_buffer[i] = (int16_t)(sample * VOLUME);
            }
        }
        
        // Debug primo sample
        if (samplesProcessed == 0) {
            Serial.printf("🔍 Primo sample (con volume 50%%): L=%d, R=%d\n", 
                         audio_buffer[0], audio_buffer[1]);
        }
        
        // Scrivi su I2S
        size_t bytes_written;
        size_t bytesToWrite = actualSamples * sizeof(int16_t);
        esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWrite, &bytes_written, portMAX_DELAY);
        
        if (err != ESP_OK) {
            Serial.printf("❌ Errore I2S write: %s\n", esp_err_to_name(err));
            break;
        }
        
        if (bytes_written != bytesToWrite) {
            Serial.printf("⚠️  Scritti %zu bytes su %zu richiesti\n", bytes_written, bytesToWrite);
        }
        
        bytesRead += actualRead;
        samplesProcessed += actualSamples / (header.numChannels == 1 ? 2 : 1);
        
        // Progress ogni 10KB
        if (bytesRead % 10240 == 0 || bytesRead >= totalBytes) {
            float progress = (float)bytesRead * 100.0 / totalBytes;
            Serial.printf("📊 Progresso: %.1f%% (%zu/%zu bytes, %zu samples) - Volume: 50%%\n", 
                         progress, bytesRead, totalBytes, samplesProcessed);
        }
        
        // Reset watchdog
        if (bytesRead % 4096 == 0) {
            esp_task_wdt_reset();
            yield();
        }
    }
    
    audioFile.close();
    
    Serial.println("✅ Riproduzione completata");
    Serial.printf("📈 Statistiche: %zu bytes letti, %zu samples processati (Volume: 50%%)\n", 
                 bytesRead, samplesProcessed);
    
    // Attendi svuotamento buffer
    delay(200);
    
    return true;
}

bool initializeLittleFS() {
    Serial.println("Inizializzazione LittleFS...");
    esp_task_wdt_reset();
    
    if (!LittleFS.begin(false)) {
        Serial.println("⚠️  LittleFS mount fallito, provo a formattare...");
        Serial.println("⏳ Questa operazione può richiedere fino a 30 secondi...");
        
        esp_task_wdt_reset();
        esp_task_wdt_deinit();
        
        bool formatResult = LittleFS.format();
        
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = 30000,
            .idle_core_mask = 0,
            .trigger_panic = false
        };
        esp_task_wdt_init(&wdt_config);
        esp_task_wdt_add(NULL);
        
        if (!formatResult) {
            Serial.println("❌ Errore nella formattazione LittleFS");
            return false;
        }
        
        Serial.println("✅ LittleFS formattato con successo");
        esp_task_wdt_reset();
        
        if (!LittleFS.begin(false)) {
            Serial.println("❌ Errore nel montaggio LittleFS dopo formattazione");
            return false;
        }
    }
    
    Serial.println("✅ LittleFS montato correttamente");
    esp_task_wdt_reset();
    
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    Serial.printf("Spazio totale: %zu bytes (%.2f MB)\n", total, total / 1048576.0);
    Serial.printf("Spazio utilizzato: %zu bytes (%.2f MB)\n", used, used / 1048576.0);
    Serial.printf("Spazio libero: %zu bytes (%.2f MB)\n", total - used, (total - used) / 1048576.0);
    
    Serial.println("\nContenuto di LittleFS:");
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("⚠️  Errore apertura directory root o root non è directory");
        return true;
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  📁 %s/ (directory)\n", file.name());
        } else {
            Serial.printf("  📄 %s (%zu bytes)\n", file.name(), file.size());
        }
        file.close();
        file = root.openNextFile();
        fileCount++;
        
        if (fileCount % 10 == 0) {
            esp_task_wdt_reset();
        }
    }
    root.close();
    
    if (fileCount == 0) {
        Serial.println("  (Directory vuota)");
        Serial.println("  💡 Puoi caricare file usando 'pio run --target uploadfs'");
    }
    
    return true;
}

bool initializeI2S() {
    Serial.println("Configurazione pin I2S...");
    esp_task_wdt_reset();
    
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
    
    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore installazione driver I2S: %s\n", esp_err_to_name(err));
        return false;
    }
    
    delay(100);
    esp_task_wdt_reset();
    
    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore configurazione pin I2S: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM);
        return false;
    }
    
    delay(100);
    esp_task_wdt_reset();
    
    err = i2s_start(I2S_NUM);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore avvio I2S: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM);
        return false;
    }
    
    i2s_zero_dma_buffer(I2S_NUM);
    
    Serial.println("✅ I2S configurato e avviato correttamente");
    Serial.printf("  📌 Pin Configuration:\n");
    Serial.printf("    BCK (Bit Clock): GPIO %d\n", I2S_BCK_PIN);
    Serial.printf("    WS (Word Select): GPIO %d\n", I2S_WS_PIN);
    Serial.printf("    DATA (Audio Out): GPIO %d\n", I2S_DATA_PIN);
    Serial.printf("  🎵 Audio Configuration:\n");
    Serial.printf("    Sample Rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("    Bits per Sample: %d\n", (int)BITS_PER_SAMPLE);
    Serial.printf("    Channels: %d (Stereo)\n", CHANNELS);
    Serial.printf("    DMA Buffers: %d x %d samples\n", DMA_BUF_COUNT, DMA_BUF_LEN);
    Serial.printf("    Volume: 50%% (fisso)\n");
    
    i2s_initialized = true;
    return true;
}

void playTestTone() {
    if (!i2s_initialized) {
        Serial.println("❌ I2S non inizializzato");
        return;
    }
    
    Serial.println("🎵 Riproduzione tono di test (440 Hz per 2 secondi, Volume: 50%)...");
    
    const int frequency = 440;
    const int duration_ms = 2000;
    const int samples_per_cycle = SAMPLE_RATE / frequency;
    const int total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    
    const int fade_samples = SAMPLE_RATE / 20;
    
    for (int i = 0; i < total_samples; i += DMA_BUF_LEN) {
        int samples_to_write = MIN(DMA_BUF_LEN, total_samples - i);
        
        for (int j = 0; j < samples_to_write; j++) {
            int sample_index = i + j;
            float phase = (2.0 * PI * sample_index) / samples_per_cycle;
            
            float amplitude = 1.0;
            if (sample_index < fade_samples) {
                amplitude = (float)sample_index / fade_samples;
            } else if (sample_index > total_samples - fade_samples) {
                amplitude = (float)(total_samples - sample_index) / fade_samples;
            }
            
            // Genera il sample base CON VOLUME AL 50%
            int16_t sample_value = (int16_t)(sin(phase) * 8000 * amplitude * VOLUME);
            
            audio_buffer[j * 2] = sample_value;
            audio_buffer[j * 2 + 1] = sample_value;
        }
        
        size_t bytes_written;
        esp_err_t err = i2s_write(I2S_NUM, audio_buffer, samples_to_write * 4, &bytes_written, portMAX_DELAY);
        
        if (err != ESP_OK) {
            Serial.printf("❌ Errore scrittura I2S: %s\n", esp_err_to_name(err));
            break;
        }
        
        if (i % (DMA_BUF_LEN * 10) == 0) {
            esp_task_wdt_reset();
            delay(1);
        }
    }
    
    delay(100);
    Serial.println("✅ Tono di test completato (Volume: 50%)");
}

String toLowerCase(String str) {
    String result = str;
    for (int i = 0; i < result.length(); i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

void listAudioFiles() {
    Serial.println("\n📁 Scansione file audio in LittleFS:");
    esp_task_wdt_reset();
    
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Errore apertura directory root");
        return;
    }
    
    File file = root.openNextFile();
    bool foundAudio = false;
    int totalFiles = 0;
    
    while (file) {
        totalFiles++;
        String filename = String(file.name());
        String lowerFilename = toLowerCase(filename);
        
        if (lowerFilename.endsWith(".wav") || lowerFilename.endsWith(".mp3") || 
            lowerFilename.endsWith(".pcm") || lowerFilename.endsWith(".raw") ||
            lowerFilename.endsWith(".flac") || lowerFilename.endsWith(".aac")) {
            
            Serial.printf("  🎵 %s\n", filename.c_str());
            Serial.printf("      Dimensione: %zu bytes (%.2f KB)\n", 
                         file.size(), file.size() / 1024.0);
            
            if (lowerFilename.endsWith(".wav")) {
                size_t audioData = file.size() - 44;
                float duration = (float)audioData / (SAMPLE_RATE * CHANNELS * 2);
                Serial.printf("      Durata stimata: %.1f secondi (Volume: 50%%)\n", duration);
            }
            
            foundAudio = true;
        }
        
        file.close();
        file = root.openNextFile();
        
        if (totalFiles % 10 == 0) {
            esp_task_wdt_reset();
        }
    }
    root.close();
    
    Serial.printf("\nTotale file: %d\n", totalFiles);
    
    if (!foundAudio) {
        Serial.println("  📭 Nessun file audio trovato");
        Serial.println("  🎯 Formati supportati: .wav, .mp3, .pcm, .raw, .flac, .aac");
        Serial.println("  📤 Per caricare file: crea cartella 'data/' e usa 'pio run --target uploadfs'");
        Serial.println("  💡 Esempio struttura:");
        Serial.println("      project/");
        Serial.println("      ├── src/main.cpp");
        Serial.println("      └── data/");
        Serial.println("          ├── song1.wav");
        Serial.println("          └── sound.mp3");
    }
}

void checkSystemHealth() {
    Serial.println("\n🏥 System Health Check:");
    Serial.printf("  Free Heap: %zu bytes (%.2f KB)\n", 
                 ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    Serial.printf("  Min Free Heap: %zu bytes\n", ESP.getMinFreeHeap());
    Serial.printf("  Heap Size: %zu bytes\n", ESP.getHeapSize());
    Serial.printf("  PSRAM Total: %zu bytes\n", ESP.getPsramSize());
    Serial.printf("  PSRAM Free: %zu bytes\n", ESP.getFreePsram());
    Serial.printf("  Flash Size: %zu bytes\n", ESP.getFlashChipSize());
    Serial.printf("  CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  Uptime: %lu seconds\n", millis() / 1000);
    Serial.printf("  🔊 Volume: 50%% (fisso)\n");
    
    esp_task_wdt_reset();
}