#include <MPU9250_asukiaaa.h>
#include <Wire.h>
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 21  // gpio pin 21  //I2C 통신포트를 사용하여 MPU9250 센서가 연결됨
#define SCL_PIN 22  // gpio pin 22
#endif
MPU9250_asukiaaa mySensor;  // MPU9250_asukiaaa.h에 기술된 클래스로부터 객체를 하나 생성
void setup() {
  Serial.begin(115200);
  Serial.println("started");
  Wire.begin(SDA_PIN, SCL_PIN); //sda, scl   // I2C통신을 사용하기 위해서는 Wire.h 를 include 해야 함
//  Wire.begin();
  mySensor.setWire(&Wire);
  mySensor.beginAccel();
//  mySensor.beginMag();
  // you can set your own offset for mag values
  // mySensor.magXOffset = -50;
  // mySensor.magYOffset = -55;
  // mySensor.magZOffset = -10;
}
void loop() {
  mySensor.accelUpdate();
//  Serial.println("print accel values");
  Serial.print("accelX: " + String(mySensor.accelX()));
  Serial.print(" accelY: " + String(mySensor.accelY()));
  Serial.println(" accelZ: " + String(mySensor.accelZ()));
//  Serial.println("accelSqrt: " + String(mySensor.accelSqrt()));
//  mySensor.magUpdate();
//  Serial.println("print mag values");
//  Serial.println("magX: " + String(mySensor.magX()));
//  Serial.println("maxY: " + String(mySensor.magY()));
//  Serial.println("magZ: " + String(mySensor.magZ()));
//  Serial.println("horizontal direction: " + String(mySensor.magHorizDirection()));
//  Serial.println("at " + String(millis()) + "ms");
  delay(1);
}
