/****************************************************************
* Folow Line
* CESAR School
* 
* Line Follower PID with the microcontroller Vespa 
* from RoboCore and the Pololu's QTR-8RC sensor
****************************************************************/

#define DEBUG

#ifdef DEBUG
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
#endif

#include <RoboCore_Vespa.h>  // Library for the Vespa microcontroller
#include <QTRSensors.h>      // Library for the QTR-8A or the QTR-8RC

VespaMotors motor;         // Vespa Motor Object
QTRSensors qtr;            // QTR Sensor
BluetoothSerial SerialBT;  // Bluetooth Serial instance

// Set button and led pins
const uint8_t PIN_BUTTON = 35;
const uint8_t PIN_LED = 15;
const uint8_t PIN_MARKER_SENSOR = 36;

//Setup of the module of sensors
const uint8_t SENSOR_COUNT = 8;       // The number of sensors, which should match the length of the pins array
uint16_t sensorValues[SENSOR_COUNT];  // An array in which to store the calibrated sensor readings

// Maximum line position, considering the amount of sensors.
const long MAX_POSITION = (SENSOR_COUNT - 1) * 1000;

//Stop sensor variable
bool robotRun = false;

//Marker sensor variables
int markerCount = 10;
int markerCountNow = 0;
bool findLine = false;

// Limit value of the margin of error
const int MARGIN_ERROR = 20;

//------------------PID Control-------------------
float p = 0, i = 0, d = 0, pid = 0, error = 0, lastError = 0;

float Kp = 0.25;
float Ki = 0.0001;
float Kd = 3;

byte maxSpeed = 100;
int lSpeed, rSpeed;

const bool LINE_BLACK = false;
//-------------------------------------------------

void setup() {
  qtr.setTypeRC();  // For QTR-8RC      Sensor pins:
  qtr.setSensorPins((const uint8_t[]){ 17, 16, 18, 5, 23, 19, 22, 21 }, SENSOR_COUNT);

  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_MARKER_SENSOR, INPUT);
  pinMode(PIN_LED, OUTPUT);

#ifdef DEBUG
  Serial.begin(115200);
  delay(100);

  SerialBT.begin("Mutuca|Motoneta");  // Bluetooth device name
  Serial.println("Start BT communication");

  String btMessage;
  String prefix;

  while (prefix != "end" || digitalRead(PIN_BUTTON) == HIGH) {
    btMessage = receiveBtMessage();
    prefix = getPrefix(btMessage);

    if (prefix == "pid") {
      Kp = getNumber(btMessage, 1);
      Ki = getNumber(btMessage, 2);
      Kd = getNumber(btMessage, 3);
    } else if (prefix == "end") {
      break;
    }
  }

  delay(500);
#endif

  // Calibration
  digitalWrite(PIN_LED, HIGH);
  while (digitalRead(PIN_BUTTON) == HIGH) {  // Calibrates until the button is pressed
    qtr.calibrate();
  }
  digitalWrite(PIN_LED, LOW);

#ifdef DEBUG
  // Print the calibration minimum values measured when emitters were on
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(qtr.calibrationOn.minimum[i]);
    Serial.print(' ');
  }
  Serial.println();

  // Print the calibration maximum values measured when emitters were on
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(qtr.calibrationOn.maximum[i]);
    Serial.print(' ');
  }
  Serial.println();
#endif

  delay(2000);  // Start loop after 2 seconds
}

void loop() {
  // readSensors() returns the line position between 0 and `MAX_POSITION`.
  // error is a re-map from -1000 to 1000 range.
  error = map(readSensors(), 0, MAX_POSITION, -1000, 1000);

  // Calculate PID
  p = error;
  i = i + error;
  d = error - lastError;
  pid = (Kp * p) + (Ki * i) + (Kd * d);
  lastError = error;

  // Control Motors
  lSpeed = maxSpeed + pid;
  rSpeed = maxSpeed - pid;

  lSpeed = constrain(lSpeed, -maxSpeed, maxSpeed);
  rSpeed = constrain(rSpeed, -maxSpeed, maxSpeed);

  // If the error value is less than MARGIN_ERROR, move on
  if (error >= -MARGIN_ERROR && error <= MARGIN_ERROR) {
    motor.turn(maxSpeed, maxSpeed);
  } else {
    motor.turn(lSpeed, rSpeed);
  }

  // Count the markers and stop the robot when reach a certain number
  if (markerChecker()) {
    digitalWrite(PIN_LED, HIGH);
    motor.stop();
  }
}

int readSensors() {
  if (LINE_BLACK) {
    return qtr.readLineBlack(sensorValues);
  } else {
    return qtr.readLineWhite(sensorValues);
  }
}

/**
  Counts the number of crossed lines.

  @return `true` if the limit number of rows was detected.
*/
bool markerChecker() {
  if (analogRead(PIN_MARKER_SENSOR) < 2000 && findLine == false) {
    findLine = true;
  }

  if (analogRead(PIN_MARKER_SENSOR) >= 2000 && findLine == true) {
    findLine = false;
    markerCountNow++;
  }

  if (markerCountNow >= markerCount) {
    return true;
  }

  return false;
}

#ifdef DEBUG

/**
  Returns all stream of data sent over bluetooth until the
  button is pressed.

  @return `String` with the message sent by the bluetooth device
*/
String receiveBtMessage() {
  String message;
  char incomingChar;

  digitalWrite(PIN_LED, HIGH);
  while (digitalRead(PIN_BUTTON) == HIGH) {
    if (SerialBT.available()) {
      incomingChar = SerialBT.read();

      if (incomingChar == '\n') break;

      message += String(incomingChar);
    }
  }
  digitalWrite(PIN_LED, LOW);

  message.trim();
  return message;
}

String getPrefix(String data) {
  return getElement(data, 0);
}

double getNumber(String data, int index) {
  return atof(getElement(data, index).c_str());
}

/**
  Returns a sub-string in the `String` data, in the index
  position.

  @param `data` String with the message
  @param `index` Position of the element to be returned
  @return `String` sub-string in the indicated position. If there is
  no value at this position, it returns empty string.
*/
String getElement(String data, int index) {
  char separator = ' ';
  int found = 0;
  int startIndex = 0, endIndex = -1;
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      startIndex = endIndex + 1;
      endIndex = (i == maxIndex) ? i + 1 : i;
    }
  }

  if (found <= index) {
    return "";
  }

  return data.substring(startIndex, endIndex);
}
#endif
