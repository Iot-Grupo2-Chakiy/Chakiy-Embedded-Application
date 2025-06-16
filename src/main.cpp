// ...existing code...
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <HTTPClient.h>

// DHT22
#define DHTPIN 33
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// LED
const int ledPin = 32;

// Variables
float temperature;
float humidity;
int ICA = 42;     
int ICA_min = 999;  
int ICA_max = 0;    

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 2000;

void sendToEdgeApi(float temp, float hum, int ica, int ica_min) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    const char* url = "http://host.wokwi.internal:5000/api/v1/health-monitoring/data-records";

    Serial.print("Conectando a la API en: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", "apichakiykey");

    String device_id = "dehumidifier_chakiy_001";
    String humidifier_info = "{\"temperature\":" + String(temp, 1) +
                             ",\"humidity\":" + String(hum, 1) +
                             ",\"ICA\":" + String(ica) +
                             ",\"ICA_min\":" + String(ica_min) + "}";

    String payload = "{\"device_id\":\"" + device_id + "\",\"humidifier_info\":\"" + humidifier_info + "\"}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      Serial.print("Datos enviados. Código HTTP: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error al enviar datos. Código HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end(); 

    HTTPClient httpGet;
    httpGet.begin(url);
    httpGet.addHeader("X-API-Key", "apichakiykey");

    int getResponseCode = httpGet.GET();

    if (getResponseCode > 0) {
      Serial.print("GET realizado. Código HTTP: ");
      Serial.println(getResponseCode);
      String responseBody = httpGet.getString();
      Serial.println("Respuesta del servidor:");
      Serial.println(responseBody);
    } else {
      Serial.print("Error en GET. Código HTTP: ");
      Serial.println(getResponseCode);
    }

    httpGet.end(); 
  } else {
    Serial.println("No hay conexión WiFi.");
  }
}

void checkEnvironment(float temp, float hum) {
  bool tempOk = temp <= 21.6;  // Se considera seguro si es menor o igual
  bool humOk = hum < 30 || hum > 50 ? false : true;

  // ICA dinámico (puedes ajustar la fórmula)
  ICA = (int)(abs(temp - 22) * 2 + abs(hum - 50) * 0.5);

  // Actualizar ICA mínimo y máximo
  if (ICA < ICA_min) ICA_min = ICA;
  if (ICA > ICA_max) ICA_max = ICA;

  lcd.clear();

  if (temp > 21.6 && hum > 50) {
    lcd.setCursor(0, 0);
    lcd.print("Inseguro");
    digitalWrite(ledPin, LOW);
  } else if (!tempOk && humOk) {
    lcd.setCursor(0, 0);
    lcd.print("Inseguro");
    digitalWrite(ledPin, LOW);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Seguro");
    digitalWrite(ledPin, HIGH);
  }

  // Mostrar ICA y mínimo
  lcd.setCursor(0, 1);
  lcd.print("ICA:");
  lcd.print(ICA);
  lcd.print(" M:");
  lcd.print(ICA_min);
}

void updateSensorData() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print(" °C | Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    checkEnvironment(temperature, humidity);

    sendToEdgeApi(temperature, humidity, ICA, ICA_min);
  } else {
    Serial.println("Error leyendo del DHT22");
  }
}

void setup() {
  Serial.begin(9600);
  WiFi.begin("Wokwi-GUEST", "");

  dht.begin();
  lcd.init();
  lcd.backlight();

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Esperar conexión WiFi
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    lastUpdate = now;
    updateSensorData();
  }
}
// ...existing code...