#include <Servo.h>
Servo myservo1;
Servo myservo2;
bool button0 = 0;
bool button1 = 0;
bool button2 = 0;
bool button3 = 0;
bool button4 = 0;

void setup() {
  Serial.begin(115200);
  //myservo1.attach(9);
  //myservo2.attach(10);
}
void loop() {
  int val = 10;
  //myservo1.write(val);
  //myservo2.write(val);




  decodeButton(analogRead(A0), &button0, &button1, &button2, &button3, &button4);

  //Serial.print("A0: ");
  //Serial.println(analogRead(A0));
  //Serial.print("A1: ");
  //Serial.println(analogRead(A1));
  
  Serial.println(button0);
}

void decodeButton(int value, bool *level0, bool *level1, bool *level2, bool *level3, bool *level4) {

  *level0 = 0;
  *level1 = 0;
  *level2 = 0;
  *level3 = 0;
  *level4 = 0;

  if ((value >= 0) && (value <= 300)) {
    *level0 = true;
  } else if ((value >= 300) && (value <= 550)) {
    *level1 = true;
  } else if ((value >= 550) && (value <= 700)) {
    *level2 = true;
  } else if ((value >= 700) && (value <= 790)) {
    *level3 = true;
  } else if ((value >= 790) && (value <= 900)) {
    *level4 = true;
  }
}