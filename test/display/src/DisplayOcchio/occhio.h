// occhio.h
// Dichiarazioni per i dati dell'immagine dell'occhio

#ifndef OCCHIO_H
#define OCCHIO_H

#include <Arduino.h> // Per definizioni come uint16_t e PROGMEM
#include <pgmspace.h>  // <--- AGGIUNGI QUESTA RIGA PER PROGMEM

// Dichiarazione esterna dell'array dell'immagine
// Questo dice al compilatore che 'occhio' esiste altrove (in occhio.c)
extern const uint16_t occhio[] PROGMEM;

// Dichiarazioni delle dimensioni dell'immagine
// È buona pratica definire le dimensioni qui in modo che siano globalmente accessibili
const int image_width = 240;
const int image_height = 240;

#endif // OCCHIO_H