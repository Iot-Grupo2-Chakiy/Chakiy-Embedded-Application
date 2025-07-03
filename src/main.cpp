#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define DHTPIN 33
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int ledPin = 32;

float temperature;
float humidity;
int ICA = 42;     
int ICA_min_device = 0;
int ICA_max_device = 0;
float Temp_min_device = 0.0;
float Temp_max_device = 0.0;
float humidity_min_device = 0.0;
float humidity_max_device = 0.0;
bool estado_device = false;  
bool estado_device_original = false; 
String active_device_type = "";

String deviceId = "SoyPrueba";

unsigned long lastSensorUpdate = 0;
unsigned long lastApiUpdate = 0;
unsigned long lastRoutineCheck = 0;
const unsigned long sensorUpdateInterval = 5000;   
const unsigned long apiUpdateInterval = 30000;      
const unsigned long routineCheckInterval = 30000;  

struct Routine {
  int id;
  String name;
  String condition;
  String days[7];
  int dayCount;
  String startTime;
  String endTime;
  String ubication;
  bool isDry;
  bool isActive;
};

const int MAX_ROUTINES = 100;
Routine routines[MAX_ROUTINES];
int routineCount = 0;


String getCurrentDay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "UNKNOWN";
  }
  
  String days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  return days[timeinfo.tm_wday];
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00:00";
  }
  
  char timeStr[6];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  return String(timeStr);
}

bool isDayInRoutine(const Routine& routine, const String& currentDay) {
  for (int i = 0; i < routine.dayCount; i++) {
    if (routine.days[i] == currentDay) {
      return true;
    }
  }
  return false;
}

bool isTimeInRange(const String& currentTime, const String& startTime, const String& endTime) {
  int currentMinutes = currentTime.substring(0, 2).toInt() * 60 + currentTime.substring(3, 5).toInt();
  int startMinutes = startTime.substring(0, 2).toInt() * 60 + startTime.substring(3, 5).toInt();
  int endMinutes = endTime.substring(0, 2).toInt() * 60 + endTime.substring(3, 5).toInt();
  
  if (startMinutes <= endMinutes) {
    return (currentMinutes >= startMinutes && currentMinutes <= endMinutes);
  } else {
    return (currentMinutes >= startMinutes || currentMinutes <= endMinutes);
  }
}


void checkActiveRoutines() {
  String currentDay = getCurrentDay();
  String currentTime = getCurrentTime();
  
  Serial.println("");
  Serial.println("=== VERIFICANDO RUTINAS ===");
  Serial.print("Dia actual: "); Serial.println(currentDay);
  Serial.print("Hora actual: "); Serial.println(currentTime);
  Serial.print("Humedad actual: "); Serial.print(humidity); Serial.println("%");
  Serial.print("Total rutinas cargadas: "); Serial.println(routineCount);
  
  bool routineActive = false;
  String activeRoutineName = "";
  String deviceType = "";
  
  for (int i = 0; i < routineCount; i++) {
    Serial.print("Verificando rutina #"); Serial.print(i + 1); 
    Serial.print(": "); Serial.println(routines[i].name);
    
    Serial.print("   Dias configurados: ");
    for (int d = 0; d < routines[i].dayCount; d++) {
      Serial.print(routines[i].days[d]);
      if (d < routines[i].dayCount - 1) Serial.print(", ");
    }
    Serial.println();
    
    bool dayMatch = isDayInRoutine(routines[i], currentDay);
    Serial.print("   Dia coincide: "); Serial.println(dayMatch ? "SI" : "NO");
    
    if (dayMatch) {
      bool timeMatch = isTimeInRange(currentTime, routines[i].startTime, routines[i].endTime);
      Serial.print("   Tiempo en rango ("); 
      Serial.print(routines[i].startTime); Serial.print(" - "); 
      Serial.print(routines[i].endTime); Serial.print("): ");
      Serial.println(timeMatch ? "SI" : "NO");
      
      if (timeMatch) {
        float conditionValue = routines[i].condition.toFloat();
        bool humidityCondition = false;
        
        if (routines[i].isDry) {
          humidityCondition = (humidity > conditionValue);
          Serial.print("   Condicion Deshumidificador (Humedad > ");
          Serial.print(conditionValue); Serial.print("%): ");
        } else {
          humidityCondition = (humidity < conditionValue);
          Serial.print("   Condicion Humidificador (Humedad < ");
          Serial.print(conditionValue); Serial.print("%): ");
        }
        Serial.println(humidityCondition ? "SI" : "NO");
        
        bool tempInRange = (temperature >= Temp_min_device && temperature <= Temp_max_device);
        bool humInRange = (humidity >= humidity_min_device && humidity <= humidity_max_device);
        
        Serial.print("   Temperatura en rango dispositivo: "); Serial.println(tempInRange ? "SI" : "NO");
        Serial.print("   Humedad en rango dispositivo: "); Serial.println(humInRange ? "SI" : "NO");
        
        if (humidityCondition && tempInRange && humInRange) {
          Serial.println("   RUTINA ACTIVA ENCONTRADA!");
          routineActive = true;
          activeRoutineName = routines[i].name;
          deviceType = routines[i].isDry ? "Deshumidificador" : "Humidificador";
          break; 
        } else {
          Serial.println("   Rutina no cumple todas las condiciones");
        }
      }
    }
    Serial.println("   ___________________");
  }
  
  Serial.println("==============================");
  
  if (routineActive) {
    if (!estado_device) {
      Serial.println(">>> DISPOSITIVO ACTIVADO POR RUTINA <<<");
      Serial.print("Rutina activa: "); Serial.println(activeRoutineName);
      Serial.print("Tipo de dispositivo: "); Serial.println(deviceType);
      estado_device = true;
      active_device_type = deviceType;
    } else {
      Serial.println("Dispositivo ya estaba activo");
      Serial.print("Rutina actual: "); Serial.println(activeRoutineName);
      active_device_type = deviceType;
    }
  } else {
    if (estado_device && !estado_device_original) {
      Serial.println(">>> DISPOSITIVO DESACTIVADO - FIN DE RUTINA <<<");
      estado_device = false;
      active_device_type = "";
    } else if (estado_device_original) {
      Serial.println("Dispositivo permanece activo (estado del servidor)");
      active_device_type = "Deshumidificador";
    } else {
      Serial.println("No hay rutinas activas - dispositivo inactivo");
      active_device_type = "";
    }
  }
  
  Serial.print("Estado final del dispositivo: ");
  Serial.println(estado_device ? "ACTIVO" : "INACTIVO");
  if (estado_device && active_device_type.length() > 0) {
    Serial.print("Tipo de dispositivo activo: "); Serial.println(active_device_type);
  }
  Serial.println("================================");
  Serial.println("");
}

void getRoutineDataFromApi() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://host.wokwi.internal:5000/api/v1/routine-monitoring/data-records/iot-device/" + deviceId;

    Serial.print("Haciendo GET a rutinas: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("X-API-Key", "apichakiykey"); 

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Respuesta GET rutinas (código ");
      Serial.print(httpResponseCode);
      Serial.println("):");
      String responseBody = http.getString();
      Serial.println(responseBody);
      
      if (httpResponseCode == 200) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, responseBody);
        
        if (!error) {
          JsonArray routinesArray = doc.as<JsonArray>();
          routineCount = 0;
          
          Serial.println("=== PARSEANDO RUTINAS ===");
          
          for (JsonVariant routineVar : routinesArray) {
            if (routineCount >= MAX_ROUTINES) break;
            
            JsonObject routineObj = routineVar.as<JsonObject>();
            String routineDataStr = routineObj["routine_data"].as<String>();
            
            if (routineDataStr.length() > 0) {
              Routine& routine = routines[routineCount];
              
              int idStart = routineDataStr.indexOf("'id': ") + 6;
              int idEnd = routineDataStr.indexOf(",", idStart);
              routine.id = routineDataStr.substring(idStart, idEnd).toInt();
              
              int nameStart = routineDataStr.indexOf("'name': '") + 9;
              int nameEnd = routineDataStr.indexOf("'", nameStart);
              routine.name = routineDataStr.substring(nameStart, nameEnd);
              
              int conditionStart = routineDataStr.indexOf("'condition': '") + 14;
              int conditionEnd = routineDataStr.indexOf("'", conditionStart);
              routine.condition = routineDataStr.substring(conditionStart, conditionEnd);
              
              int isDryStart = routineDataStr.indexOf("'isDry': ") + 9;
              int isDryEnd = routineDataStr.indexOf(",", isDryStart);
              if (isDryEnd == -1) isDryEnd = routineDataStr.indexOf("}", isDryStart);
              String isDryStr = routineDataStr.substring(isDryStart, isDryEnd);
              routine.isDry = (isDryStr == "True");
              
              int startTimeStart = routineDataStr.indexOf("'startTime': '") + 14;
              int startTimeEnd = routineDataStr.indexOf("'", startTimeStart);
              routine.startTime = routineDataStr.substring(startTimeStart, startTimeEnd);
              
              int endTimeStart = routineDataStr.indexOf("'endTime': '") + 12;
              int endTimeEnd = routineDataStr.indexOf("'", endTimeStart);
              routine.endTime = routineDataStr.substring(endTimeStart, endTimeEnd);
              
              int daysStart = routineDataStr.indexOf("'days': [") + 9;
              int daysEnd = routineDataStr.indexOf("]", daysStart);
              String daysStr = routineDataStr.substring(daysStart, daysEnd);
              
              daysStr.replace("'", ""); 
              daysStr.replace(" ", "");  
              
              routine.dayCount = 0;
              int startPos = 0;
              int commaPos = daysStr.indexOf(',');
              
              while (commaPos != -1 && routine.dayCount < 7) {
                String day = daysStr.substring(startPos, commaPos);
                if (day.length() > 0) {
                  routine.days[routine.dayCount] = day;
                  routine.dayCount++;
                }
                startPos = commaPos + 1;
                commaPos = daysStr.indexOf(',', startPos);
              }
              
              if (startPos < daysStr.length() && routine.dayCount < 7) {
                String lastDay = daysStr.substring(startPos);
                if (lastDay.length() > 0) {
                  routine.days[routine.dayCount] = lastDay;
                  routine.dayCount++;
                }
              }
              
              routine.isActive = true;
              
              Serial.print("Rutina #"); Serial.print(routineCount + 1); 
              Serial.print(" cargada: "); Serial.println(routine.name);
              Serial.print("  Dias ("); Serial.print(routine.dayCount); Serial.print("): ");
              for (int d = 0; d < routine.dayCount; d++) {
                Serial.print(routine.days[d]);
                if (d < routine.dayCount - 1) Serial.print(", ");
              }
              Serial.println();
              Serial.print("   Horario: "); Serial.print(routine.startTime);
              Serial.print(" - "); Serial.println(routine.endTime);
              Serial.print("   Condicion: "); Serial.print(routine.condition);
              Serial.print("% | Tipo: "); Serial.println(routine.isDry ? "Deshumidificador" : "Humidificador");
              
              routineCount++;
            }
          }
          
          Serial.print("Total de rutinas cargadas: "); Serial.println(routineCount);
          Serial.println("=============================");
        } else {
          Serial.print("Error parseando JSON de rutinas: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.print("Error en GET rutinas. Codigo HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("No hay conexion WiFi para rutinas.");
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
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, responseBody);
        
        if (!error) {
          JsonObject humidifier_info = doc["humidifier_info"];
          
          if (!humidifier_info.isNull()) {
            ICA_min_device = humidifier_info["calidadDeAireMin"].as<int>();
            ICA_max_device = humidifier_info["calidadDeAireMax"].as<int>();
            Temp_min_device = humidifier_info["temperaturaMin"].as<float>();
            Temp_max_device = humidifier_info["temperaturaMax"].as<float>();
            humidity_min_device = humidifier_info["humedadMin"].as<float>();
            humidity_max_device = humidifier_info["humedadMax"].as<float>();
            estado_device_original = humidifier_info["estado"].as<bool>();
            
            if (!estado_device) {
              estado_device = estado_device_original;
              if (estado_device_original) {
                active_device_type = "Deshumidificador";
              }
            }
            
            Serial.println("=== Configuración del dispositivo cargada ===");
            Serial.print("ICA Min: "); Serial.println(ICA_min_device);
            Serial.print("ICA Max: "); Serial.println(ICA_max_device);
            Serial.print("Temp Min: "); Serial.println(Temp_min_device);
            Serial.print("Temp Max: "); Serial.println(Temp_max_device);
            Serial.print("Humedad Min: "); Serial.println(humidity_min_device);
            Serial.print("Humedad Max: "); Serial.println(humidity_max_device);
            Serial.print("Estado Device (servidor): "); Serial.println(estado_device_original ? "ACTIVO" : "INACTIVO");
            Serial.print("Estado Device (actual): "); Serial.println(estado_device ? "ACTIVO" : "INACTIVO");
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

  if ((tempOk && humOk) || estado_device)
  {
    lcd.setCursor(0, 0);
    if (estado_device && active_device_type.length() > 0) {
      if (active_device_type == "Deshumidificador") {
        lcd.print("Deshumidif. ON");
      } else {
        lcd.print("Humidif. ON");
      }
    } else {
      lcd.print("Deshumidif. ON");
    }
    digitalWrite(ledPin, HIGH);
    
    if (estado_device) {
      if (active_device_type.length() > 0) {
        Serial.print("INFO: "); Serial.print(active_device_type); Serial.println(" ON por rutina/servidor");
      } else {
        Serial.println("INFO: Deshumidificador ON por rutina/servidor");
      }
    } else {
      Serial.println("INFO: Deshumidificador ON por condiciones óptimas");
    }
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("Ambiente MAL");
    digitalWrite(ledPin, LOW);
    
    if (!tempOk && !humOk)
    {
      Serial.println("ALERTA: Temperatura Y humedad fuera de rango");
    }
    else if (!tempOk)
    {
      Serial.print("ALERTA: Temperatura fuera de rango (");
      Serial.print(Temp_min_device);
      Serial.print("-");
      Serial.print(Temp_max_device);
      Serial.println(")");
    }
    else if (!humOk)
    {
      Serial.print("ALERTA: Humedad fuera de rango (");
      Serial.print(humidity_min_device);
      Serial.print("-");
      Serial.print(humidity_max_device);
      Serial.println(")");
    }
    else if (!estado_device)
    {
      Serial.println("INFO: Dispositivo desactivado desde el servidor");
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

    sendToEdgeApi(temperature, humidity, ICA);
  } else {
    Serial.println("Error leyendo del DHT22");
  }
}

void setup() {
  Serial.begin(9600);

  //Cambiar por red wifi real
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
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
  getDeviceInfoFromApi();
  getRoutineDataFromApi();
}

void loop() {
  unsigned long now = millis();
  
  if (now - lastSensorUpdate >= sensorUpdateInterval) {
    lastSensorUpdate = now;
    updateSensorData();
  }
  
  if (now - lastApiUpdate >= apiUpdateInterval) {
    lastApiUpdate = now;
    Serial.println("=== Actualizando datos del servidor ===");
    getDeviceInfoFromApi();
    getRoutineDataFromApi();
    Serial.println("=== Actualización completada ===");
  }
  
  if (now - lastRoutineCheck >= routineCheckInterval) {
    lastRoutineCheck = now;
    checkActiveRoutines();
  }
}