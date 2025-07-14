// audio.h
// Contains declarations and implementations of functions for I2S audio playback
// and tone generation.

#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>     // For functions like millis(), Serial, etc.
#include <LittleFS.h>    // For file management on the filesystem
#include <driver/i2s.h>  // For the ESP32 I2S driver
#include <math.h>        // For the sin() function used in tone generation
#include "config.h"      // For I2S and volume constant definitions

// Forward declarations of functions
void stopAudioPlayback();

// Simplified WAV header
// This structure defines the essential fields of a WAV file header.
struct WAVHeader {
  char riff[4];            // "RIFF"
  uint32_t fileSize;       // Total file size
  char wave[4];            // "WAVE"
  char fmt[4];             // "fmt "
  uint32_t fmtSize;        // Size of the "fmt" chunk (usually 16)
  uint16_t audioFormat;    // Audio format (1 for PCM)
  uint16_t numChannels;    // Number of channels (1 for mono, 2 for stereo)
  uint32_t sampleRate;     // Sample rate (e.g., 44100 Hz)
  uint32_t byteRate;       // Bytes per second (SampleRate * NumChannels * BitsPerSample/8)
  uint16_t blockAlign;     // Bytes per block (NumChannels * BitsPerSample/8)
  uint16_t bitsPerSample;  // Bits per sample (e.g., 16)
  char data[4];            // "data"
  uint32_t dataSize;       // Size of the audio data
};

// Declarations and definitions of global variables for audio management
static int16_t audio_buffer[DMA_BUF_LEN * 2];  // Buffer for DMA audio samples
bool i2s_initialized = false;                  // Flag to indicate if I2S has been initialized
bool wavFilesExists = false;                   // Flag to indicate if all necessary WAV files exist
bool isPlayingAudio = false;                   // Flag to indicate if audio is currently playing
File currentAudioFile;                         // File object for the currently playing audio file
size_t currentBytesRead = 0;                   // Count of bytes read from the current file
size_t currentTotalBytes = 0;                  // Total size of audio data in the current file
WAVHeader currentHeader;                       // Header of the currently playing WAV file

// ========== AUDIO PLAYBACK FUNCTIONS ==========

// Initializes the I2S driver with the configurations specified in config.h.
bool initializeI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),  // Master and transmit mode
    .sample_rate = SAMPLE_RATE,                           // Sample rate
    .bits_per_sample = BITS_PER_SAMPLE,                   // Bits per sample (e.g., 16 bit)
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,         // Channel format (stereo)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,    // Standard I2S communication format
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,             // Flags for interrupt allocation
    .dma_buf_count = DMA_BUF_COUNT,                       // Number of DMA buffers
    .dma_buf_len = DMA_BUF_LEN,                           // Length of each DMA buffer in samples
    .use_apll = false,                                    // Do not use APLL
    .tx_desc_auto_clear = true,                           // Automatically clear transmit DMA descriptors
    .fixed_mclk = 0                                       // No fixed master clock
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,        // Pin for BCLK clock
    .ws_io_num = I2S_WS_PIN,          // Pin for word select (LRCLK)
    .data_out_num = I2S_DATA_PIN,     // Pin for data output
    .data_in_num = I2S_PIN_NO_CHANGE  // No pin for data input
  };

  // Install the I2S driver
  if (i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL) != ESP_OK) return false;
  // Set I2S pins
  if (i2s_set_pin(I2S_NUM, &pin_config) != ESP_OK) return false;
  // Start the I2S driver
  if (i2s_start(I2S_NUM) != ESP_OK) return false;

  i2s_zero_dma_buffer(I2S_NUM);  // Clear the I2S DMA buffer
  i2s_initialized = true;        // Set the initialization flag
  return true;
}

// Starts playback of a WAV audio file from the filesystem.
bool playAudioFile(const char *filename) {
  // If audio is already playing, stop it before starting a new file.
  if (isPlayingAudio) {
    stopAudioPlayback();
  }

  // Check that I2S is initialized and WAV files exist.
  if (!i2s_initialized || !wavFilesExists) {
    Serial.println("❌ Audio not available");
    return false;
  }

  // Close the previously opened audio file, if any.
  if (currentAudioFile) {
    currentAudioFile.close();
  }

  // Open the new audio file in read mode.
  currentAudioFile = LittleFS.open(filename, "r");
  if (!currentAudioFile) {
    Serial.printf("❌ Unable to open file: %s\n", filename);
    return false;
  }

  // Read the WAV file header.
  if (currentAudioFile.read((uint8_t *)&currentHeader, sizeof(WAVHeader)) != sizeof(WAVHeader)) {
    Serial.println("❌ Error reading WAV header");
    currentAudioFile.close();
    return false;
  }

  // Verify the WAV file format (must be RIFF, WAVE, and PCM).
  if (memcmp(currentHeader.riff, "RIFF", 4) != 0 || memcmp(currentHeader.wave, "WAVE", 4) != 0 || currentHeader.audioFormat != 1) {
    Serial.println("❌ Unsupported WAV format (PCM only)");
    currentAudioFile.close();
    return false;
  }

  // Position the file pointer at the beginning of the audio data.
  currentAudioFile.seek(sizeof(WAVHeader));
  currentTotalBytes = currentHeader.dataSize;  // Set the total audio data size
  currentBytesRead = 0;                        // Reset the bytes read counter
  isPlayingAudio = true;                       // Set the playing audio flag

  Serial.printf("🎵 Playing: %s\n", filename);
  return true;
}

// Processes a chunk of audio data and sends it to the I2S driver.
void processAudioChunk() {
  // If no file is open or all bytes have been read, stop playback.
  if (!currentAudioFile || currentBytesRead >= currentTotalBytes) {
    stopAudioPlayback();
    return;
  }

  // Calculate how many bytes to read in the current chunk.
  size_t bytesToRead = MIN((size_t)DMA_BUF_LEN * (currentHeader.bitsPerSample / 8),
                           currentTotalBytes - currentBytesRead);

  // If there are no more bytes to read, stop playback.
  if (bytesToRead == 0) {
    stopAudioPlayback();
    return;
  }

  // Read audio data from the file into the temporary buffer.
  uint8_t *tempReadBuffer = (uint8_t *)audio_buffer;  // Use audio_buffer as a temporary buffer
  size_t actualRead = currentAudioFile.read(tempReadBuffer, bytesToRead);

  // If no bytes were read, stop playback.
  if (actualRead == 0) {
    stopAudioPlayback();
    return;
  }

  size_t actualSamplesRead = actualRead / (currentHeader.bitsPerSample / 8);

  // If the file is mono, duplicate samples to create stereo output.
  if (currentHeader.numChannels == 1) {
    for (int i = actualSamplesRead - 1; i >= 0; i--) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      int16_t volumeAdjusted = (int16_t)(sample * VOLUME);  // Apply volume

      audio_buffer[i * 2] = volumeAdjusted;      // Left channel
      audio_buffer[i * 2 + 1] = volumeAdjusted;  // Right channel
    }
    actualSamplesRead *= 2;  // Number of samples doubles for stereo
  } else {                   // If the file is already stereo
    for (int i = 0; i < actualSamplesRead; i++) {
      int16_t sample = ((int16_t *)tempReadBuffer)[i];
      audio_buffer[i] = (int16_t)(sample * VOLUME);  // Apply volume
    }
  }

  size_t bytes_written;
  size_t bytesToWrite = actualSamplesRead * sizeof(int16_t);  // Calculate bytes to write
  // Write audio data to the I2S driver
  esp_err_t err = i2s_write(I2S_NUM, audio_buffer, bytesToWrite, &bytes_written, 0);

  if (err == ESP_OK) {
    currentBytesRead += actualRead;  // Update the bytes read counter
  } else {
    Serial.printf("❌ I2S write error: %s\n", esp_err_to_name(err));
    stopAudioPlayback();  // In case of error, stop playback
  }
}

// Stops current audio playback and closes the file.
void stopAudioPlayback() {
  if (currentAudioFile) {
    currentAudioFile.close();  // Close the file if it's open
  }
  isPlayingAudio = false;  // Set the playing flag to false
  Serial.println("✅ Playback completed");
}

// Checks if an audio file exists in the filesystem and prints its size.
bool checkAudioFileExists(const char *filename) {
  File file = LittleFS.open(filename, "r");  // Attempt to open the file
  if (!file) {
    Serial.printf("File %s not found\n", filename);
    return false;  // File not found
  }

  size_t fileSize = file.size();  // Get the file size
  file.close();                   // Close the file

  Serial.printf("File %s found, size: %zu bytes\n", filename, fileSize);
  return fileSize > sizeof(WAVHeader);  // Return true if the file exists and has a minimum size
}

// Plays a tone of a given frequency and duration using I2S output.
void playTone(int frequency, int duration_ms) {
  if (!i2s_initialized) {
    Serial.println("❌ I2S not initialized, cannot play tone.");
    return;
  }

  // Stop any current audio playback to ensure the tone is audible.
  stopAudioPlayback();

  Serial.printf("🎵 Playing tone: %d Hz for %d ms\n", frequency, duration_ms);

  // Calculate the total number of samples for the tone duration.
  int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
  size_t bytes_written;

  // Generate and play the tone in blocks (DMA_BUF_LEN buffer).
  for (int i = 0; i < num_samples; i += DMA_BUF_LEN) {
    int samples_to_generate = MIN(DMA_BUF_LEN, num_samples - i);  // Samples to generate in the current block
    for (int j = 0; j < samples_to_generate; j++) {
      // Calculate the sample value for the sine wave.
      // Multiply by 32767 to get the full range of an int16_t and apply VOLUME.
      int16_t sample = (int16_t)(sin(2 * PI * frequency * ((float)(i + j) / SAMPLE_RATE)) * 32767 * VOLUME);

      // Write the sample for both channels (stereo).
      audio_buffer[j * 2] = sample;
      audio_buffer[j * 2 + 1] = sample;
    }
    // Write samples to the I2S driver, waiting for the buffer to be available.
    i2s_write(I2S_NUM, audio_buffer, samples_to_generate * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }

  // Clear the I2S DMA buffer after tone playback to avoid residue.
  i2s_zero_dma_buffer(I2S_NUM);
}

#endif  // AUDIO_H