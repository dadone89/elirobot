#include <Servo.h>
Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

int thresholdsA0[] = { 0, 300, 550, 700, 790, 900 };
int thresholdsA1[] = { 0, 400, 600, 720, 900, 1000 };

void setup() {
  Serial.begin(115200);
  //myservo1.attach(9);
  //myservo2.attach(10);
}
void loop() {
  int val = 10;
  //myservo1.write(val);
  //myservo2.write(val);

  //Serial.print("value A0: ");
  //Serial.println(analogRead(A0));

  //Serial.print("value A1 : ");
  //Serial.println(analogRead(A1));

  decodeButton(analogRead(A0), thresholdsA0, &btnC, &btnDL, &btnUL, &btnUR, &btnDR);
  decodeButton(analogRead(A1), thresholdsA1, &btnL, &btnU, &btnR, &btnD, &btnDummy);
  
  // Debug messages per A0 (primo joystick/gruppo di tasti)
  if (btnU) {
    Serial.println("DEBUG A0: Tasto UP premuto");
  }
  if (btnR) {
    Serial.println("DEBUG A0: Tasto RIGHT premuto");
  }
  if (btnD) {
    Serial.println("DEBUG A0: Tasto DOWN premuto");
  }
  if (btnL) {
    Serial.println("DEBUG A0: Tasto LEFT premuto");
  }
  if (btnC) {
    Serial.println("DEBUG A0: Tasto CENTER premuto");
  }
  
  // Debug messages per A1 (secondo joystick/gruppo di tasti)
  if (btnUR) {
    Serial.println("DEBUG A1: Tasto UP-RIGHT premuto");
  }
  if (btnDR) {
    Serial.println("DEBUG A1: Tasto DOWN-RIGHT premuto");
  }
  if (btnDL) {
    Serial.println("DEBUG A1: Tasto DOWN-LEFT premuto");
  }
  if (btnUL) {
    Serial.println("DEBUG A1: Tasto UP-LEFT premuto");
  }
  if (btnDummy) {
    Serial.println("DEBUG A1: Tasto DUMMY premuto");
  }
  
  delay(100); // Piccola pausa per evitare spam nel monitor seriale

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
