#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "StateManager.h"
#include "ActuatorManager.h"

class DeviceManager {
private:
    // Components
    DHT dht;
    StateManager stateManager;
    ActuatorManager actuatorManager;
    
    // Network configuration
    String serverIP;
    String deviceId;
    
    // Timing control
    unsigned long lastSensorUpdate;
    unsigned long lastApiUpdate;
    unsigned long lastRoutineCheck;
    const unsigned long sensorUpdateInterval = 5000;
    const unsigned long apiUpdateInterval = 10000;
    const unsigned long routineCheckInterval = 10000;
    
public:
    DeviceManager(int dhtPin = 33, int dhtType = DHT22, int ledPin = 32);
    
    void setup();
    void loop();
    
    // Network methods
    void connectWiFi(const char* ssid, const char* password);
    void setServerIP(const String& ip);
    void setDeviceId(const String& id);
    
    // API methods
    void getDeviceInfoFromApi();
    void getRoutineDataFromApi();
    void sendToEdgeApi(float temp, float hum, int ica);
    String getCurrentTimeDevice();
    
    // Sensor methods
    void updateSensorData();
    
    // Utility methods
    void printConnectionInfo();
    void processSerialCommands();
    
private:
    void initializeTime();
    void parseRoutineData(const String& routineDataStr, Routine& routine);
};

#endif
