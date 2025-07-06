#include "StateManager.h"
#include <time.h>

StateManager::StateManager() {
    // Initialize device state
    deviceState.temperature = 0.0;
    deviceState.humidity = 0.0;
    deviceState.ICA = 42;
    deviceState.estado_device = false;
    deviceState.estado_device_original = false;
    deviceState.active_device_type = "";
    deviceState.api_error_message = "";
    
    // Initialize device configuration with defaults
    deviceState.ICA_min_device = 0;
    deviceState.ICA_max_device = 0;
    deviceState.Temp_min_device = 0.0;
    deviceState.Temp_max_device = 0.0;
    deviceState.humidity_min_device = 0.0;
    deviceState.humidity_max_device = 0.0;
    
    // Initialize routines
    routineCount = 0;
}

DeviceState& StateManager::getDeviceState() {
    return deviceState;
}

void StateManager::updateSensorData(float temp, float hum) {
    deviceState.temperature = temp;
    deviceState.humidity = hum;
    
    // Calculate ICA
    deviceState.ICA = (int)(abs(temp - 22) * 2 + abs(hum - 50) * 0.5);
}

void StateManager::updateDeviceConfiguration(int icaMin, int icaMax, float tempMin, float tempMax, float humMin, float humMax) {
    deviceState.ICA_min_device = icaMin;
    deviceState.ICA_max_device = icaMax;
    deviceState.Temp_min_device = tempMin;
    deviceState.Temp_max_device = tempMax;
    deviceState.humidity_min_device = humMin;
    deviceState.humidity_max_device = humMax;
}

void StateManager::setDeviceStatus(bool status, String deviceType) {
    deviceState.estado_device = status;
    if (deviceType.length() > 0) {
        deviceState.active_device_type = deviceType;
    }
}

void StateManager::setApiError(String error) {
    deviceState.api_error_message = error;
}

void StateManager::clearRoutines() {
    routineCount = 0;
}

bool StateManager::addRoutine(const Routine& routine) {
    if (routineCount >= MAX_ROUTINES) {
        return false;
    }
    
    routines[routineCount] = routine;
    routineCount++;
    return true;
}

int StateManager::getRoutineCount() const {
    return routineCount;
}

Routine* StateManager::getRoutines() {
    return routines;
}

String StateManager::getCurrentDay() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "UNKNOWN";
    }
    
    String days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
    return days[timeinfo.tm_wday];
}

String StateManager::getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "00:00";
    }
    
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    return String(timeStr);
}

bool StateManager::isDayInRoutine(const Routine& routine, const String& currentDay) {
    for (int i = 0; i < routine.dayCount; i++) {
        if (routine.days[i] == currentDay) {
            return true;
        }
    }
    return false;
}

bool StateManager::isTimeInRange(const String& currentTime, const String& startTime, const String& endTime) {
    int currentMinutes = currentTime.substring(0, 2).toInt() * 60 + currentTime.substring(3, 5).toInt();
    int startMinutes = startTime.substring(0, 2).toInt() * 60 + startTime.substring(3, 5).toInt();
    int endMinutes = endTime.substring(0, 2).toInt() * 60 + endTime.substring(3, 5).toInt();
    
    if (startMinutes <= endMinutes) {
        return (currentMinutes >= startMinutes && currentMinutes <= endMinutes);
    } else {
        return (currentMinutes >= startMinutes || currentMinutes <= endMinutes);
    }
}

bool StateManager::isTemperatureInRange() const {
    return (deviceState.temperature >= deviceState.Temp_min_device && 
            deviceState.temperature <= deviceState.Temp_max_device);
}

bool StateManager::isHumidityInRange() const {
    return (deviceState.humidity >= deviceState.humidity_min_device && 
            deviceState.humidity <= deviceState.humidity_max_device);
}

bool StateManager::isEnvironmentSafe() const {
    return isTemperatureInRange() && isHumidityInRange();
}

void StateManager::checkActiveRoutines() {
    String currentDay = getCurrentDay();
    String currentTime = getCurrentTime();
    
    Serial.println("");
    Serial.println("=== VERIFICANDO RUTINAS ===");
    Serial.print("Dia actual: "); Serial.println(currentDay);
    Serial.print("Hora actual: "); Serial.println(currentTime);
    Serial.print("Humedad actual: "); Serial.print(deviceState.humidity); Serial.println("%");
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
                    humidityCondition = (deviceState.humidity > conditionValue);
                    Serial.print("   Condicion Deshumidificador (Humedad > ");
                    Serial.print(conditionValue); Serial.print("%): ");
                } else {
                    humidityCondition = (deviceState.humidity < conditionValue);
                    Serial.print("   Condicion Humidificador (Humedad < ");
                    Serial.print(conditionValue); Serial.print("%): ");
                }
                Serial.println(humidityCondition ? "SI" : "NO");
                
                bool tempInRange = isTemperatureInRange();
                bool humInRange = isHumidityInRange();
                
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
    
    // Las rutinas tienen PRIORIDAD ABSOLUTA sobre decisiones manuales del usuario
    if (routineActive) {
        if (!deviceState.estado_device) {
            Serial.println(">>> DISPOSITIVO ACTIVADO POR RUTINA <<<");
            Serial.print("Rutina activa: "); Serial.println(activeRoutineName);
            Serial.print("Tipo de dispositivo: "); Serial.println(deviceType);
            
            if (!deviceState.estado_device_original) {
                Serial.println(">>> RUTINA OVERRIDE - IGNORANDO ESTADO MANUAL DEL USUARIO <<<");
                Serial.println("    Las rutinas programadas tienen prioridad absoluta");
            }
            
            deviceState.estado_device = true;
            deviceState.active_device_type = deviceType;
        } else {
            Serial.println("Dispositivo ya estaba activo");
            Serial.print("Rutina actual: "); Serial.println(activeRoutineName);
            deviceState.active_device_type = deviceType;
        }
    } else {
        if (deviceState.estado_device && !deviceState.estado_device_original) {
            deviceState.estado_device = false;
            deviceState.active_device_type = "";
        } else if (deviceState.estado_device_original) {
            deviceState.estado_device = true;
            deviceState.active_device_type = "Deshumidificador";
        } else {
            deviceState.estado_device = false;
            deviceState.active_device_type = "";
        }
    }
    
    Serial.print("Estado final del dispositivo: ");
    Serial.println(deviceState.estado_device ? "ACTIVO" : "INACTIVO");
    if (deviceState.estado_device && deviceState.active_device_type.length() > 0) {
        Serial.print("Tipo de dispositivo activo: "); Serial.println(deviceState.active_device_type);
    }
}
