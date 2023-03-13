// ------------------------- INCLUDES -------------------------
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DHT.h>

// ------------------------- DEFINES --------------------------
#define DHT_PIN 12
#define DHT_TYPE DHT11
// How often temp and humidity values are refreshed in ms
#define DHT_SAMPLE_RATE 60000
#define WEB_SERVER_PORT 80
// ESP8266 maximum EEPROM size is 4096 bytes (4 KB)
#define EEPROM_SIZE 512
// Credential reset button
#define CRED_RES_PIN 14

// ------------------------- GLOBALS --------------------------
boolean is_configuration_mode = false;
// DHT variables
unsigned long previousMillis = 0;
float temp = -1;
float humidity = -1;
// Init objects
DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server(WEB_SERVER_PORT);

// ------------------------- DECLARATIONS ---------------------
void reset_stored_credentials();
void web_api_configuration_hotspot();
void web_api_sensor_data();
void check_credentials_button();

// ------------------------- SETUP ----------------------------
void setup() {
  Serial.begin(115200);
  pinMode(CRED_RES_PIN, INPUT);

  // EEPROM init
  EEPROM.begin(EEPROM_SIZE);
  delay(500);

  // Load SSID (max length 32 characters by standard)
  String esid = "";
  for (int i = 0; i < 32; i++) {
    esid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(esid);

  // Load PASS (max length 63 characters by standard)
  String epass = "";
  for (int i = 32; i < 96; i++) {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);

  String trim_sid = esid;
  trim_sid.trim();
  String trim_pass = epass;
  trim_pass.trim();
  if (trim_sid.length() == 0 && trim_pass.length() == 0) {
    Serial.println("Starting device configuration hotspot");
    is_configuration_mode = true;

    boolean result = WiFi.softAP("Weather Station", "12345678");
    Serial.print("Hotspot start status: ");
    Serial.println(result);

    web_api_configuration_hotspot();
    server.begin();
  }
  else {
    is_configuration_mode = false;
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(esid);
    WiFi.begin(esid, epass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(50);
      check_credentials_button();
      Serial.print(".");
    }
    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Start server and sensor
    web_api_sensor_data();
    dht.begin();
    // Set next sample after 2s, for sensor to warm up
    previousMillis = millis() - DHT_SAMPLE_RATE + 2000;
    server.begin();    
  }
}

// ------------------------- LOOP ----------------------------
void loop() {
  // Credentials reset button
  check_credentials_button();

  // HTTP server handle
  server.handleClient();

  if (!is_configuration_mode) {
    // Do sensor measurement 
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= DHT_SAMPLE_RATE) {
      previousMillis = currentMillis;
      humidity = dht.readHumidity();
      temp = dht.readTemperature();
      if (isnan(humidity) || isnan(temp)) {
        Serial.println("Failed to read from DHT sensor!");
      }
    }
  }

}

// ------------------------- DEFINITIONS ------------------------
void check_credentials_button() {
  int reset_credentials_val = digitalRead(CRED_RES_PIN);
  if (!reset_credentials_val) {
    reset_stored_credentials();
  }
}

void reset_stored_credentials() {
  Serial.println("Reset stored credentials");
  // Reset SSID (max length 32 characters by standard)
  for (int i = 0; i < 32; i++) {
    EEPROM.write(i, ' ');
  }
  // Reset PASS (max length 63 characters by standard)
  for (int i = 32; i < 96; i++) {
    EEPROM.write(i, ' ');
  }
  EEPROM.commit();
  
  Serial.println("Reset device");
  ESP.reset();
}

void web_api_configuration_hotspot() {
  server.on("/", []() {
    String content = "<!DOCTYPE HTML>";
    content += "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Weather WiFi Station</title></head>";
    content += "<body style='background-color:#6B5B95;color:#ffffff;'>";
    content += "<div style='margin-left: 25%;width:50%;'><h1>Weather WiFi Station</h1></div>";
    content += "<form method='get' action='setting' style='margin-left: 25%;width:50%'>";
    content += "<div style='margin-bottom:1%;'><label>WiFi</label><br/><input name='ssid' length=32 style='width:100%;'></div>";
    content += "<div style='margin-bottom:1%;'><label>Pass</label><br/><input type='password' name='pass' length=64 style='width:100%;'></div>";
    content += "<div style='margin-bottom:1%;'><input type='submit' value='Connect' style=''></div>";
    content += "</form><div style='margin-left: 25%;width:50%;margin-top:10%'>MrLaki5</div>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  });

  server.on("/setting", []() {
    String qsid = server.arg("ssid");
    String qpass = server.arg("pass");
    int statusCode = 400;
    String content = "{\"Error\":\"404 not found\"}";
    if (qsid.length() > 0 && qpass.length() > 0) {
      // Clearing EEPROM
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      
      // Writting SSID and PASS
      for (int i = 0; i < qsid.length(); i++) {
        EEPROM.write(i, qsid[i]);
      }
      for (int i = 0; i < qpass.length(); i++) {
        EEPROM.write(32 + i, qpass[i]);
      }
      EEPROM.commit();

      content = "{\"Success\":\"Saved ssid and password\"}";
      statusCode = 200;
      ESP.reset();
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(statusCode, "application/json", content);
  });
}

void web_api_sensor_data() {
  server.on("/", []() {
    String content = "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Weather WiFi Station</title></head>";
    content += "<body style='background-color:#6B5B95;color:#ffffff;'>";
    content += "<div style='margin-left: 25%;width:50%;'><h1>Weather WiFi Station</h1></div>";
    content += "<div style='margin-left: 25%;width:50%;'><h3>Temperature: " + String(temp) + "C</h3></div>";
    content += "<div style='margin-left: 25%;width:50%;'><h3>Humidity: " + String(humidity) + "%</h3></div>";
    content += "<div style='margin-left: 25%;width:50%;margin-top:10%'>MrLaki5</div>";
    content += "</body></html>";
    server.send(200, "text/html", content);
  });
}
