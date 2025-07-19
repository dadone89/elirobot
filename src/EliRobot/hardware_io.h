// hardware_io.h
// Contains declarations and implementations of functions for servo control
// and analog button management.

#ifndef HARDWARE_IO_H
#define HARDWARE_IO_H

#include <Arduino.h>     // For functions like delayMicroseconds(), Serial, etc.
#include <ESP32Servo.h>  // For the Servo class
#include <driver/adc.h>  // For ESP32 ADC functions
#include "config.h"      // For pin and threshold definitions

// Declarations of servo instances (defined in main.ino)
extern Servo myservo1;
extern Servo myservo2;

// Forward declarations of functions
int decodeButton(int value, const int thresholds[]);
int getStableAnalogRead(int pin);
int analogReadLegacy(uint8_t gpio_num);


// ========== SERVO CONTROL FUNCTIONS ==========

// Moves the robot forward.
void moveForward() {
  myservo1.write(180);  // Motor 1 forward (e.g., 180 degrees for continuous servo)
  myservo2.write(0);    // Motor 2 forward (e.g., 0 degrees for continuous servo, if inverted)
}

// Moves the robot backward.
void moveBackward() {
  myservo1.write(0);    // Motor 1 backward
  myservo2.write(180);  // Motor 2 backward
}

// Moves the robot left.
void moveLeft() {
  myservo1.write(0);  // Motor 1 backward (to turn left)
  myservo2.write(0);  // Motor 2 forward (to turn left)
}

// Moves the robot right.
void moveRight() {
  myservo1.write(180);  // Motor 1 forward (to turn right)
  myservo2.write(180);  // Motor 2 backward (to turn right)
}

// Stops both motors.
void stopMotors() {
  myservo1.write(90);  // Stop motor 1 (center position for continuous servo)
  myservo2.write(90);  // Stop motor 2
}

// ========== BUTTON MANAGEMENT FUNCTIONS ==========

// Decodes an analog value into a button ID based on thresholds.
// Returns the button index or BTN_NONE if it doesn't fall within any threshold.
int decodeButton(int value, const int thresholds[]) {
  if (value >= thresholds[0] && value < thresholds[1]) return 0;
  else if (value >= thresholds[1] && value < thresholds[2]) return 1;
  else if (value >= thresholds[2] && value < thresholds[3]) return 2;
  else if (value >= thresholds[3] && value < thresholds[4]) return 3;
  else if (value >= thresholds[4] && value <= thresholds[5]) return 4;
  else return BTN_NONE;  // No button recognized
}

// Performs multiple analog readings from a pin to get a more stable value.
// Calculates the average of 3 readings.
int getStableAnalogRead(int pin) {
  int sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogReadLegacy(pin);  // Read the analog value
    delayMicroseconds(100);        // Short delay between readings
  }
  return sum / 3;  // Return the average
}

// Implementation of the legacy analogRead function for ESP32.
// Maps specific GPIO pins to ADC channels and configures the ADC.
int analogReadLegacy(uint8_t gpio_num) {
  adc1_channel_t channel;
  adc_unit_t unit;

  // Map the GPIO pin to the corresponding ADC channel
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
    return -1;  // Unsupported pin
  }

  // Configure the ADC bit width (12 bit for 0-4095)
  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK) {
    return -1;
  }

  // Configure the ADC channel attenuation (11dB for full range)
  if (adc1_config_channel_atten(channel, ADC_ATTEN_DB_11) != ESP_OK) {
    return -1;
  }

  // Get the raw value from the ADC channel
  int raw_value = adc1_get_raw(channel);
  return raw_value;
}

#endif  // HARDWARE_IO_H