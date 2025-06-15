#include <Servo.h>
Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

int thresholdsA0[] = { 0, 300, 550, 700, 790, 900 };
int thresholdsA1[] = { 0, 400, 600, 720, 900, 1000 };

// Array per memorizzare la sequenza di movimenti
char moveSequence[50];  // Massimo 50 movimenti
int sequenceIndex = 0;
bool isPlayingSequence = false;
int playIndex = 0;
unsigned long lastMoveTime = 0;
const unsigned long moveDuration = 1000; // Durata di ogni movimento in ms

// Per evitare ripetizioni dei tasti
bool lastBtnU = false, lastBtnR = false, lastBtnD = false, lastBtnL = false, lastBtnC = false;

void setup() {
  Serial.begin(115200);
  myservo1.attach(9);
  myservo2.attach(10);
  
  // Ferma i motori all'avvio
  myservo1.write(90);
  myservo2.write(90);
  
  Serial.println("Sistema pronto! Registra i movimenti e premi C per riprodurli");
}

void loop() {
  decodeButton(analogRead(A0), thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
  decodeButton(analogRead(A1), thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);
  
  // Se stiamo riproducendo la sequenza
  if (isPlayingSequence) {
    playSequence();
    return;
  }
  
  // Registrazione dei movimenti (solo sui fronti di salita)
  if (btnU && !lastBtnU) {
    addToSequence('U');
    Serial.println("Movimento UP aggiunto alla sequenza");
  }
  if (btnD && !lastBtnD) {
    addToSequence('D');
    Serial.println("Movimento DOWN aggiunto alla sequenza");
  }
  if (btnL && !lastBtnL) {
    addToSequence('L');
    Serial.println("Movimento LEFT aggiunto alla sequenza");
  }
  if (btnR && !lastBtnR) {
    addToSequence('R');
    Serial.println("Movimento RIGHT aggiunto alla sequenza");
  }
  
  // Avvio riproduzione sequenza
  if (btnC && !lastBtnC) {
    if (sequenceIndex > 0) {
      Serial.println("Avvio riproduzione sequenza:");
      printSequence();
      startPlayback();
    } else {
      Serial.println("Nessuna sequenza registrata!");
    }
  }
  
  // Controllo motori in tempo reale durante la registrazione
  if (btnD) {
    moveForward();
  }
  else if (btnU) {
    moveBackward();
  }
  else if (btnL) {
    moveLeft();
  }
  else if (btnR) {
    moveRight();
  }
  else {
    stopMotors();
  }
  
  // Salva lo stato precedente dei tasti
  lastBtnU = btnU;
  lastBtnR = btnR;
  lastBtnD = btnD;
  lastBtnL = btnL;
  lastBtnC = btnC;
  
  delay(50);
}

void addToSequence(char move) {
  if (sequenceIndex < 49) { // Lascia spazio per il terminatore
    moveSequence[sequenceIndex] = move;
    sequenceIndex++;
    moveSequence[sequenceIndex] = '\0'; // Terminatore stringa
  } else {
    Serial.println("Sequenza piena! Resetto...");
    resetSequence();
  }
}

void printSequence() {
  Serial.print("Sequenza registrata: ");
  for (int i = 0; i < sequenceIndex; i++) {
    Serial.print(moveSequence[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void resetSequence() {
  sequenceIndex = 0;
  moveSequence[0] = '\0';
  Serial.println("Sequenza resettata");
}

void startPlayback() {
  isPlayingSequence = true;
  playIndex = 0;
  lastMoveTime = millis();
}

void playSequence() {
  unsigned long currentTime = millis();
  
  if (playIndex >= sequenceIndex) {
    // Sequenza completata
    Serial.println("Sequenza completata!");
    isPlayingSequence = false;
    stopMotors();
    resetSequence(); // Resetta per una nuova sequenza
    return;
  }
  
  if (currentTime - lastMoveTime >= moveDuration) {
    // Esegui il movimento corrente
    char currentMove = moveSequence[playIndex];
    Serial.print("Eseguendo movimento: ");
    Serial.println(currentMove);
    
    switch (currentMove) {
      case 'U':
        moveBackward();
        break;
      case 'D':
        moveForward();
        break;
      case 'L':
        moveLeft();
        break;
      case 'R':
        moveRight();
        break;
    }
    
    playIndex++;
    lastMoveTime = currentTime;
  }
}

void moveForward() {
  Serial.println("DEBUG: Movimento AVANTI");
  myservo1.write(180);  // Motore 1 avanti
  myservo2.write(0);    // Motore 2 avanti
}

void moveBackward() {
  Serial.println("DEBUG: Movimento INDIETRO");
  myservo1.write(0);    // Motore 1 indietro
  myservo2.write(180);  // Motore 2 indietro
}

void moveLeft() {
  Serial.println("DEBUG: Movimento SINISTRA");
  myservo1.write(0);    // Motore 1 indietro
  myservo2.write(0);    // Motore 2 avanti
}

void moveRight() {
  Serial.println("DEBUG: Movimento DESTRA");
  myservo1.write(180);  // Motore 1 avanti
  myservo2.write(180);  // Motore 2 indietro
}

void stopMotors() {
  myservo1.write(90);   // Stop motore 1
  myservo2.write(90);   // Stop motore 2
}

// Funzione che decodifica il valore in base alle soglie
void decodeButton(int value, const int thresholds[], bool *level0, bool *level1, bool *level2, bool *level3, bool *level4) {
  *level0 = false;
  *level1 = false;
  *level2 = false;
  *level3 = false;
  *level4 = false;

  if (value >= thresholds[0] && value < thresholds[1]) {
    *level0 = true;
  } else if (value >= thresholds[1] && value < thresholds[2]) {
    *level1 = true;
  } else if (value >= thresholds[2] && value < thresholds[3]) {
    *level2 = true;
  } else if (value >= thresholds[3] && value < thresholds[4]) {
    *level3 = true;
  } else if (value >= thresholds[4] && value <= thresholds[5]) {
    *level4 = true;
  }
}