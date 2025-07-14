// audio.h
// Contiene le dichiarazioni e implementazioni delle funzioni per la riproduzione audio I2S
// e la generazione di toni.

#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>        // Per funzioni come millis(), Serial, ecc.
#include <LittleFS.h>       // Per la gestione dei file sul filesystem
#include <driver/i2s.h>     // Per il driver I2S dell'ESP32
#include <math.h>           // Per la funzione sin() usata nella generazione dei toni
#include "config.h"         // Per le definizioni delle costanti I2S e volume

// Dichiarazioni anticipate delle funzioni
void stopAudioPlayback();

// Header WAV semplificato
// Questa struttura definisce i campi essenziali di un header di file WAV.
struct WAVHeader {
  char riff[4];         // "RIFF"
  uint32_t fileSize;    // Dimensione totale del file
  char wave[4];         // "WAVE"
  char fmt[4];          // "fmt "
  uint32_t fmtSize;     // Dimensione del chunk "fmt" (solitamente 16)
  uint16_t audioFormat; // Formato audio (1 per PCM)
  uint16_t numChannels; // Numero di canali (1 per mono, 2 per stereo)
  uint32_t sampleRate;  // Frequenza di campionamento (es. 44100 Hz)
  uint32_t byteRate;    // Byte per secondo (SampleRate * NumChannels * BitsPerSample/8)
  uint16_t blockAlign;  // Byte per blocco (NumChannels * BitsPerSample/8)
  uint16_t bitsPerSample; // Bit per campione (es. 16)
  char data[4];         // "data"
  uint32_t dataSize;    // Dimensione dei dati audio
};

// Dichiarazioni e definizioni delle variabili globali per la gestione dell'audio
static int16_t audio_buffer[DMA_BUF_LEN * 2]; // Buffer per i campioni audio DMA
bool i2s_initialized = false;             // Flag per indicare se I2S è stato inizializzato
bool wavFilesExists = false;              // Flag per indicare se tutti i file WAV necessari esistono
bool isPlayingAudio = false;              // Flag per indicare se un audio è in riproduzione
File currentAudioFile;                    // Oggetto File per il file audio attualmente in riproduzione
size_t currentBytesRead = 0;              // Conteggio dei byte letti dal file corrente
size_t currentTotalBytes = 0;             // Dimensione totale dei dati audio nel file corrente
WAVHeader currentHeader;                  // Header del file WAV attualmente in riproduzione

// ========== FUNZIONI DI RIPRODUZIONE AUDIO ==========

// Inizializza il driver I2S con le configurazioni specificate in config.h.
bool initializeI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Modalità master e trasmissione
    .sample_rate = SAMPLE_RATE,                          // Frequenza di campionamento
    .bits_per_sample = BITS_PER_SAMPLE,                  // Bit per campione (es. 16 bit)
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,        // Formato canali (stereo)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Formato di comunicazione I2S standard
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Flag per l'allocazione dell'interrupt
    .dma_buf_count = DMA_BUF_COUNT,                      // Numero di buffer DMA
    .dma_buf_len = DMA_BUF_LEN,                          // Lunghezza di ogni buffer DMA in campioni
    .use_apll = false,                                   // Non usare APLL
    .tx_desc_auto_clear = true,                          // Cancella automaticamente i descrittori DMA di trasmissione
    .fixed_mclk = 0                                      // Nessun clock master fisso
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,     // Pin per il clock BCLK
    .ws_io_num = I2S_WS_PIN,       // Pin per il word select (LRCLK)
    .data_out_num = I2S_DATA_PIN,  // Pin per i dati in uscita
    .data_in_num = I2S_PIN_NO_CHANGE // Nessun pin per i dati in ingresso
  };

  // Installa il driver I2S
  if (i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL) != ESP_OK) return false;
  // Imposta i pin I2S
  if (i2s_set_pin(I2S_NUM, &pin_config) != ESP_OK) return false;
  // Avvia il driver I2S
  if (i2s_start(I2S_NUM) != ESP_OK) return false;

  i2s_zero_dma_buffer(I2S_NUM); // Pulisce il buffer DMA I2S
  i2s_initialized = true;       // Imposta il flag di inizializzazione
  return true;
}

// Avvia la riproduzione di un file audio WAV dal filesystem.
bool playAudioFile(const char *filename) {
  // Se c'è già un audio in riproduzione, lo ferma prima di iniziare un nuovo file.
  if (isPlayingAudio) {
    stopAudioPlayback();
  }

  // Verifica che I2S sia inizializzato e che i file WAV esistano.
  if (!i2s_initialized || !wavFilesExists) {
    Serial.println("❌ Audio non disponibile");
    return false;
  }

  // Chiude il file audio precedentemente aperto, se presente.
  if (currentAudioFile) {
    currentAudioFile.close();
  }

  // Apre il nuovo file audio in modalità lettura.
  currentAudioFile = LittleFS.open(filename, "r");
  if (!currentAudioFile) {
    Serial.printf("❌ Impossibile aprire file: %s\n", filename);
    return false;
  }

  // Legge l'header del file WAV.
  if (currentAudioFile.read((uint8_t *)&currentHeader, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
    Serial.println("❌ Errore lettura header WAV");
    currentAudioFile.close();
    return false;
  }

  // Verifica il formato del file WAV (deve essere RIFF, WAVE e PCM).
  if (memcmp(currentHeader.riff, "RIFF", 4) != 0 || memcmp(currentHeader.wave, "WAVE", 4) != 0 || currentHeader.audioFormat != 1) {
    Serial.println("❌ Formato WAV non supportato (solo PCM)");
    currentAudioFile.close();
    return false;
  }

  // Posiziona il puntatore del file all'inizio dei dati audio.
  currentAudioFile.seek(sizeof(WAVHeader));
  currentTotalBytes = currentHeader.dataSize; // Imposta la dimensione totale dei dati audio
  currentBytesRead = 0;                       // Resetta il contatore dei byte letti
  isPlayingAudio = true;                      // Imposta il flag di riproduzione in corso

  Serial.printf("🎵 Riproduzione: %s\n", filename);
  return true;
}

// Processa un chunk di dati audio e lo invia al driver I2S.
void processAudioChunk() {
  // Se non c'è un file aperto o tutti i byte sono stati letti, ferma la riproduzione.
  if (!currentAudioFile || currentBytesRead >= currentTotalBytes) {
    stopAudioPlayback();
    return;
  }

  // Calcola quanti byte leggere nel chunk corrente.
  size_t bytesToRead = MIN((size_t)DMA_BUF_LEN * (currentHeader.bitsPerSample / 8),
                           currentTotalBytes - currentBytesRead);

  // Se non ci sono più byte da leggere, ferma la riproduzione.
  if (bytesToRead == 0) {
    stopAudioPlayback();
    return;
  }

  // Legge i dati audio dal file nel buffer temporaneo.
  uint8_t *tempReadBuffer = (uint8_t *)audio_buffer; // Usa audio_buffer come buffer temporaneo
  size_t actualRead = currentAudioFile.read(tempReadBuffer, bytesToRead);

  // Se non sono stati letti byte, ferma la riproduzione.
  if (actualRead == 0) {
    stopAudioPlayback();
    return;
  }

  size_t actualSamplesRead = actualRead / (currentHeader.bitsPerSample / 8);

  // Se il file è mono, duplica i campioni per creare un output stereo.
  if (currentHeader.numChannels == 1) {
    for (int i = actualSamplesRead - 1; i >= 0; i--) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      int16_t volumeAdjusted = (int16_t)(sample * VOLUME); // Applica il volume

      audio_buffer[i * 2] = volumeAdjusted;     // Canale sinistro
      audio_buffer[i * 2 + 1] = volumeAdjusted; // Canale destro
    }
    actualSamplesRead *= 2; // Il numero di campioni raddoppia per lo stereo
  } else { // Se il file è già stereo
    for (int i = 0; i < actualSamplesRead; i++) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      audio_buffer[i] = (int16_t)(sample * VOLUME); // Applica il volume
    }
  }

  size_t bytes_written;
  size_t bytesToWrite = actualSamplesRead * sizeof(int16_t); // Calcola i byte da scrivere
  // Scrive i dati audio al driver I2S
  esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWrite, &bytes_written, 0);

  if (err == ESP_OK) {
    currentBytesRead += actualRead; // Aggiorna il contatore dei byte letti
  } else {
    Serial.printf("❌ Errore I2S write: %s\n", esp_err_to_name(err));
    stopAudioPlayback(); // In caso di errore, ferma la riproduzione
  }
}

// Ferma la riproduzione audio corrente e chiude il file.
void stopAudioPlayback() {
  if (currentAudioFile) {
    currentAudioFile.close(); // Chiude il file se è aperto
  }
  isPlayingAudio = false; // Imposta il flag di riproduzione su falso
  Serial.println("✅ Riproduzione completata");
}

// Verifica se un file audio esiste nel filesystem e ne stampa la dimensione.
bool checkAudioFileExists(const char *filename) {
  File file = LittleFS.open(filename, "r"); // Tenta di aprire il file
  if (!file) {
    Serial.printf("File %s non trovato\n", filename);
    return false; // File non trovato
  }

  size_t fileSize = file.size(); // Ottiene la dimensione del file
  file.close(); // Chiude il file

  Serial.printf("File %s trovato, dimensione: %zu bytes\n", filename, fileSize);
  return fileSize > sizeof(WAVHeader); // Restituisce true se il file esiste e ha una dimensione minima
}

// Suona un tono di una data frequenza e durata utilizzando l'output I2S.
void playTone(int frequency, int duration_ms) {
  if (!i2s_initialized) {
    Serial.println("❌ I2S non inizializzato, impossibile suonare il tono.");
    return;
  }

  // Ferma qualsiasi riproduzione audio corrente per assicurare che il tono sia udibile.
  stopAudioPlayback();

  Serial.printf("🎵 Suono tono: %d Hz per %d ms\n", frequency, duration_ms);

  // Calcola il numero totale di campioni per la durata del tono.
  int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
  size_t bytes_written;

  // Genera e riproduci il tono in blocchi (buffer DMA_BUF_LEN).
  for (int i = 0; i < num_samples; i += DMA_BUF_LEN) {
    int samples_to_generate = MIN(DMA_BUF_LEN, num_samples - i); // Campioni da generare nel blocco corrente
    for (int j = 0; j < samples_to_generate; j++) {
      // Calcola il valore del campione per l'onda sinusoidale.
      // Moltiplica per 32767 per ottenere il range completo di un int16_t e applica il VOLUME.
      int16_t sample = (int16_t)(sin(2 * PI * frequency * ((float)(i + j) / SAMPLE_RATE)) * 32767 * VOLUME);

      // Scrive il campione per entrambi i canali (stereo).
      audio_buffer[j * 2] = sample;
      audio_buffer[j * 2 + 1] = sample;
    }
    // Scrive i campioni al driver I2S, aspettando che il buffer sia disponibile.
    i2s_write(I2S_NUM, audio_buffer, samples_to_generate * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }

  // Pulisce il buffer DMA I2S dopo la riproduzione del tono per evitare residui.
  i2s_zero_dma_buffer(I2S_NUM);
}

#endif // AUDIO_H
