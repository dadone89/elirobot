#ifndef OCCHIO_H
#define OCCHIO_H

// Inclusione delle nuove librerie Adafruit per il display
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"  // Assicurati di aver installato Adafruit_GC9A01_Library
#include <SPI.h>               // Necessario per il bus SPI
#include <Arduino.h>           // Per definizioni come uint16_t e Serial
#include <pgmspace.h>          // Per PROGMEM

// Dati dell'immagine dell'occhio (se definiti qui, altrimenti assicurati che 'occhio' sia esterno)
extern const uint16_t occhio[] PROGMEM;
extern const uint16_t happy[] PROGMEM;

// Dichiarazioni delle dimensioni dell'immagine
#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 240

// Dimensioni del display GC9A01
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// I costruttori della Adafruit_GC9A01A per hardware SPI.
Adafruit_GC9A01A tft1(TFT1_CS, SHARED_DC, TFT1_RST);  // Display 1
Adafruit_GC9A01A tft2(TFT2_CS, SHARED_DC, TFT2_RST);  // Display 2

// Funzione per disegnare un'immagine NORMALE (senza modifiche)
void drawNormalImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;
  display.drawRGBBitmap(x_pos, y_pos + y_offset_image, (uint16_t *)data, IMAGE_WIDTH, IMAGE_HEIGHT);
}

// Funzione per disegnare un'immagine specchiata orizzontalmente
void drawMirroredImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Errore: Impossibile allocare buffer riga per mirroring.");
    return;
  }

  for (int j = 0; j < IMAGE_HEIGHT; j++) {
    for (int i = 0; i < IMAGE_WIDTH; i++) {
      row_buffer[i] = pgm_read_word(&data[j * IMAGE_WIDTH + i]);
    }

    uint16_t *mirrored_row = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
    if (!mirrored_row) {
      Serial.println("Errore: Impossibile allocare buffer per riga specchiata.");
      free(row_buffer);
      return;
    }
    for (int i = 0; i < IMAGE_WIDTH; i++) {
      mirrored_row[i] = row_buffer[IMAGE_WIDTH - 1 - i];
    }

    display.drawRGBBitmap(x_pos, y_pos + j + y_offset_image, mirrored_row, IMAGE_WIDTH, 1);

    free(mirrored_row);
  }
  free(row_buffer);
}

// Funzione per disegnare un'immagine specchiata verticalmente
void drawFlippedImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Errore: Impossibile allocare buffer riga per flip.");
    return;
  }

  for (int j = 0; j < IMAGE_HEIGHT; j++) {
    for (int i = 0; i < IMAGE_WIDTH; i++) {
      row_buffer[i] = pgm_read_word(&data[(IMAGE_HEIGHT - 1 - j) * IMAGE_WIDTH + i]);
    }

    display.drawRGBBitmap(x_pos, y_pos + j + y_offset_image, row_buffer, IMAGE_WIDTH, 1);
  }
  free(row_buffer);
}

// Funzione per disegnare un'immagine specchiata Orizzontalmente e Verticalmente
void drawMirroredAndFlippedImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Errore: Impossibile allocare buffer riga per mirror+flip.");
    return;
  }

  for (int j = 0; j < IMAGE_HEIGHT; j++) {
    for (int i = 0; i < IMAGE_WIDTH; i++) {
      row_buffer[i] = pgm_read_word(&data[(IMAGE_HEIGHT - 1 - j) * IMAGE_WIDTH + (IMAGE_WIDTH - 1 - i)]);
    }

    display.drawRGBBitmap(x_pos, y_pos + j + y_offset_image, row_buffer, IMAGE_WIDTH, 1);
  }
  free(row_buffer);
}

// Funzione per inizializzare i display degli occhi
void initializeEyeDisplays() {
  // Inizializza il bus SPI globale con i pin condivisi.
  SPI.begin(SHARED_SCLK, -1, SHARED_MOSI, -1);

  // --- CONFIGURAZIONE DISPLAY 1 (Occhio Sinistro) ---
  Serial.println("Inizializzazione Occhio 1...");
  tft1.begin();
  tft1.setRotation(0);
  tft1.fillScreen(0x0000);  // Nero

  //int x_offset_1 = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  //int y_offset_1 = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;
  //tft1.drawRGBBitmap(x_offset_1, y_offset_1, happy, IMAGE_WIDTH, IMAGE_HEIGHT);

  // --- CONFIGURAZIONE DISPLAY 2 (Occhio Destro - specchiato) ---
  Serial.println("Inizializzazione Occhio 2...");
  tft2.begin();
  tft2.setRotation(0);
  tft2.fillScreen(0x0000);

  int x_offset_2 = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_offset_2 = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  // Esempio di utilizzo dell'offset Y nella chiamata:
  // drawMirroredImage(tft2, x_offset_2, y_offset_2, IMAGE_WIDTH, IMAGE_HEIGHT, occhio, 10); // Sposta in basso di 10 pixel
  // drawMirroredImage(tft2, x_offset_2, y_offset_2, IMAGE_WIDTH, IMAGE_HEIGHT, occhio, -5); // Sposta in alto di 5 pixel
  
  //drawMirroredImage(tft2, x_offset_2, y_offset_2, IMAGE_WIDTH, IMAGE_HEIGHT, happy, -20);

  Serial.println("Display inizializzati.");
}

#endif  // OCCHIO_H