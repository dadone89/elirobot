#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <vector> // Necessario per std::vector

// Macro helper per min/max con tipi diversi
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Configurazione pin I2S
#define I2S_BCK_PIN     26    // Bit Clock
#define I2S_WS_PIN      25    // Word Select (LR Clock)
#define I2S_DATA_PIN    22    // Data Out
#define I2S_NUM         I2S_NUM_0

// Configurazione audio
#define SAMPLE_RATE     8000  // Frequenza di campionamento (es. 44100 per qualità CD)
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT // Profondità di bit (16 bit)
#define CHANNELS        1     // Numero di canali (1 per mono, 2 per stereo)
#define DMA_BUF_COUNT   8     // Numero di buffer DMA
#define DMA_BUF_LEN     1024  // Lunghezza di ogni buffer DMA in campioni

#define VOLUME          0.1f  // Volume fisso al 50% (modificato da 0.2f a 0.5f per più volume)

// Buffer per audio: dimensione per 2 canali (stereo) di 16-bit sample
static int16_t audio_buffer[DMA_BUF_LEN * 2];
static bool i2s_initialized = false;

// Struttura per l'header del file WAV
struct WAVHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // Dimensione totale del file - 8 byte
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // Dimensione del chunk 'fmt' (16 per PCM)
    uint16_t audioFormat;   // Formato audio (1 per PCM)
    uint16_t numChannels;   // Numero di canali (1 mono, 2 stereo)
    uint32_t sampleRate;    // Frequenza di campionamento
    uint32_t byteRate;      // Byte rate (SampleRate * NumChannels * BitsPerSample/8)
    uint16_t blockAlign;    // Block align (NumChannels * BitsPerSample/8)
    uint16_t bitsPerSample; // Bit per campione
    char data[4];           // "data"
    uint32_t dataSize;      // Dimensione dei dati audio
};

// Dichiarazioni di funzioni prototipo
bool initializeLittleFS();
bool initializeI2S();
bool analyzeWAVFile(const char* filename);
bool playWAVFile(const char* filename);
void playTestTone();
String toLowerCase(String str);
std::vector<String> getWAVFilePaths(); // Funzione per ottenere i percorsi dei file WAV
void checkSystemHealth();

// Variabili globali per la gestione della riproduzione dei file
std::vector<String> wavFiles;
unsigned int currentFileIndex = 0;
bool initialScanDone = false; // Flag per assicurarsi che la scansione avvenga solo una volta

void setup() {
    Serial.begin(115200);
    delay(2000); // Ritardo iniziale per la stabilità della Serial

    Serial.println("=== ESP32 I2S Audio Player con Riproduzione Tutti WAV ===");

    // Configura il Task Watchdog (WDT) per prevenire blocchi
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,        // Timeout di 30 secondi
        .idle_core_mask = 0,        // Non monitorare il core inattivo
        .trigger_panic = false      // Non causare panic in caso di timeout
    };
    esp_task_wdt_reconfigure(&wdt_config);

    // Inizializzazione LittleFS
    if (!initializeLittleFS()) {
        Serial.println("❌ Errore fatale: impossibile inizializzare LittleFS");
        Serial.println("🔄 Continuo senza filesystem per testare I2S...");
        // Non blocchiamo qui, ma il player non potrà riprodurre file.
    }

    // Inizializzazione I2S
    if (!initializeI2S()) {
        Serial.println("❌ Errore fatale: impossibile inizializzare I2S");
        while(1) delay(1000); // Blocco se I2S non può essere inizializzato
    }

    Serial.println("✅ Inizializzazione completata con successo!");
    Serial.printf("🔊 Volume fisso impostato al %.0f%%\n", VOLUME * 100);

    // Il tono di test iniziale è stato rimosso per dare priorità ai file audio.
    // Verrà riprodotto solo se non vengono trovati file WAV.
}

void loop() {
    // Esegui la scansione dei file WAV solo una volta all'avvio
    if (!initialScanDone) {
        Serial.println("\n--- Ricerca file WAV sul dispositivo ---");
        wavFiles = getWAVFilePaths();
        initialScanDone = true; // Imposta il flag a true per evitare scansioni future

        if (wavFiles.empty()) {
            Serial.println("📭 Nessun file WAV trovato. Riproduzione tono di test ogni 5 secondi.");
            playTestTone(); // Riproduci tono di test se nessun file è presente
            delay(5000);
            return; // Salta il resto del loop per questa iterazione
        } else {
            Serial.printf("✅ Trovati %zu file WAV.\n", wavFiles.size());
        }
    }

    // Riproduci tutti i file WAV in un ciclo continuo
    if (!wavFiles.empty()) {
        String currentFilePath = wavFiles[currentFileIndex];
        Serial.printf("\n--- Riproduzione file %u di %zu: %s ---\n",
                      currentFileIndex + 1, wavFiles.size(), currentFilePath.c_str());

        // Opzionale: analizza il file WAV prima della riproduzione
        // analyzeWAVFile(currentFilePath.c_str());

        // Riproduci il file corrente
        playWAVFile(currentFilePath.c_str());

        // Passa al file successivo
        currentFileIndex++;
        if (currentFileIndex >= wavFiles.size()) {
            currentFileIndex = 0; // Torna al primo file per ripetere la lista
            Serial.println("\n--- Tutti i file WAV riprodotti. Riavvio la lista. ---");
        }
    } else {
        // Questa parte del codice dovrebbe essere raggiunta solo se initialScanDone
        // è true e non sono stati trovati file WAV.
        Serial.println("Continuo a riprodurre tono di test (nessun file WAV trovato).");
        playTestTone();
        delay(5000);
    }

    // Health check del sistema (meno frequente per non intasare la seriale)
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 30000) { // Ogni 30 secondi
        checkSystemHealth();
        lastHealthCheck = millis();
    }
    
    esp_task_wdt_reset(); // Reset del watchdog regolarmente
    delay(10); // Piccolo ritardo per evitare un ciclo di attesa troppo stretto
}

// Implementazione delle funzioni ausiliarie:

// Funzione per analizzare l'header di un file WAV
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
    
    // Debug dei valori raw dell'header
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
    
    // Verifica di validità del formato WAV
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
        // Potrebbe essere ancora riproducibile ma con avviso
    }
    
    if (header.sampleRate != SAMPLE_RATE) {
        Serial.printf("⚠️  Sample rate: %u Hz (configurato per %d Hz)\n", header.sampleRate, SAMPLE_RATE);
        // Potrebbe causare problemi o riproduzione a velocità errata
    }
    
    if (memcmp(header.data, "data", 4) != 0) {
        Serial.println("❌ Chunk data non trovato");
        isValid = false; // Essenziale per la riproduzione
    }
    
    // Calcola e stampa la durata stimata del file
    if (header.byteRate > 0) {
        float duration = (float)header.dataSize / header.byteRate;
        Serial.printf("📊 Durata calcolata: %.2f secondi\n", duration);
    }
    
    // Verifica coerenza della dimensione del file
    uint32_t expectedFileSize = header.dataSize + 36; // Header WAV standard è 44 bytes, quindi dataSize + 36
    if (audioFile.size() != expectedFileSize + 8) { // +8 per il chunk RIFF iniziale
        Serial.printf("⚠️  File size inconsistente: %zu vs atteso %u\n", 
                      audioFile.size(), expectedFileSize + 8);
    }
    
    Serial.printf("✅ File WAV %s\n", isValid ? "VALIDO" : "NON VALIDO");
    
    audioFile.close();
    return isValid;
}

// Funzione migliorata per riprodurre file WAV con volume fisso
bool playWAVFile(const char* filename) {
    if (!i2s_initialized) {
        Serial.println("❌ I2S non inizializzato, impossibile riprodurre.");
        return false;
    }
    
    File audioFile = LittleFS.open(filename, "r");
    if (!audioFile) {
        Serial.printf("❌ Impossibile aprire file: %s\n", filename);
        return false;
    }
    
    Serial.printf("\n🎵 RIPRODUZIONE: %s (Volume: %.0f%%)\n", filename, VOLUME * 100);
    
    // Leggi e verifica l'header del file WAV
    WAVHeader header;
    if (audioFile.read((uint8_t*)&header, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
        Serial.println("❌ Errore lettura header WAV");
        audioFile.close();
        return false;
    }
    
    // Verifica che il formato sia PCM e le signature siano corrette
    if (memcmp(header.riff, "RIFF", 4) != 0 || 
        memcmp(header.wave, "WAVE", 4) != 0 ||
        header.audioFormat != 1) {
        Serial.println("❌ Formato WAV non supportato (solo PCM)");
        audioFile.close();
        return false;
    }
    
    Serial.printf("📊 Configurazione audio del file:\n");
    Serial.printf("  Sample Rate: %u Hz (ESP32 I2S configurato: %d Hz)\n", header.sampleRate, SAMPLE_RATE);
    Serial.printf("  Canali: %u (ESP32 I2S configurato: stereo)\n", header.numChannels);
    Serial.printf("  Bit depth: %u bit\n", header.bitsPerSample);
    Serial.printf("  Dati audio: %u bytes\n", header.dataSize);
    
    // Posiziona il puntatore del file all'inizio dei dati audio
    audioFile.seek(sizeof(WAVHeader));
    
    size_t totalBytes = header.dataSize;
    size_t bytesRead = 0;
    size_t samplesProcessed = 0;
    
    Serial.println("▶️  Inizio riproduzione...");
    
    // Loop per leggere e riprodurre i dati audio
    while (bytesRead < totalBytes && audioFile.available()) {
        // Calcola quanti sample leggere nel buffer corrente
        size_t bytesToReadFromBuffer = MIN((size_t)DMA_BUF_LEN * (header.bitsPerSample / 8), totalBytes - bytesRead);
        
        if (bytesToReadFromBuffer == 0) break;
        
        // Leggi i dati audio dal file nel buffer temporaneo
        uint8_t* tempReadBuffer = (uint8_t*)audio_buffer; // Usiamo audio_buffer come buffer temporaneo
        size_t actualRead = audioFile.read(tempReadBuffer, bytesToReadFromBuffer);
        
        if (actualRead == 0) break; // Fine del file o errore di lettura
        
        // Calcola il numero effettivo di campioni letti
        size_t actualSamplesReadFromFile = actualRead / (header.bitsPerSample / 8);
        
        // Processa i campioni per volume e conversione mono-stereo
        if (header.numChannels == 1) {
            // Se il file è mono, lo convertiamo in stereo duplicando i campioni
            // e applicando il volume fisso
            for (int i = actualSamplesReadFromFile - 1; i >= 0; i--) {
                int16_t sample = ((int16_t*)tempReadBuffer)[i];
                int16_t volumeAdjusted = (int16_t)(sample * VOLUME);
                
                audio_buffer[i * 2] = volumeAdjusted;     // Canale Left
                audio_buffer[i * 2 + 1] = volumeAdjusted; // Canale Right
            }
            actualSamplesReadFromFile *= 2; // Il numero di campioni raddoppia per stereo
        } else { // Se il file è già stereo
            // Applica il volume fisso direttamente ai campioni esistenti
            for (int i = 0; i < actualSamplesReadFromFile; i++) {
                int16_t sample = ((int16_t*)tempReadBuffer)[i];
                audio_buffer[i] = (int16_t)(sample * VOLUME);
            }
        }
        
        // Debug del primo sample processato
        if (samplesProcessed == 0) {
            Serial.printf("🔍 Primo sample processato (con volume %.0f%%): L=%d, R=%d\n", 
                          VOLUME * 100, audio_buffer[0], audio_buffer[1]);
        }
        
        // Scrivi i campioni processati su I2S
        size_t bytes_written;
        // La dimensione da scrivere è il numero di campioni processati * dimensione di un sample (2 bytes per int16_t)
        size_t bytesToWriteToI2S = actualSamplesReadFromFile * sizeof(int16_t);
        esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWriteToI2S, &bytes_written, portMAX_DELAY);
        
        if (err != ESP_OK) {
            Serial.printf("❌ Errore I2S write: %s\n", esp_err_to_name(err));
            break;
        }
        
        if (bytes_written != bytesToWriteToI2S) {
            Serial.printf("⚠️  Scritti %zu bytes su %zu richiesti\n", bytes_written, bytesToWriteToI2S);
        }
        
        bytesRead += actualRead; // Aggiorna i byte totali letti dal file
        // Aggiorna il conteggio dei campioni "originali" processati
        samplesProcessed += actualSamplesReadFromFile / (header.numChannels == 1 ? 2 : 1); 
        
        // Stampa il progresso della riproduzione ogni 10KB o alla fine
        if (bytesRead % 10240 == 0 || bytesRead >= totalBytes) {
            float progress = (float)bytesRead * 100.0 / totalBytes;
            Serial.printf("📊 Progresso: %.1f%% (%zu/%zu bytes) - Volume: %.0f%%\n", 
                          progress, bytesRead, totalBytes, VOLUME * 100);
        }
        
        // Reset del watchdog per prevenire timeout durante la riproduzione di file lunghi
        if (bytesRead % 4096 == 0) {
            esp_task_wdt_reset();
            yield(); // Permette ad altre task di eseguire
        }
    }
    
    audioFile.close(); // Chiudi il file alla fine della riproduzione
    
    Serial.println("✅ Riproduzione completata");
    Serial.printf("📈 Statistiche: %zu bytes letti, %zu campioni processati (Volume: %.0f%%)\n", 
                  bytesRead, samplesProcessed, VOLUME * 100);
    
    delay(200); // Piccolo ritardo per permettere al buffer I2S di svuotarsi
    
    return true;
}

// Funzione per inizializzare LittleFS
bool initializeLittleFS() {
    Serial.println("Inizializzazione LittleFS...");
    esp_task_wdt_reset(); // Reset del watchdog
    
    if (!LittleFS.begin(false)) { // Prova a montare senza formattare
        Serial.println("⚠️  LittleFS mount fallito, provo a formattare...");
        Serial.println("⏳ Questa operazione può richiedere fino a 30 secondi...");
        
        // Disabilita temporaneamente il watchdog per la formattazione lunga
        esp_task_wdt_deinit();
        
        bool formatResult = LittleFS.format(); // Tenta la formattazione
        
        // Ritorna le impostazioni del watchdog dopo la formattazione
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = 30000,
            .idle_core_mask = 0,
            .trigger_panic = false
        };
        esp_task_wdt_init(&wdt_config);
        esp_task_wdt_add(NULL); // Aggiungi la task corrente al watchdog
        
        if (!formatResult) {
            Serial.println("❌ Errore nella formattazione LittleFS");
            return false;
        }
        
        Serial.println("✅ LittleFS formattato con successo");
        esp_task_wdt_reset();
        
        if (!LittleFS.begin(false)) { // Riprova il montaggio dopo la formattazione
            Serial.println("❌ Errore nel montaggio LittleFS dopo formattazione");
            return false;
        }
    }
    
    Serial.println("✅ LittleFS montato correttamente");
    esp_task_wdt_reset();
    
    // Stampa le informazioni sullo spazio di LittleFS
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    Serial.printf("Spazio totale: %zu bytes (%.2f MB)\n", total, total / 1048576.0);
    Serial.printf("Spazio utilizzato: %zu bytes (%.2f MB)\n", used, used / 1048576.0);
    Serial.printf("Spazio libero: %zu bytes (%.2f MB)\n", total - used, (total - used) / 1048576.0);
    
    // Stampa il contenuto della directory root di LittleFS
    Serial.println("\nContenuto di LittleFS:");
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("⚠️  Errore apertura directory root o root non è directory");
        return true; // Non è un errore fatale, il sistema può ancora funzionare
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  📁 %s/ (directory)\n", file.name());
        } else {
            Serial.printf("  📄 %s (%zu bytes)\n", file.name(), file.size());
        }
        file.close(); // Chiudi il file prima di aprirne il successivo
        file = root.openNextFile();
        fileCount++;
        
        if (fileCount % 10 == 0) { // Reset del watchdog ogni 10 file
            esp_task_wdt_reset();
        }
    }
    root.close(); // Chiudi la directory root
    
    if (fileCount == 0) {
        Serial.println("  (Directory vuota)");
        Serial.println("  💡 Puoi caricare file usando 'pio run --target uploadfs'");
    }
    
    return true;
}

// Funzione per inizializzare l'interfaccia I2S
bool initializeI2S() {
    Serial.println("Configurazione pin I2S...");
    esp_task_wdt_reset();
    
    // Configurazione del driver I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Modalità Master, solo trasmissione (output)
        .sample_rate = SAMPLE_RATE,                        // Frequenza di campionamento
        .bits_per_sample = BITS_PER_SAMPLE,                // Bit per campione
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,      // Formato canale (Left-Right per stereo)
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Standard I2S
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,          // Flag di allocazione interrupt
        .dma_buf_count = DMA_BUF_COUNT,                    // Numero di buffer DMA
        .dma_buf_len = DMA_BUF_LEN,                        // Lunghezza di ogni buffer DMA
        .use_apll = false,                                 // Non usare APLL (Phase-Locked Loop Audio)
        .tx_desc_auto_clear = true,                        // Auto-clear dei descrittori DMA di trasmissione
        .fixed_mclk = 0                                    // Nessun master clock fisso
    };
    
    // Configurazione dei pin I2S
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_PIN,     // Pin Bit Clock
        .ws_io_num = I2S_WS_PIN,       // Pin Word Select (LR Clock)
        .data_out_num = I2S_DATA_PIN,  // Pin Data Out
        .data_in_num = I2S_PIN_NO_CHANGE // Non usato per la trasmissione
    };
    
    // Installazione del driver I2S
    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore installazione driver I2S: %s\n", esp_err_to_name(err));
        return false;
    }
    
    delay(100);
    esp_task_wdt_reset();
    
    // Configurazione dei pin I2S
    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore configurazione pin I2S: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM); // Disinstalla il driver in caso di errore
        return false;
    }
    
    delay(100);
    esp_task_wdt_reset();
    
    // Avvio dell'interfaccia I2S
    err = i2s_start(I2S_NUM);
    if (err != ESP_OK) {
        Serial.printf("❌ Errore avvio I2S: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM);
        return false;
    }
    
    i2s_zero_dma_buffer(I2S_NUM); // Pulisce i buffer DMA
    
    Serial.println("✅ I2S configurato e avviato correttamente");
    Serial.printf("  📌 Pin Configuration:\n");
    Serial.printf("    BCK (Bit Clock): GPIO %d\n", I2S_BCK_PIN);
    Serial.printf("    WS (Word Select): GPIO %d\n", I2S_WS_PIN);
    Serial.printf("    DATA (Audio Out): GPIO %d\n", I2S_DATA_PIN);
    Serial.printf("  🎵 Audio Configuration:\n");
    Serial.printf("    Sample Rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("    Bits per Sample: %d\n", (int)BITS_PER_SAMPLE);
    Serial.printf("    Channels: %d (Stereo Output)\n", 2); // Output I2S è sempre stereo (L/R)
    Serial.printf("    DMA Buffers: %d x %d samples\n", DMA_BUF_COUNT, DMA_BUF_LEN);
    Serial.printf("    Volume: %.0f%% (fisso)\n", VOLUME * 100);
    
    i2s_initialized = true;
    return true;
}

// Funzione per riprodurre un tono di test sinusoidale
void playTestTone() {
    if (!i2s_initialized) {
        Serial.println("❌ I2S non inizializzato, impossibile riprodurre tono di test.");
        return;
    }
    
    Serial.println("🎵 Riproduzione tono di test (440 Hz per 2 secondi, Volume: 50%)...");
    
    const int frequency = 440; // Frequenza del tono
    const int duration_ms = 2000; // Durata del tono in millisecondi
    const int samples_per_cycle = SAMPLE_RATE / frequency; // Campioni per ciclo d'onda
    const int total_samples = (SAMPLE_RATE * duration_ms) / 1000; // Campioni totali da generare
    
    const int fade_samples = SAMPLE_RATE / 20; // Campioni per il fade-in/fade-out (50ms)
    
    // Genera e invia i campioni del tono di test
    for (int i = 0; i < total_samples; i += DMA_BUF_LEN) {
        int samples_to_write = MIN(DMA_BUF_LEN, total_samples - i);
        
        for (int j = 0; j < samples_to_write; j++) {
            int sample_index = i + j;
            float phase = (2.0 * PI * sample_index) / samples_per_cycle;
            
            float amplitude = 1.0;
            // Applica fade-in
            if (sample_index < fade_samples) {
                amplitude = (float)sample_index / fade_samples;
            } 
            // Applica fade-out
            else if (sample_index > total_samples - fade_samples) {
                amplitude = (float)(total_samples - sample_index) / fade_samples;
            }
            
            // Genera il sample sinusoidale e applica volume fisso
            int16_t sample_value = (int16_t)(sin(phase) * 8000 * amplitude * VOLUME);
            
            audio_buffer[j * 2] = sample_value;     // Canale Left
            audio_buffer[j * 2 + 1] = sample_value; // Canale Right
        }
        
        size_t bytes_written;
        // Scrive su I2S. Ogni sample è stereo (Left+Right), quindi 4 byte (2*int16_t) per coppia.
        esp_err_t err = i2s_write(I2S_NUM, audio_buffer, samples_to_write * 4, &bytes_written, portMAX_DELAY);
        
        if (err != ESP_OK) {
            Serial.printf("❌ Errore scrittura I2S per tono di test: %s\n", esp_err_to_name(err));
            break;
        }
        
        if (i % (DMA_BUF_LEN * 10) == 0) { // Reset watchdog periodicamente
            esp_task_wdt_reset();
            delay(1);
        }
    }
    
    delay(100); // Piccolo ritardo dopo la riproduzione
    Serial.println("✅ Tono di test completato (Volume: 50%)");
}

// Funzione helper per convertire una Stringa in minuscolo
String toLowerCase(String str) {
    String result = str;
    for (int i = 0; i < result.length(); i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

// Funzione per ottenere la lista di tutti i percorsi dei file WAV su LittleFS
std::vector<String> getWAVFilePaths() {
    std::vector<String> filePaths;
    Serial.println("\n📁 Scansione file WAV in LittleFS:");
    esp_task_wdt_reset(); // Reset del watchdog
    
    File root = LittleFS.open("/"); // Apre la directory root
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Errore apertura directory root.");
        return filePaths; // Restituisce un vettore vuoto
    }
    
    File file = root.openNextFile();
    int totalFilesScanned = 0;
    
    // Scansiona tutti i file nella directory root
    while (file) {
        totalFilesScanned++;
        String filename = String(file.name());
        String lowerFilename = toLowerCase(filename);
        
        // Controlla se l'estensione è .wav
        if (lowerFilename.endsWith(".wav")) {
            Serial.printf("  🎵 Trovato file WAV: %s\n", filename.c_str());
            Serial.printf("      Dimensione: %zu bytes (%.2f KB)\n",
                          file.size(), file.size() / 1024.0);
            
            // Stima la durata basandosi sulla dimensione del file e la configurazione I2S
            if (file.size() > 44) { // Assumendo un header WAV standard di 44 byte
                size_t audioDataBytes = file.size() - 44;
                // Calcolo della durata stimata (byte / (campioni/sec * canali * byte/campione))
                // Qui usiamo CHANNELS * 2 perché I2S è configurato per stereo (2 canali)
                float duration = (float)audioDataBytes / (SAMPLE_RATE * 2 * (BITS_PER_SAMPLE / 8)); 
                Serial.printf("      Durata stimata: %.1f secondi\n", duration);
            }
            
            filePaths.push_back("/" + filename); // Aggiungi il percorso completo al vettore
        }
        
        file.close(); // Chiudi il file prima di passare al successivo
        file = root.openNextFile();
        
        if (totalFilesScanned % 10 == 0) { // Reset del watchdog ogni 10 file scansionati
            esp_task_wdt_reset();
        }
    }
    root.close(); // Chiudi la directory root
    
    Serial.printf("\nTotale file scansionati: %d\n", totalFilesScanned);
    if (filePaths.empty()) {
        Serial.println("  📭 Nessun file WAV trovato.");
        Serial.println("  📤 Per caricare file: crea cartella 'data/' nella root del tuo progetto PlatformIO");
        Serial.println("      e usa il comando 'pio run --target uploadfs' per caricarli.");
        Serial.println("  💡 Esempio struttura del progetto:");
        Serial.println("      project/");
        Serial.println("      ├── src/main.cpp");
        Serial.println("      └── data/");
        Serial.println("          ├── my_song.wav");
        Serial.println("          └── another_sound.wav");
    }
    return filePaths; // Restituisce il vettore dei percorsi dei file WAV
}

// Funzione per controllare e stampare lo stato di salute del sistema
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
    Serial.printf("  🔊 Volume: %.0f%% (fisso)\n", VOLUME * 100);
    
    esp_task_wdt_reset(); // Reset del watchdog
}
