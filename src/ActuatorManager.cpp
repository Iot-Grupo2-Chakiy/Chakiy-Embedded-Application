#include "ActuatorManager.h"

ActuatorManager::ActuatorManager(int ledPin) : lcd(0x27, 20, 4), ledPin(ledPin), stateManager(nullptr) {
}

void ActuatorManager::begin() {
    lcd.init();
    lcd.backlight();
    
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    
    Serial.println("ActuatorManager initialized");
}

void ActuatorManager::setStateManager(StateManager* sm) {
    stateManager = sm;
}

void ActuatorManager::updateDisplay() {
    if (!stateManager) return;
    
    DeviceState& state = stateManager->getDeviceState();
    lcd.clear();
    
    lcd.setCursor(0, 0);
    if (state.estado_device) {
        if (state.active_device_type.length() > 0) {
            if (state.active_device_type == "Deshumidificador") {
                lcd.print("Deshumidif. ON");
            } else {
                lcd.print("Humidif. ON");
            }
        } else {
            lcd.print("Deshumidif. ON");
        }
    } else {
        lcd.print("Dispositivo OFF");
    }
    
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(state.temperature, 1);
    lcd.print("C H:");
    lcd.print(state.humidity, 0);
    lcd.print("%");
    lcd.print("ICA:");
    lcd.print(state.ICA);
    
    lcd.setCursor(0, 2);
    if (state.api_error_message.length() > 0) {
        lcd.print(state.api_error_message);
        for (int i = state.api_error_message.length(); i < 20; i++) {
            lcd.print(" ");
        }
    } else {
        lcd.print("                    "); 
    }
}

void ActuatorManager::updateLED() {
    if (!stateManager) return;
    
    DeviceState& state = stateManager->getDeviceState();
    digitalWrite(ledPin, state.estado_device ? HIGH : LOW);
}

void ActuatorManager::controlDevice(bool activate) {
    if (!stateManager) return;
    
    DeviceState& state = stateManager->getDeviceState();
    
    // ALWAYS check environment safety - turn off device if unsafe
    if (!stateManager->isEnvironmentSafe()) {
        if (state.estado_device) {  // Only show message if device was on
            Serial.println(">>> DISPOSITIVO APAGADO AUTOMÁTICAMENTE - FUERA DE UMBRALES DE SEGURIDAD <<<");
            
            if (!stateManager->isTemperatureInRange() && !stateManager->isHumidityInRange()) {
                Serial.println("ALERTA: Temperatura Y humedad fuera de rango de seguridad");
            } else if (!stateManager->isTemperatureInRange()) {
                Serial.print("ALERTA: Temperatura fuera de rango de seguridad (");
                Serial.print(state.Temp_min_device);
                Serial.print("-");
                Serial.print(state.Temp_max_device);
                Serial.println(")");
            } else if (!stateManager->isHumidityInRange()) {
                Serial.print("ALERTA: Humedad fuera de rango de seguridad (");
                Serial.print(state.humidity_min_device);
                Serial.print("-");
                Serial.print(state.humidity_max_device);
                Serial.println(")");
            }
        }
        
        stateManager->setDeviceStatus(false, "");
        digitalWrite(ledPin, LOW);
        return;
    }
    
    digitalWrite(ledPin, activate ? HIGH : LOW);
    
    if (activate) {
        if (state.active_device_type.length() > 0) {
            Serial.print("INFO: "); Serial.print(state.active_device_type); 
            Serial.println(" ON por rutina/servidor (rutinas tienen prioridad)");
        } else {
            Serial.println("INFO: Deshumidificador ON por rutina/servidor (rutinas tienen prioridad)");
        }
    } else {
        Serial.println("INFO: Condiciones ambientales óptimas - Dispositivo OFF (esperando rutina o activación manual)");
    }
}

void ActuatorManager::displayServerInfo(const String& serverIP) {
    lcd.setCursor(0, 3);
    lcd.print("IP: ");
    lcd.print(serverIP);
    // Fill with spaces to clear remaining characters
    for (int i = serverIP.length() + 4; i < 20; i++) {
        lcd.print(" ");
    }
}
