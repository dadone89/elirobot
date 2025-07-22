#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // Disegna rettangoli colorati per il test
  tft.fillRect(10, 10, 70, 70, TFT_RED);     // Dovrebbe essere ROSSO
  tft.fillRect(90, 10, 70, 70, TFT_GREEN);   // Dovrebbe essere VERDE  
  tft.fillRect(170, 10, 70, 70, TFT_BLUE);   // Dovrebbe essere BLU
  
  tft.fillRect(10, 90, 70, 70, TFT_YELLOW);  // Dovrebbe essere GIALLO
  tft.fillRect(90, 90, 70, 70, TFT_MAGENTA); // Dovrebbe essere MAGENTA
  tft.fillRect(170, 90, 70, 70, TFT_CYAN);   // Dovrebbe essere CIANO
  
  // Testo bianco
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Test Colori", 80, 200);
}

void loop() {
  // Vuoto
}