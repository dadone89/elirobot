#ifndef OCCHIO_H
#define OCCHIO_H

// Include new Adafruit libraries for the display
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"  // Make sure you have installed Adafruit_GC9A01_Library
#include <SPI.h>               // Required for SPI bus
#include <Arduino.h>           // For definitions like uint16_t and Serial
#include <pgmspace.h>          // For PROGMEM

// Eye image data (if defined here, otherwise make sure 'occhio' is external)
extern const uint16_t occhio[] PROGMEM;
extern const uint16_t happy[] PROGMEM;

// Image dimension declarations
#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 240

// GC9A01 display dimensions
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// Adafruit_GC9A01A constructors for hardware SPI.
Adafruit_GC9A01A tft1(TFT1_CS, SHARED_DC, TFT1_RST);  // Display 1
Adafruit_GC9A01A tft2(TFT2_CS, SHARED_DC, TFT2_RST);  // Display 2

// Function to draw a NORMAL image (without modifications)
void drawNormalImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;
  display.drawRGBBitmap(x_pos, y_pos + y_offset_image, (uint16_t *)data, IMAGE_WIDTH, IMAGE_HEIGHT);
}

// Function to draw a horizontally mirrored image
void drawMirroredImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Error: Unable to allocate row buffer for mirroring.");
    return;
  }

  for (int j = 0; j < IMAGE_HEIGHT; j++) {
    for (int i = 0; i < IMAGE_WIDTH; i++) {
      row_buffer[i] = pgm_read_word(&data[j * IMAGE_WIDTH + i]);
    }

    uint16_t *mirrored_row = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
    if (!mirrored_row) {
      Serial.println("Error: Unable to allocate buffer for mirrored row.");
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

// Function to draw a vertically flipped image
void drawFlippedImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Error: Unable to allocate row buffer for flip.");
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

// Function to draw an image mirrored both Horizontally and Vertically
void drawMirroredAndFlippedImage(Adafruit_GFX &display, const uint16_t *data, int y_offset_image = 0) {
  int x_pos = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_pos = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  uint16_t *row_buffer = (uint16_t *)malloc(IMAGE_WIDTH * sizeof(uint16_t));
  if (!row_buffer) {
    Serial.println("Error: Unable to allocate row buffer for mirror+flip.");
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

// Function to initialize the eye displays
void initializeEyeDisplays() {
  // Initialize the global SPI bus with shared pins.
  SPI.begin(SHARED_SCLK, -1, SHARED_MOSI, -1);

  // --- DISPLAY 1 CONFIGURATION (Left Eye) ---
  Serial.println("Initializing Eye 1...");
  tft1.begin();
  tft1.setRotation(0);
  tft1.fillScreen(0x0000);  // Black

  // --- DISPLAY 2 CONFIGURATION (Right Eye - mirrored) ---
  Serial.println("Initializing Eye 2...");
  tft2.begin();
  tft2.setRotation(0);
  tft2.fillScreen(0x0000);

  int x_offset_2 = (TFT_WIDTH - IMAGE_WIDTH) / 2;
  int y_offset_2 = (TFT_HEIGHT - IMAGE_HEIGHT) / 2;

  Serial.println("Displays initialized.");
}

#endif  // OCCHIO_H