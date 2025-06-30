// ...existing code...
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
int ICA_min_device = 0;
int ICA_max_device = 0;
float Temp_min_device = 0.0;
float Temp_max_device = 0.0;
float humidity_min_device = 0.0;
float humidity_max_device = 0.0;
bool estado_device = false;  // Estado del dispositivo desde el servidor

String deviceId = "SoyPrueba";

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 2000;

void getRoutineDataFromApi() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://host.wokwi.internal:5000/api/v1/routine-monitoring/data-records/iot-device/" + deviceId;

    Serial.print("Haciendo GET a: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("X-API-Key", "apichakiykey"); 

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Respuesta GET (código ");
      Serial.print(httpResponseCode);
      Serial.println("):");
      String responseBody = http.getString();
      Serial.println(responseBody);
    } else {
      Serial.print("Error en GET. Código HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("No hay conexión WiFi.");
  }
}

void getDeviceInfoFromApi() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://host.wokwi.internal:5000/api/v1/health-dehumidifier/get-dehumidifier?device_id=" + deviceId;

    Serial.print("Obteniendo info del dispositivo: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("X-API-Key", "apichakiykey");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Respuesta GET dispositivo (código ");
      Serial.print(httpResponseCode);
      Serial.println("):");
      String responseBody = http.getString();
      Serial.println(responseBody);
      
      if (httpResponseCode == 200) {
        Serial.println("Información del dispositivo obtenida exitosamente");
        
        // Parsear JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, responseBody);
        
        if (!error) {
          JsonObject humidifier_info = doc["humidifier_info"];
          
          if (!humidifier_info.isNull()) {
            // Asignar valores de configuración del servidor
            ICA_min_device = humidifier_info["calidadDeAireMin"].as<int>();
            ICA_max_device = humidifier_info["calidadDeAireMax"].as<int>();
            Temp_min_device = humidifier_info["temperaturaMin"].as<float>();
            Temp_max_device = humidifier_info["temperaturaMax"].as<float>();
            humidity_min_device = humidifier_info["humedadMin"].as<float>();
            humidity_max_device = humidifier_info["humedadMax"].as<float>();
            estado_device = humidifier_info["estado"].as<bool>();
            
            Serial.println("=== Configuración del dispositivo cargada ===");
            Serial.print("ICA Min: "); Serial.println(ICA_min_device);
            Serial.print("ICA Max: "); Serial.println(ICA_max_device);
            Serial.print("Temp Min: "); Serial.println(Temp_min_device);
            Serial.print("Temp Max: "); Serial.println(Temp_max_device);
            Serial.print("Humedad Min: "); Serial.println(humidity_min_device);
            Serial.print("Humedad Max: "); Serial.println(humidity_max_device);
            Serial.print("Estado Device: "); Serial.println(estado_device ? "ACTIVO" : "INACTIVO");
            Serial.println("============================================");
          } else {
            Serial.println("No se encontró humidifier_info en la respuesta");
          }
        } else {
          Serial.print("Error parseando JSON: ");
          Serial.println(error.c_str());
        }
      } else if (httpResponseCode == 404) {
        Serial.println("Dispositivo no encontrado en el servidor");
      } else if (httpResponseCode == 500) {
        Serial.println("Error interno del servidor - continuando con operación normal");
      } else {
        Serial.println("Error desconocido del servidor");
      }
    } else {
      Serial.print("Error al obtener info del dispositivo. Código HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("No hay conexión WiFi para obtener info del dispositivo.");
  }
}

void sendToEdgeApi(float temp, float hum, int ica) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    const char* url = "http://host.wokwi.internal:5000/api/v1/health-dehumidifier/data-records";

    Serial.print("Conectando a la API en: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", "apichakiykey");

    String humidifier_info = "{\"temperature\":" + String(temp, 1) +
                             ",\"humidity\":" + String(hum, 1) +
                             ",\"ICA\":" + String(ica) + "}";

    String payload = "{\"device_id\":\"" + deviceId + "\",\"humidifier_info\":\"" + humidifier_info + "\"}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      Serial.print("Datos enviados. Código HTTP: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error al enviar datos. Código HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end(); 
  } else {
    Serial.println("No hay conexión WiFi.");
  }
}


void checkEnvironment(float temp, float hum) {
  bool tempOk = (temp >= Temp_min_device && temp <= Temp_max_device);  
  bool humOk = (hum >= humidity_min_device && hum <= humidity_max_device);

  ICA = (int)(abs(temp - 22) * 2 + abs(hum - 50) * 0.5);

  lcd.clear();
  
  if (!estado_device) {
    lcd.setCursor(0, 0);
    lcd.print("Dispositivo OFF");
    digitalWrite(ledPin, LOW);
  } else {
    if (!tempOk || !humOk) {
      lcd.setCursor(0, 0);
      lcd.print("Deshumidif. ON");
      digitalWrite(ledPin, HIGH);  
      
      if (!tempOk && !humOk) {
        Serial.println("ALERTA: Temperatura Y humedad fuera de rango");
      } else if (!tempOk) {
        Serial.print("ALERTA: Temperatura fuera de rango (");
        Serial.print(Temp_min_device); Serial.print("-"); Serial.print(Temp_max_device); Serial.println(")");
      } else if (!humOk) {
        Serial.print("ALERTA: Humedad fuera de rango (");
        Serial.print(humidity_min_device); Serial.print("-"); Serial.print(humidity_max_device); Serial.println(")");
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Ambiente OK");
      digitalWrite(ledPin, LOW);   
      Serial.println("INFO: Condiciones ambientales óptimas");
    }
  }

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(hum, 0);
  lcd.print("%");
  lcd.print("ICA:");
  lcd.print(ICA);
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

    delay(10000); 
    sendToEdgeApi(temperature, humidity, ICA);
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

  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
  
  getDeviceInfoFromApi();
  getRoutineDataFromApi();
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    lastUpdate = now;
    updateSensorData();
  }
}