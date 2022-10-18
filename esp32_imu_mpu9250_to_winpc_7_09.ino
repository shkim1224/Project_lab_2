/**
 * ESP32 Accelerometer IoT Logger
 * Author: Sungho Kim
 * Date: 7 9, 2022
 * Check server for flag and then start recording raw accelerometer (x, y, z)
 * measurements for 1 second. When done, transmit them to server in JSON format
 * as an HTTP POST request.
 */

#include <WiFi.h>   // wifi와 ble를 사용할 경우에는 #include <WiFiNINA.h>를 include 해야 함
#include <Wire.h>   // i2c 통신을 사용할 경우
#include <HttpClient.h>  // wifi에서 mqtt를 사용하여 data를 전송할 경우 사용 
#include <ArduinoJson.h>  // Json 파일 전송을 위함
#include <MPU9250_asukiaaa.h>  // mpu9250 사용하기 위한 라이브러리
#ifdef _ESP32_HAL_I2C_H_
#define SDA_PIN 21  // gpio pin 21
#define SCL_PIN 22  // gpio pin 22
#endif
MPU9250_asukiaaa mySensor;

// WiFi credentials
char *ssid = "iptime_shkim_24";
char *password = "ksh89377";
// Server, file, and port
const char *hostname = "192.168.0.6";  // my windows pc ip address
const String uri = "/";
const int port = 1337;

// Settings
const float collection_period = 1.0;   // 매 1초 마다 3축 가속도 센서값 측정(sec)
const int sample_rate = 128;           // 1초마다 128개의 데이터 측정 (Hz)
const int num_dec = 7;                 // Number of decimal places
const int num_samples = (int)(collection_period * sample_rate); // 초당 128개의 가속도 센서값을 센싱함
const unsigned long timeout = 500;     // Time to wait for server response

// Expected JSON size. Use the following to calculate values:
// https://arduinojson.org/v6/assistant/
const size_t json_capacity = (3 * JSON_ARRAY_SIZE(num_samples)) + 
                            JSON_OBJECT_SIZE(3);  // 원격의 라파에서 numpy array data로 바로 사용할 수 있도록 json 파일 형태로 보냄
// Globals
WiFiClient client;
/*******************************************************************************
* Functions
*/
// Send GET request to server to get "ready" flag => 서버로부터 ready flag를 받기 위해 우선 GET request를 보냄
int getServerReadyFlag(unsigned long timeout) {
  int ret_status = -1;
  unsigned long timestamp;
  // Make sure we're connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connecting to: ");
    Serial.println(hostname);
    if (client.connect(hostname, port)) {
       Serial.println("Sending GET request");
       client.print("GET " + uri + " HTTP/1.1\r\n" +   
                "Host: " + hostname + "\r\n" +
                "Connection: close\r\n\r\n");   // 원격의 내 windows pc에 GET request 를 보냄
 // Wait for up to specified time for response from server
      timestamp = millis();
      while (!client.available()) {
        if (millis() > timestamp + timeout) {
          Serial.println("GET response timeout");
          return -1;
        }
      }
      // Header should take up 4 lines, so throw them away
      for (int i = 0; i < 4; i++) {
        if (client.available()) {
          String resp = client.readStringUntil('\r');
        } else {
          return -1;
        }
      }

      // Response from server should be only a 0 or 1
      if (client.available()) {
        String resp = client.readStringUntil('\r');
        resp.trim();
        Serial.print("Server response: ");
        Serial.println(resp);
        if (resp == "0") {
          ret_status = 0;
        } else if (resp == "1") {
          ret_status = 1;
        }
      } else {
        return -1;
      }
    }

    // Close TCP connection
    client.stop();
    Serial.println();
    Serial.println("Connection closed");
  }
  return ret_status;
}

int sendPostRequest(DynamicJsonDocument json, unsigned long timeout) {
  unsigned long timestamp;
  // Connect to server
  Serial.print("Connecting to ");
  Serial.println(hostname);
  if (!client.connect(hostname, port)) {
    Serial.println("Connection failed");
    return 0;
  }
  // Send HTTP POST request
  Serial.println("Sending POST request");
  client.print("POST " + uri + " HTTP/1.1\r\n" +
               "Host: " + hostname + "\r\n" +
               "Connection: close\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + measureJson(json) + "\r\n" +
               "\r\n");
  // Send JSON data
  serializeJson(json, client);
  // Wait for up to specified time for response from server
  timestamp = millis();
  while (!client.available()) {
    if (millis() > timestamp + timeout) {
      Serial.println("GET response timeout");
      return -1;
    }
  }

  // Print response
  while (client.available() ) {
    String ln = client.readStringUntil('\r');
    Serial.print(ln);
  }
  Serial.println();
  // Close TCP connection
  client.stop();
  Serial.println();
  Serial.println("Connection closed");
  return 1;
}

/*******************************************************************************
 * Main
 */
void setup() {
  // Initialize Serial port for debugging
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN); //sda, scl
  Wire.begin();
  mySensor.setWire(&Wire);
  mySensor.beginAccel();
  mySensor.beginMag();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
 }
  // Print connection details
  Serial.println();
  Serial.println("Connected to the WiFi network");
  Serial.print("My IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  static unsigned long timestamp = millis();
  static unsigned long prev_timestamp = timestamp;
  // Check to see if server is ready to accept a new sample
  int resp_status = getServerReadyFlag(timeout);
  if (resp_status != 1) {
    Serial.println("Server is not accepting new samples");
    return;
  }
  // Create JSON document
  DynamicJsonDocument json(json_capacity);
  JsonArray json_x = json.createNestedArray("x");
  JsonArray json_y = json.createNestedArray("y");
  JsonArray json_z = json.createNestedArray("z");

  // 주어진 1초 마다 128개의 3축 가속도 값을 측정함
  int i = 0;
  while (i < num_samples) { // num_samples==128
     if (millis() >= timestamp + (1000 / sample_rate)) { //sample_rate = 128; => 7.8msec 마다 3축 가속도센서값 측정
      // Update timestamps to maintain sample rate
      prev_timestamp = timestamp;
      timestamp = millis();
      mySensor.accelUpdate();
      Serial.print("Step=");
      Serial.print(i);
      Serial.print(" X: ");
      Serial.print(mySensor.accelX());
      Serial.print(" Y: ");
      Serial.print(mySensor.accelY());
      Serial.print(" Z: ");
      Serial.println(mySensor.accelZ());
      // Add x, y, and z measurements to JSON doc
      json_x.add(mySensor.accelX());
      json_y.add(mySensor.accelY());
      json_z.add(mySensor.accelZ());
      // Update sample counter
      i++;
    }
  }

  // Send JSON to server via POST request
  serializeJson(json, Serial);
  Serial.println();
  sendPostRequest(json, timeout);
}
