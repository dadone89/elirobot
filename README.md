![Development Status](https://img.shields.io/badge/Status-In%20Development-yellow)
![Version](https://img.shields.io/badge/Version-0.3.0--alpha-red)
![Build](https://img.shields.io/badge/Build-Unstable-red)

# elirobot

**elirobot** è un robottino educativo progettato da e per bambini, basato su Arduino. Il bambino può impartire comandi direttamente utilizzando la tastiera integrata nella parte superiore del robot. I comandi includono movimenti di base e funzioni speciali pensate per stimolare la creatività e l’apprendimento del coding.

## Funzionalità principali

- Interfaccia semplice e intuitiva tramite tastiera fisica
- Riproduzione di suoni pre-registrati per guidare le azioni
- Modalità:
   - Sequenza: esecuzione di sequenze di movimenti programmabili dal bambino
   - Ballo: esegue dei movimenti ripetuti per imitare un ballo
   - Telecomando: movimenti azionati direttamente dalle frecce direzionali
- Progetto open source modulare ed espandibile

## Funzionalità in sviluppo

- Funzioni speciali (ad esempio: inseguimento luce, piccoli giochi logici, etc)
- Estetica (occhi, luci varie)

## Prerequisiti

- Scheda Arduino compatibile (nello specifico ho utilizzato un ESP32DevModule a 30pin, scheda basata su ESP32)
- Componenti elettronici (motori servo a rotazione continua, pulsanti, batterie lipo, LED, buzzer, cavetti di collegamento, ecc.)
- Stampante 3D per realizzazione tasti (ma puoi realizzarla anche in altri modi più semplici)
- IDE Arduino 2.x.x e pc per la programmazione

## Installazione

1. Clona il repository:
   ```bash
   git clone https://github.com/dadone89/elirobot.git
   cd elirobot
   ```

2. Apri il file principale (`elirobot.ino`) tramite l’IDE Arduino.

3. Collega i componenti secondo lo schema presente nella cartella `docs/hardware` (o segui le istruzioni nel file `SCHEMA.md` se disponibile).

4. Carica lo sketch sulla scheda, se utilizzi ESP32 assicurati di avere prima installato dal board manager il supporto alla scheda, nel mio caso ho utilizzato ESP32 by Espressif V3.2.0.

## Utilizzo

1. Accendi/alimenta elirobot.
2. Utilizza la pulsantiera posta sulla parte superiore per impartire i comandi desiderati.
3. Sperimenta con le funzioni di movimento e le funzioni speciali.
4. Puoi modificare il codice o aggiungere nuove funzionalità per personalizzare il robot.

## Contribuire

Contributi e idee sono i benvenuti! Puoi:

- Segnalare bug o suggerire nuove funzionalità tramite le issue
- Inviare pull request per miglioramenti hardware/software
- Proporre implementazioni alternative o materiale didattico

## Licenza

Questo progetto è distribuito con una licenza Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0).  
Non è consentito l’uso commerciale del progetto o dei suoi derivati.  
Per maggiori dettagli consulta il file [`LICENSE`](LICENSE) o visita [https://creativecommons.org/licenses/by-nc/4.0/deed.it](https://creativecommons.org/licenses/by-nc/4.0/deed.it).

## Autori

- [dadone89](https://github.com/dadone89)
[danielealberti.it](https://danielealberti.it)

---

Divertiti a costruire, programmare e giocare con elirobot!
