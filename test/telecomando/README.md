# ESP32 BLE Interface per Magicsee R1 Controller

Questo repository contiene la documentazione e i sorgenti per interfacciare un microcontrollore **ESP32** con un telecomando Bluetooth **Magicsee R1** (spesso venduto come mini gamepad o controller VR).

Il progetto è strutturato per identificare il dispositivo tramite scansione e successivamente connettersi per leggere gli input dei tasti in tempo reale.

---

## ⚠️ ATTENZIONE: CONNESSIONE BLUETOOTH

Il punto più critico di questo progetto riguarda la gestione della connessione Bluetooth.

> **🛑 IL TELECOMANDO NON DEVE ESSERE COLLEGATO ALLO SMARTPHONE!**

Il protocollo BLE di questi dispositivi economici permette solitamente **una sola connessione attiva**. Se il Magicsee R1 è già connesso ("pairato") al tuo telefono, tablet o PC:
1.  Il dispositivo smette di fare "advertising" (non trasmette la sua presenza).
2.  L'ESP32 **non riuscirà a trovarlo** durante la scansione.
3.  L'ESP32 **non riuscirà a connettersi**.



**Procedura Corretta:**
* Vai nelle impostazioni Bluetooth del tuo telefono.
* Seleziona il Magicsee R1 e scegli **"Dissocia"**, **"Dimentica dispositivo"** o **"Unpair"**.
* Disattiva il Bluetooth del telefono durante i test per evitare riconnessioni automatiche indesiderate.

---

## 🛠 Hardware Richiesto

* **Microcontrollore:** ESP32 (qualsiasi variante con supporto BLE, es. WROOM-32).
* **Periferica:** Telecomando [Magicsee R1](https://www.google.com/search?q=magicsee+r1) o compatibile.
    * *Nota:* Il codice è testato su Magicsee R1 ma potrebbe funzionare su altri controller "VR Box" generici che utilizzano protocolli HID standard.



## 📦 Struttura del Repository

Il progetto include due script principali distinti:

### 1. Scanner BLE (`BLE_scan.ino`)
Questo script serve per la prima configurazione.
* **Funzione:** Scansiona l'ambiente circostante alla ricerca di dispositivi BLE.
* **Output:** Stampa su Monitor Seriale l'indirizzo **MAC Address**, il nome del dispositivo e la potenza del segnale (RSSI).
* **Utilizzo:** Esegui questo script per scoprire l'indirizzo MAC univoco del tuo telecomando (es. `ff:25:12:xx:xx:xx`), necessario per lo step successivo.

### 2. Client NimBLE (`BLE_telecomando.ino`)
Questo è lo script operativo principale.
* **Libreria:** Utilizza `NimBLE-Arduino` per una gestione della memoria più efficiente rispetto alla libreria standard.
* **Funzione:** Si connette all'indirizzo MAC specificato, scansiona i servizi disponibili e si iscrive alle notifiche (Subscribe).
* **Output:** Restituisce i dati grezzi (HEX) ogni volta che un tasto viene premuto o rilasciato.

---

## 🚀 Istruzioni per l'uso

1.  **Installazione Librerie:** Assicurati di aver installato la libreria `NimBLE-Arduino` tramite il Library Manager del tuo IDE.
2.  **Scansione:** Carica lo script scanner, accendi il telecomando e annota il MAC Address che appare nel monitor seriale.
3.  **Configurazione:** Apri lo script client e inserisci il MAC Address trovato nella variabile apposita all'inizio del file.
4.  **Esecuzione:** Carica lo script client. Se il telecomando è acceso e non collegato ad altri device, l'ESP32 si connetterà automaticamente e inizierà a stampare i codici dei tasti.

---

## 💡 Troubleshooting e Consigli

### Modalità di Avvio (Boot Modes)
Il Magicsee R1 può avviarsi in modalità diverse (Android, iOS, Mouse Mode) premendo combinazioni di tasti all'accensione (es. `M + B`).
* Se l'ESP32 si connette ma ricevi dati incomprensibili, prova a spegnere il telecomando e riaccenderlo con una combinazione diversa.

### Risparmio Energetico
Il telecomando entra in modalità **sleep** molto rapidamente per preservare la batteria.
* Se noti una disconnessione nel log, premi un tasto qualsiasi sul telecomando per "svegliarlo". Lo script è progettato per tentare la riconnessione automatica.

### Stabilità
Se l'ESP32 si riavvia spesso (Loop di boot):
* Verifica l'alimentazione (il Bluetooth richiede picchi di corrente).
* Assicurati di utilizzare la versione `NimBLE` dello script, poiché la libreria BLE standard di Espressif consuma molta RAM e può causare crash su progetti complessi.

---

**Licenza:** MIT
