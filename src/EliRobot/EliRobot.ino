#include <Servo.h>
Servo myservo1;
Servo myservo2;
bool btnU, btnR, btnD, btnL, btnC = false;
bool btnUR, btnDR, btnDL, btnUL, btnDummy = false;

int thresholdsA0[] = { 0, 300, 550, 700, 790, 900 };
int thresholdsA1[] = { 0, 300, 550, 700, 790, 900 };

void setup() {
  Serial.begin(115200);
  //myservo1.attach(9);
  //myservo2.attach(10);
}
void loop() {
  int val = 10;
  //myservo1.write(val);
  //myservo2.write(val);

  Serial.print("value A0: ");
  Serial.println(analogRead(A0));

  Serial.print("value A1 : ");
  Serial.println(analogRead(A1));

  decodeButton(analogRead(A0), thresholdsA0, &btnU, &btnR, &btnD, &btnL, &btnC);
  decodeButton(analogRead(A1), thresholdsA1, &btnUR, &btnDR, &btnDL, &btnUL, &btnDummy);
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
