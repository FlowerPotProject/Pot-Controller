#include <ArduinoJson.h>
#include <DHT11.h>
#include <SoftwareSerial.h>
SoftwareSerial Zigbee(2,3); // RX:2, TX:3

#define ID 1
#define ERR_RANGE 10
#define PUMP 8
#define LED 9
#define DHT_SENSOR A0 
#define SOIL_SENSOR A1
#define WAIT_TIME 60000


DHT11 dht11(DHT_SENSOR);

// control variable
unsigned long C_M_001_sec;      // water supply operation time (time)
unsigned long C_M_002_ms;       // water supply operation time (flux)
unsigned long C_M_002_flux;     // water supply operation flux (flux → time) 
int C_M_001_ing = 0;            // C_M_001 operation state (0: OFF, 1: ON)
int C_M_002_ing = 0;            // C_M_002 operation state (0: OFF, 1: ON)
int LED_ing = 0;                // LED operation state (0: OFF, 1: ON)
int auto_control_ing = 0;       // auto control state (0: OFF, 1: ON)
int auto_control_on = 0;        // auto control state (0: ready, 1: done)
int auto_control_off = 0;       // auto control state (0: ready, 1: done)
int auto_control_wait = 0;      // auto control state (0: ready, 1: wait)
int set_humi;                   // humidity set value (auto control)

// time variable
unsigned long time_current = 0;
unsigned long time_previous = 0;
unsigned long time_previous_001 = 0;  // C_M_001 start time check
unsigned long time_previous_002 = 0;  // C_M_002 start time check
unsigned long time_previous_wait = 0; // Auto control wait time check

// sensor variable
float temp, humi;
int soil_humi;

void setup() {
  Serial.begin(9600);
  Zigbee.begin(9600);

  pinMode(PUMP, OUTPUT);
  pinMode(LED, OUTPUT);
  
  Serial.println("POT Controller Start...");
}

// data from Soil Sensor, DHT
void read_sensor() {
  int i = analogRead(SOIL_SENSOR);
  soil_humi = map(i, 0, 800, 0, 100);
//  Serial.println("-----Sensor Data-----");
//  Serial.print("Soil Humidity: ");
//  Serial.print(soil_humi);
//  Serial.println("%");
    
  if(dht11.read(humi, temp) == 0) {
//    Serial.print("Temperature: ");
//    Serial.print(temp);
//    Serial.println("°C");
//    Serial.print("Humidity: ");
//    Serial.print(humi);
//    Serial.println("%");
  }
  else {
//    Serial.println("Can't read DHT");
  }
//  Serial.println("---------------------");
}

// JSON size
StaticJsonDocument<200> doc;
StaticJsonDocument<100> doc_2;
StaticJsonDocument<100> doc_3;

// data to Zigbee
void send_sensorData() {
  JsonObject root = doc_2.to<JsonObject>();
  JsonObject sensorDataSet = root.createNestedObject("sensorDataSet");
  sensorDataSet["temp"] = temp;
  sensorDataSet["humi"] = humi;
  sensorDataSet["soil_humi"] = soil_humi;

  serializeJsonPretty(doc_2, Serial);
  Serial.write('#');

//  Serial.println("---Send Sensor Data--");
//  Serial.print("Temperature: ");
//  Serial.println(temp);
//  Serial.print("Humidity: ");
//  Serial.println(humi);
//  Serial.print("Soil Humidity: ");
//  Serial.println(soil_humi);
//  Serial.println("---------------------");
}

void send_stateData() {
  int i = 0;
  JsonObject root2 = doc_3.to<JsonObject>();
  JsonObject stateDataSet = root2.createNestedObject("stateDataSet");
  stateDataSet["auto"] = auto_control_ing;
  if(C_M_001_ing == 1 || C_M_002_ing == 1) {
    stateDataSet["water"] = 1;
    i = 1;
  }
  else {
    stateDataSet["water"] = 0;
    i = 0;
  }
  stateDataSet["LED"] = LED_ing;

  serializeJsonPretty(doc_3, Serial);
  Serial.write('#');

  Serial.println("---Send state Data---");
  Serial.print("Auto control: ");
  Serial.println(auto_control_ing);
  Serial.print("Water Supply: ");
  Serial.println(i);
  Serial.print("LED: ");
  Serial.println(LED_ing);
  Serial.println("---------------------");
}

// message format: JSON
void process_message() {
  String buffer;
  while(Serial.available()){ // read just 1 line;
    char data = Serial.read();
    if(data == '\n') {
      break;
    }
    else {
      buffer = buffer + data;
    }
    delay(10);
  }
  Serial.println("---Got from Zigbee---");
  Serial.println(buffer);
  Serial.println("---------------------");

  deserializeJson(doc, buffer);

  // ID CHECK
  if(doc["id"] == ID) {
    if(doc["requestData"] == 1) { // send sensor data to Zigbee
      if(temp != 0) {
        send_sensorData();
      }
    }
    else if(doc["code"] == "C_S_001") { // Auto control ON
      set_humi = doc["set_humi"];
      auto_control_ing = 1;
      auto_control_wait = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("Auto control ON");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_S_002") { // Auto control OFF
      digitalWrite(PUMP, LOW);
      auto_control_ing = 0;
      auto_control_wait = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("Auto control OFF");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_M_001") { // water supply (time)
      if(auto_control_ing == 0) {
        C_M_001_sec = doc["time"];
        digitalWrite(PUMP, HIGH);
        time_previous_001 = millis();
        C_M_001_ing = 1;
        send_stateData();
        Serial.println("----Control Panel----");
        Serial.println("Water supply ON");
        Serial.print("Time: ");
        Serial.print(C_M_001_sec);
        Serial.println("s");
        Serial.println("---------------------");
      }
    }
    else if(doc["code"] == "C_M_002") { // water supply (flux)
      if(auto_control_ing == 0) {
        C_M_002_flux = doc["flux"];
        C_M_002_ms = C_M_002_flux*50; // Convert flux to time (1ml = 50ms)
        digitalWrite(PUMP, HIGH);
        time_previous_002 = millis();
        C_M_002_ing = 1;
        send_stateData();
        Serial.println("----Control Panel----");
        Serial.println("Water supply ON");
        Serial.print("Flux: ");
        Serial.print(C_M_002_flux);
        Serial.println("mL");
        Serial.println("---------------------");
      }
    }
    else if(doc["code"] == "C_M_003") { // LED ON
      digitalWrite(LED, HIGH);
      LED_ing = 1;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.print("LED ON");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_M_004") { // LED OFF
      digitalWrite(LED, LOW);
      LED_ing = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.print("LED OFF");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_M_005") { // Stop working
      digitalWrite(PUMP, LOW);
      C_M_001_ing = 0;
      C_M_002_ing = 0;
      auto_control_ing = 0;
      auto_control_wait = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("Emergency Stop");
      Serial.println("Auto control OFF");
      Serial.println("Stop water supply");
      Serial.println("---------------------");
    }
    else {
      Serial.println("This code doesn't exist");
    }
  }
}


void loop() {
  // data from Zigbee
  if(Serial.available()){
     process_message();
  }
  
  // Read sensor every 2 seconds
  time_current = millis();
  if(time_current - time_previous >= 2000) {
    time_previous = time_current;
    read_sensor();
  }

  // Auto control
  if(auto_control_ing == 1) {
    if(set_humi > soil_humi+ERR_RANGE) {
      if(auto_control_wait == 0) {
        digitalWrite(PUMP, HIGH);
        if(auto_control_on == 0) {
          Serial.println("----Control Panel----");
          Serial.println("Auto water supply ON");
          Serial.println("---------------------");
          auto_control_off = 0;
        }
        auto_control_on = 1;
      }
    }
    else {
      if(auto_control_wait == 0) {
        digitalWrite(PUMP, LOW);
        auto_control_wait = 1;
        time_previous_wait = millis();
        if(auto_control_off == 0) {
          Serial.println("----Control Panel----");
          Serial.println("Auto water supply OFF");
          Serial.println("---------------------");
          auto_control_on = 0;
        }
        auto_control_off = 1;
      }
    }
    if(auto_control_wait == 1) {
      if(time_current - time_previous_wait >= WAIT_TIME) {
        time_previous_wait = time_current;
        auto_control_wait = 0;
      }
    }
  }
  
  // Stop working
  if(C_M_001_ing == 1) {
    if(time_current - time_previous_001 >= C_M_001_sec*1000) {
      digitalWrite(PUMP, LOW);
      C_M_001_ing = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("water supply OFF");
      Serial.print("Operation time: ");
      Serial.print((time_current - time_previous_001) / 1000);
      Serial.println("s");
      Serial.println("---------------------");
    }
  }
  if(C_M_002_ing == 1) {
    if(time_current - time_previous_002 >= C_M_002_ms) {
      digitalWrite(PUMP, LOW);
      C_M_002_ing = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("Stop water supply OFF");
      Serial.print("Operation flux: ");
      Serial.print(C_M_002_flux);
      Serial.println("mL");
      Serial.println("---------------------");
    }
  }
}
