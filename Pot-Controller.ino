#include <ArduinoJson.h>
#include <DHT11.h>

#define POT_ID 1
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
  Serial3.begin(9600);

  pinMode(PUMP, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(PUMP, HIGH);
  digitalWrite(LED, HIGH);
  
  Serial.println("POT Controller Start...");
}

// data from Soil Sensor, DHT
void read_sensor() {
  int i = analogRead(SOIL_SENSOR);
  soil_humi = map(i, 0, 950, 0, 100);
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

// data to Serial3
void send_sensorData() {
  JsonObject root = doc_2.to<JsonObject>();
  JsonObject sensorDataSet = root.createNestedObject("sensorDataSet");
  sensorDataSet["pot_id"] = POT_ID;
  sensorDataSet["temp"] = temp;
  sensorDataSet["humi"] = humi;
  sensorDataSet["soil_humi"] = soil_humi;

  serializeJsonPretty(doc_2, Serial3);
  Serial3.write('#');

  Serial.println("---Send Sensor Data--");
  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(humi);
  Serial.print("Soil Humidity: ");
  Serial.println(soil_humi);
  Serial.println("---------------------");
}

void send_stateData() {
  bool isAuto, isWater, isLed;
  if(auto_control_ing == 1) {
    isAuto = true;
  } 
  else {
    isAuto = false;
  }
  if(C_M_001_ing == 1 || C_M_002_ing == 1) {
    isWater = true;
  }
  else {
    isWater = false;
  }
  if(LED_ing == 1) {
    isLed = true;
  }
  else {
    isLed = false;
  }
  
  JsonObject root2 = doc_3.to<JsonObject>();
  JsonObject stateDataSet = root2.createNestedObject("stateDataSet");
  stateDataSet["pot_id"] = POT_ID;
  stateDataSet["is_auto"] = isAuto;
  stateDataSet["is_water"] = isWater;
  stateDataSet["is_led"] = isLed;

  serializeJsonPretty(doc_3, Serial3);
  Serial3.write('#');

  Serial.println("---Send state Data---");
  Serial.print("Auto control: ");
  Serial.println(isAuto);
  Serial.print("Water Supply: ");
  Serial.println(isWater);
  Serial.print("LED: ");
  Serial.println(isLed);
  Serial.println("---------------------");
}

// message format: JSON
void process_message() {
  String buffer;
  while(Serial3.available()){ // read just 1 line;
    char data = Serial3.read();
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

  if(doc["stateData"] == 1) { // send state data to Zigbee
    send_stateData();
  }
  // ID CHECK
  if(doc["potId"] == POT_ID) {
    if(doc["sensorData"] == 1) { // send sensor data to Zigbee
      if(temp != NULL) {
        send_sensorData();
      }
    }
    else if(doc["code"] == "C_S_001") { // Auto control ON
      set_humi = doc["setHumi"];
      auto_control_ing = 1;
      auto_control_wait = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("Auto control ON");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_S_002") { // Auto control OFF
      digitalWrite(PUMP, HIGH);
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
        digitalWrite(PUMP, LOW);
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
        digitalWrite(PUMP, LOW);
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
      digitalWrite(LED, LOW);
      LED_ing = 1;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("LED ON");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_M_004") { // LED OFF
      digitalWrite(LED, HIGH);
      LED_ing = 0;
      send_stateData();
      Serial.println("----Control Panel----");
      Serial.println("LED OFF");
      Serial.println("---------------------");
    }
    else if(doc["code"] == "C_M_005") { // Stop working
      digitalWrite(PUMP, HIGH);
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
  else {
    Serial.println("ID doesn't match");
  }
}


void loop() {
  // data from Zigbee
  if(Serial3.available()){
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
        digitalWrite(PUMP, LOW);
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
        digitalWrite(PUMP, HIGH);
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
      digitalWrite(PUMP, HIGH);
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
      digitalWrite(PUMP, HIGH);
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
