#include "DeviceManager.h"
#include <time.h>

DeviceManager::DeviceManager(int dhtPin, int dhtType, int ledPin) 
    : dht(dhtPin, dhtType), actuatorManager(ledPin) {
    
    serverIP = "host.wokwi.internal";
    deviceId = "PruebaOtraVes";
    
    lastSensorUpdate = 0;
    lastApiUpdate = 0;
    lastRoutineCheck = 0;
}

void DeviceManager::setup() {
    Serial.begin(9600);
    
    // Initialize components
    dht.begin();
    actuatorManager.begin();
    actuatorManager.setStateManager(&stateManager);
    
    // Connect to WiFi
    connectWiFi("Wokwi-GUEST", "");
    
    // Print connection info
    printConnectionInfo();
    
    // Initialize time
    initializeTime();
    
    // Get initial data from API
    getDeviceInfoFromApi();
    getRoutineDataFromApi();
    
    Serial.println("DeviceManager setup completed");
}

String DeviceManager::getCurrentTimeDevice() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "00:00";
    }
    
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    return String(timeStr);
}
void DeviceManager::loop() {
    unsigned long now = millis();
    
    // Update sensor data periodically
    if (now - lastSensorUpdate >= sensorUpdateInterval) {
        lastSensorUpdate = now;
        updateSensorData();
    }
    
    // Update API data periodically
    if (now - lastApiUpdate >= apiUpdateInterval) {
        lastApiUpdate = now;
        Serial.println("=== Actualizando datos del servidor ===");
        getDeviceInfoFromApi();
        getRoutineDataFromApi();
        Serial.println("=== Actualización completada ===");
    }
    
    // Check routines periodically
    if (now - lastRoutineCheck >= routineCheckInterval) {
        lastRoutineCheck = now;
        stateManager.checkActiveRoutines();
        
        // Update actuators based on state
        DeviceState& state = stateManager.getDeviceState();
        actuatorManager.controlDevice(state.estado_device);
        actuatorManager.updateDisplay();
        actuatorManager.displayServerInfo(serverIP);
    }
    
    // Process serial commands
    processSerialCommands();
}

void DeviceManager::connectWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    
    Serial.print("Conectando a WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" Conectado!");
}

void DeviceManager::setServerIP(const String& ip) {
    serverIP = ip;
}

void DeviceManager::setDeviceId(const String& id) {
    deviceId = id;
}

void DeviceManager::getDeviceInfoFromApi() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://" + serverIP + ":5000/api/v1/health-dehumidifier/get-dehumidifier?device_id=" + deviceId;

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
                        int icaMin = humidifier_info["calidadDeAireMin"].as<int>();
                        int icaMax = humidifier_info["calidadDeAireMax"].as<int>();
                        float tempMin = humidifier_info["temperaturaMin"].as<float>();
                        float tempMax = humidifier_info["temperaturaMax"].as<float>();
                        float humMin = humidifier_info["humedadMin"].as<float>();
                        float humMax = humidifier_info["humedadMax"].as<float>();
                        
                        stateManager.updateDeviceConfiguration(icaMin, icaMax, tempMin, tempMax, humMin, humMax);
                        
                        DeviceState& state = stateManager.getDeviceState();
                        bool newEstadoDeviceOriginal = humidifier_info["estado"].as<bool>();
                        
                        if (newEstadoDeviceOriginal != state.estado_device_original) {
                            Serial.println("=== CAMBIO MANUAL DEL USUARIO DETECTADO ===");
                            Serial.print("Estado anterior: "); Serial.println(state.estado_device_original ? "ACTIVO" : "INACTIVO");
                            Serial.print("Estado nuevo: "); Serial.println(newEstadoDeviceOriginal ? "ACTIVO" : "INACTIVO");
                            
                            if (!newEstadoDeviceOriginal && state.estado_device_original) {
                                Serial.println(">>> DISPOSITIVO APAGADO POR USUARIO <<<");
                                stateManager.setDeviceStatus(false, "");
                            }
                            else if (newEstadoDeviceOriginal && !state.estado_device_original) {
                                Serial.println(">>> DISPOSITIVO ENCENDIDO POR USUARIO <<<");
                                stateManager.setDeviceStatus(true, "Deshumidificador");
                            }
                            
                            state.estado_device_original = newEstadoDeviceOriginal;
                            Serial.println("============================================");
                        } else {
                            if (!state.estado_device) {
                                stateManager.setDeviceStatus(state.estado_device_original, 
                                    state.estado_device_original ? "Deshumidificador" : "");
                            }
                        }
                        
                        Serial.println("=== Configuración del dispositivo cargada ===");
                        Serial.print("ICA Min: "); Serial.println(icaMin);
                        Serial.print("ICA Max: "); Serial.println(icaMax);
                        Serial.print("Temp Min: "); Serial.println(tempMin);
                        Serial.print("Temp Max: "); Serial.println(tempMax);
                        Serial.print("Humedad Min: "); Serial.println(humMin);
                        Serial.print("Humedad Max: "); Serial.println(humMax);
                        Serial.print("Estado Device (servidor): "); Serial.println(state.estado_device_original ? "ACTIVO" : "INACTIVO");
                        Serial.print("Estado Device (actual): "); Serial.println(state.estado_device ? "ACTIVO" : "INACTIVO");
                        Serial.println("============================================");
                        
                        stateManager.setApiError("");
                    } else {
                        Serial.println("No se encontró humidifier_info en la respuesta");
                    }
                } else {
                    Serial.print("Error parseando JSON: ");
                    Serial.println(error.c_str());
                }
            } else if (httpResponseCode == 404) {
                Serial.println("Dispositivo no encontrado en el servidor");
                stateManager.setApiError("ERROR: Device 404");
            } else if (httpResponseCode == 500) {
                Serial.println("Error interno del servidor - continuando con operación normal");
                stateManager.setApiError("ERROR: Server 500");
            } else {
                Serial.println("Error desconocido del servidor");
                stateManager.setApiError("ERROR: Server " + String(httpResponseCode));
            }
        } else {
            Serial.print("Error al obtener info del dispositivo. Código HTTP: ");
            Serial.println(httpResponseCode);
            stateManager.setApiError("ERROR: No connection");
        }

        http.end();
    } else {
        Serial.println("No hay conexión WiFi para obtener info del dispositivo.");
        stateManager.setApiError("ERROR: No WiFi");
    }
}

void DeviceManager::getRoutineDataFromApi() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://" + serverIP + ":5000/api/v1/routine-monitoring/data-records/iot-device/" + deviceId;

        http.begin(url);
        http.addHeader("X-API-Key", "apichakiykey");

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            String responseBody = http.getString();
            
            if (httpResponseCode == 200) {
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, responseBody);
                
                if (!error) {
                    JsonArray routinesArray = doc.as<JsonArray>();
                    stateManager.clearRoutines();
                    
                    Serial.println("=== PARSEANDO RUTINAS ===");
                    
                    for (JsonVariant routineVar : routinesArray) {
                        JsonObject routineObj = routineVar.as<JsonObject>();
                        String routineDataStr = routineObj["routine_data"].as<String>();
                        
                        if (routineDataStr.length() > 0) {
                            Routine routine;
                            parseRoutineData(routineDataStr, routine);
                            
                            if (stateManager.addRoutine(routine)) {
                                Serial.print("Rutina #"); Serial.print(stateManager.getRoutineCount()); 
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
                            }
                        }
                    }
                    
                    Serial.print("Total de rutinas cargadas: "); Serial.println(stateManager.getRoutineCount());
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

void DeviceManager::sendToEdgeApi(float temp, float hum, int ica) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        String url = "http://" + serverIP + ":5000/api/v1/health-dehumidifier/data-records";

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

        if (httpResponseCode > 0 && httpResponseCode >= 200 && httpResponseCode < 300) {
            Serial.print("Datos enviados exitosamente. Código HTTP: ");
            Serial.println(httpResponseCode);
            stateManager.setApiError("");
        } else if (httpResponseCode > 0) {
            Serial.print("Error al enviar datos. Código HTTP: ");
            Serial.println(httpResponseCode);
            stateManager.setApiError("ERROR: Servidor " + String(httpResponseCode));
        } else {
            Serial.println("Error de conexión al servidor");
            stateManager.setApiError("ERROR: Sin servidor");
        }

        http.end();
    } else {
        Serial.println("No hay conexión WiFi.");
        stateManager.setApiError("ERROR: Sin WiFi");
    }
}

void DeviceManager::updateSensorData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
        Serial.print("Temp: ");
        Serial.print(temperature);
        Serial.print(" °C | Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");

        stateManager.updateSensorData(temperature, humidity);
        
        DeviceState& state = stateManager.getDeviceState();
        sendToEdgeApi(temperature, humidity, state.ICA);
    } else {
        Serial.println("Error leyendo del DHT22");
    }
}

void DeviceManager::printConnectionInfo() {
    Serial.println("========================================");
    Serial.println("=== INFORMACIÓN DE CONECTIVIDAD ===");
    Serial.print("ESP32 conectado a WiFi: "); 
    Serial.println(WiFi.SSID());
    Serial.print("IP del ESP32: "); 
    Serial.println(WiFi.localIP());
    Serial.print("IP del servidor configurada: "); 
    Serial.println(serverIP);
    Serial.print("Puerto del servidor: 5000");
    Serial.println();
    Serial.print("URL base de la API: http://"); 
    Serial.print(serverIP); 
    Serial.println(":5000");
    Serial.println("========================================");
    Serial.println();
}

void DeviceManager::processSerialCommands() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("IP:")) {
            String newIP = command.substring(3);
            newIP.trim();
            
            if (newIP.length() > 0) {
                serverIP = newIP;
                Serial.println("========================================");
                Serial.println("=== IP DEL SERVIDOR ACTUALIZADA ===");
                Serial.print("Nueva IP configurada: ");
                Serial.println(serverIP);
                Serial.print("Nueva URL base: http://");
                Serial.print(serverIP);
                Serial.println(":5000");
                Serial.println("========================================");
                Serial.println();
                
                Serial.println("Probando conectividad con la nueva IP...");
                getDeviceInfoFromApi();
            } else {
                Serial.println("ERROR: IP vacía. Formato correcto: IP:192.168.1.100");
            }
        } else if (command.equals("HELP") || command.equals("help")) {
            Serial.println("========================================");
            Serial.println("=== COMANDOS DISPONIBLES ===");
            Serial.println("IP:x.x.x.x    - Cambiar IP del servidor");
            Serial.println("              - Ejemplo: IP:192.168.1.100");
            Serial.println("HELP          - Mostrar esta ayuda");
            Serial.println("INFO          - Mostrar información actual");
            Serial.println("========================================");
        } else if (command.equals("INFO") || command.equals("info")) {
            printConnectionInfo();
        } else if (command.length() > 0) {
            Serial.println("Comando no reconocido. Envía 'HELP' para ver comandos disponibles.");
        }
    }
}

void DeviceManager::initializeTime() {
    configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void DeviceManager::parseRoutineData(const String& routineDataStr, Routine& routine) {
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
}
