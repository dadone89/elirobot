// config.h
// Contiene tutte le definizioni di costanti, pin e soglie.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h> // Necessario per tipi come uint32_t, size_t, ecc.

// Macro helper per min/max con tipi diversi
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Pin audio I2S
#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DATA_PIN 22

// Pin PWM per i servo
#define SERVO_PIN_1 32
#define SERVO_PIN_2 33

// Pin analogici per pulsanti
#define BUTTONS_PIN_0 34
#define BUTTONS_PIN_1 35

// Pin analogici per fotoresistenze
#define PHOTORES_PIN_0 36
#define PHOTORES_PIN_1 39

// Configurazione I2S
#define I2S_NUM I2S_NUM_0
#define SAMPLE_RATE 8000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN 512
#define VOLUME 0.5f // Volume di riproduzione audio

// Percorsi dei file audio nel filesystem LittleFS
#define PRESENTATION_AUDIO "/elirobot.wav"
#define MOD_SEQ_AUDIO "/Frecce_e_centrale.wav"
#define MOD_DANCE_AUDIO "/Adesso_si_balla.wav"
#define MOD_DIRECT_AUDIO "/Telecomando!.wav"
#define MOD_FOLLOW_AUDIO "/Inseguo la luce.wav"

// Durata del movimento in millisecondi in modalità sequenza
#define MOVE_DURATION 2000
// Valore ADC corrispondente alla massima velocità in modalità follow
#define ADC_FOR_MAX_SPEED 500

// Definizioni delle modalità di funzionamento del robot
#define MODE_STANDBY 0
#define MODE_SEQUENCE 1
#define MODE_DIRECT 2
#define MODE_DANCE 3
#define MODE_FOLLOW 4

// Definizioni delle note musicali (frequenze in Hz)
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define TONE_DURATION_MS 300 // Durata predefinita dei toni per la pressione dei tasti

// Indici per la decodifica dei pulsanti
#define BTN_NONE -1
#define BTN_C 0   // Pulsante Centrale (collegato ad A0)
#define BTN_DL 1  // Pulsante Down-Left (collegato ad A0)
#define BTN_UL 2  // Pulsante Up-Left (collegato ad A0)
#define BTN_UR 3  // Pulsante Up-Right (collegato ad A0)
#define BTN_DR 4  // Pulsante Down-Right (collegato ad A0)
#define BTN_L 0   // Pulsante Left (collegato ad A1)
#define BTN_U 1   // Pulsante Up (collegato ad A1)
#define BTN_R 2   // Pulsante Right (collegato ad A1)
#define BTN_D 3   // Pulsante Down (collegato ad A1)

// Soglie per la decodifica dei valori ADC (12-bit ADC: 0-4095)
// Queste soglie sono specifiche per i pulsanti collegati ai pin analogici.
int thresholdsA0[] = { 0, 1200, 2200, 2800, 3050, 3600 };
int thresholdsA1[] = { 0, 1600, 2400, 2880, 3600, 4000 };

// Array di puntatori a stringhe per i nomi dei file audio
const char *audioFiles[] = {
  PRESENTATION_AUDIO,
  MOD_SEQ_AUDIO,
  MOD_DANCE_AUDIO,
  MOD_DIRECT_AUDIO,
  MOD_FOLLOW_AUDIO
};

#endif // CONFIG_H
