// hardware_io.h
// Contiene le dichiarazioni e implementazioni delle funzioni per il controllo dei servi
// e la gestione dei pulsanti analogici.

#ifndef HARDWARE_IO_H
#define HARDWARE_IO_H

#include <Arduino.h>      // Per funzioni come delayMicroseconds(), Serial, ecc.
#include <ESP32Servo.h>   // Per la classe Servo
#include <driver/adc.h>   // Per le funzioni ADC dell'ESP32
#include "config.h"       // Per le definizioni dei pin e delle soglie

// Dichiarazioni delle istanze dei servi (definite in main.ino)
extern Servo myservo1;
extern Servo myservo2;

// Dichiarazioni anticipate delle funzioni
int decodeButton(int value, const int thresholds[]);
int getStableAnalogRead(int pin);
int analogReadLegacy(uint8_t gpio_num);


// ========== FUNZIONI DI CONTROLLO SERVO ==========

// Muove il robot in avanti.
void moveForward() {
  myservo1.write(180); // Motore 1 avanti (es. 180 gradi per servo continuo)
  myservo2.write(0);   // Motore 2 avanti (es. 0 gradi per servo continuo, se invertito)
}

// Muove il robot indietro.
void moveBackward() {
  myservo1.write(0);   // Motore 1 indietro
  myservo2.write(180); // Motore 2 indietro
}

// Muove il robot a sinistra.
void moveLeft() {
  myservo1.write(0);   // Motore 1 indietro (per girare a sinistra)
  myservo2.write(0);   // Motore 2 avanti (per girare a sinistra)
}

// Muove il robot a destra.
void moveRight() {
  myservo1.write(180); // Motore 1 avanti (per girare a destra)
  myservo2.write(180); // Motore 2 indietro (per girare a destra)
}

// Ferma entrambi i motori.
void stopMotors() {
  myservo1.write(90); // Ferma il motore 1 (posizione centrale per servo continuo)
  myservo2.write(90); // Ferma il motore 2
}

// ========== FUNZIONI DI GESTIONE PULSANTI ==========

// Decodifica un valore analogico in un ID pulsante basato sulle soglie.
// Restituisce l'indice del pulsante o BTN_NONE se non rientra in nessuna soglia.
int decodeButton(int value, const int thresholds[]) {
  if (value >= thresholds[0] && value < thresholds[1]) return 0;
  else if (value >= thresholds[1] && value < thresholds[2]) return 1;
  else if (value >= thresholds[2] && value < thresholds[3]) return 2;
  else if (value >= thresholds[3] && value < thresholds[4]) return 3;
  else if (value >= thresholds[4] && value <= thresholds[5]) return 4;
  else return BTN_NONE; // Nessun pulsante riconosciuto
}

// Esegue letture analogiche multiple da un pin per ottenere un valore più stabile.
// Calcola la media di 3 letture.
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogReadLegacy(pin); // Legge il valore analogico
    delayMicroseconds(100);       // Breve ritardo tra le letture
  }
  return sum / 3; // Restituisce la media
}

// Implementazione della funzione analogRead legacy per ESP32.
// Mappa i pin GPIO specifici ai canali ADC e configura l'ADC.
int analogReadLegacy(uint8_t gpio_num) {
  adc1_channel_t channel;
  adc_unit_t unit;

  // Mappa il pin GPIO al canale ADC corrispondente
  if (gpio_num == 34) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_6;
  } else if (gpio_num == 35) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_7;
  } else if (gpio_num == 36) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_0;
  } else if (gpio_num == 39) {
    unit = ADC_UNIT_1;
    channel = ADC1_CHANNEL_3;
  } else {
    return -1; // Pin non supportato
  }

  // Configura la larghezza di bit dell'ADC (12 bit per 0-4095)
  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK) {
    return -1;
  }

  // Configura l'attenuazione del canale ADC (11dB per il range completo)
  if (adc1_config_channel_atten(channel, ADC_ATTEN_DB_11) != ESP_OK) {
    return -1;
  }

  // Ottiene il valore raw dal canale ADC
  int raw_value = adc1_get_raw(channel);
  return raw_value;
}

#endif // HARDWARE_IO_H
