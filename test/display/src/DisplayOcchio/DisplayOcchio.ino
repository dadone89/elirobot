#include <TFT_eSPI.h> // Include la libreria TFT_eSPI
#include "occhio.h"   // Include il tuo file di intestazione per l'immagine

// Configurazione dei pin per il display GC9A01 con ESP32 Dev Module
// QUESTA SEZIONE DEVE CORRISPONDERE ALLA TUA CONFIGURAZIONE IN User_Setup.h
// Se hai modificato User_Setup.h direttamente, queste definizioni qui sono più che altro per riferimento.
// Assicurati che i pin corrispondano ai tuoi collegamenti hardware reali.

// Configurazione base per il driver GC9A01
#define GC9A01_DRIVER // Seleziona il driver GC9A01

#define TFT_WIDTH  240 // Larghezza del display GC9A01
#define TFT_HEIGHT 240 // Altezza del display GC9A01

// Definizione dei pin SPI per l'ESP32 (Controlla i tuoi collegamenti!)
#define TFT_MOSI 23 // GPIO23 (MOSI) - Data Out
#define TFT_SCLK 18 // GPIO18 (SCK) - Clock
#define TFT_CS    5 // GPIO5 (Chip Select) - Seleziona il display
#define TFT_DC    2 // GPIO2 (Data/Command) - Dice se stiamo inviando dati o comandi
#define TFT_RST   4 // GPIO4 (Reset) - Resetta il display

// Frequenza SPI (40MHz è comune e veloce per ESP32)
#define SPI_FREQUENCY  40000000

// Puoi aggiungere il backlight se hai un pin dedicato, altrimenti commenta
// #define TFT_BL   27 // Esempio: GPIO27 per il backlight
// #define TFT_BACKLIGHT_ON HIGH // O LOW, a seconda di come è cablato il tuo modulo BL

// Crea un'istanza dell'oggetto TFT
TFT_eSPI tft = TFT_eSPI();


void setup() {
  Serial.begin(115200);
  Serial.println("Avvio programma display occhio...");

  // Inizializza il display
  tft.init();

  // Imposta l'orientamento del display (0-3). Prova diverse rotazioni se l'immagine appare ruotata.
  tft.setRotation(0);

  // Opzionale: imposta il backlight se definito e vuoi controllarlo via software
  #ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  #endif

  // Pulisci lo schermo, utile per iniziare
  tft.fillScreen(TFT_BLACK); // Riempi lo schermo di nero

  Serial.println("Display inizializzato. Disegno l'immagine...");

  // Calcola le coordinate per centrare l'immagine (se l'immagine è più piccola del display)
  int x_offset = (TFT_WIDTH - image_width) / 2;
  int y_offset = (TFT_HEIGHT - image_height) / 2;

  // Disegna l'immagine. Ora i riferimenti all'immagine e alle sue dimensioni
  // vengono da occhio.h (che a sua volta si collega a occhio.c)
  tft.pushImage(x_offset, y_offset, image_width, image_height, occhio);

  Serial.println("Immagine dell'occhio disegnata sul display.");
}

void loop() {
  // Il loop non ha bisogno di fare nulla
}