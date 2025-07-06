#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "StateManager.h"

class ActuatorManager {
private:
    LiquidCrystal_I2C lcd;
    int ledPin;
    StateManager* stateManager;
    
public:
    ActuatorManager(int ledPin = 32);
    
    void begin();
    void setStateManager(StateManager* sm);
    void updateDisplay();
    void updateLED();
    void controlDevice(bool activate);
    void displayServerInfo(const String& serverIP);
};

#endif
