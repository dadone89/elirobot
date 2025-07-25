# ESP32wavI2S

Questo progetto dimostra come riprodurre file audio WAV tramite l'interfaccia I2S su ESP32 utilizzando Arduino.

## Descrizione

Il file `.ino` presente in questa cartella contiene il codice sorgente per la riproduzione di file audio WAV a 16 bit, mono, 8000 Hz tramite I2S. È pensato per essere utilizzato con dispositivi compatibili ESP32.

## File audio

Il file audio presente nella cartella `data` è stato scaricato dal sito [https://mauvecloud.net/sounds/](https://mauvecloud.net/sounds/) ed è in formato PCM 16 bit, mono, 8000 Hz.

## Utilizzo

1. Carica il progetto su Arduino.
2. Carica il file audio nella memoria SPIFFS tramite LittleFS (va istallato separatamente nell'ide Arduino).
3. Compila e carica il firmware sull'ESP32.
4. Collega un DAC o un amplificatore all'uscita I2S dell'ESP32 per ascoltare l'audio.

## Licenza

Consulta la licenza del file audio sul sito di origine. Il codice sorgente è fornito secondo la licenza specificata nel repository.