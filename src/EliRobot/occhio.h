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

// Dichiarazioni delle dimensioni dell'immagine
const int image_width = 240;
const int image_height = 240;

// Pin CONDIVISI per entrambi i Display
#define SHARED_MOSI 23  // MOSI (Master Out Slave In)
#define SHARED_SCLK 18  // SCLK (Serial Clock)
#define SHARED_DC 2     // DC (Data/Command)

// Dimensioni del display GC9A01
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// I costruttori della Adafruit_GC9A01A per hardware SPI.
Adafruit_GC9A01A tft1(TFT1_CS, SHARED_DC, TFT1_RST);  // Display 1
Adafruit_GC9A01A tft2(TFT2_CS, SHARED_DC, TFT2_RST);  // Display 2

// Funzione per disegnare un'immagine specchiata orizzontalmente
// Aggiunto y_offset_image per spostare l'immagine verticalmente
void drawMirroredImage(Adafruit_GFX &display, int x, int y, int w, int h, const uint16_t *data, int y_offset_image = 0) {
  uint16_t *row_buffer = (uint16_t *)malloc(w * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Errore: Impossibile allocare buffer riga per mirroring.");
    return;
  }

  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      row_buffer[i] = data[j * w + i];
    }

    // Per disegnare una riga specchiata, ricrei una riga invertita e poi usi drawRGBBitmap.
    uint16_t *mirrored_row = (uint16_t *)malloc(w * sizeof(uint16_t));
    if (!mirrored_row) {
      Serial.println("Errore: Impossibile allocare buffer per riga specchiata.");
      free(row_buffer);
      return;
    }
    for (int i = 0; i < w; i++) {
      mirrored_row[i] = row_buffer[w - 1 - i];
    }

    // Applica l'offset Y qui
    display.drawRGBBitmap(x, y + j + y_offset_image, mirrored_row, w, 1);  // Disegna una riga alla volta con offset

    free(mirrored_row);
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

  int x_offset_1 = (TFT_WIDTH - image_width) / 2;
  int y_offset_1 = (TFT_HEIGHT - image_height) / 2;
  tft1.drawRGBBitmap(x_offset_1, y_offset_1, occhio, image_width, image_height);
  Serial.println("Occhio 1 disegnato.");

  // --- CONFIGURAZIONE DISPLAY 2 (Occhio Destro - specchiato) ---
  Serial.println("Inizializzazione Occhio 2...");
  tft2.begin();
  tft2.setRotation(0);
  tft2.fillScreen(0x0000);

  int x_offset_2 = (TFT_WIDTH - image_width) / 2;
  int y_offset_2 = (TFT_HEIGHT - image_height) / 2;

  // Esempio di utilizzo dell'offset Y nella chiamata:
  // drawMirroredImage(tft2, x_offset_2, y_offset_2, image_width, image_height, occhio, 10); // Sposta in basso di 10 pixel
  // drawMirroredImage(tft2, x_offset_2, y_offset_2, image_width, image_height, occhio, -5); // Sposta in alto di 5 pixel
  drawMirroredImage(tft2, x_offset_2, y_offset_2, image_width, image_height, occhio, 30);  // Nessun offset (default)

  Serial.println("Occhio 2 (specchiato) disegnato.");
  Serial.println("Display inizializzati e occhi disegnati.");
}

#endif  // OCCHIO_H