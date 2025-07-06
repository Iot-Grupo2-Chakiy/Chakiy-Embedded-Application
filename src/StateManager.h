#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>

struct DeviceState {
    float temperature;
    float humidity;
    int ICA;
    bool estado_device;
    bool estado_device_original;
    String active_device_type;
    String api_error_message;
    
    // Device configuration
    int ICA_min_device;
    int ICA_max_device;
    float Temp_min_device;
    float Temp_max_device;
    float humidity_min_device;
    float humidity_max_device;
};

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

class StateManager {
private:
    DeviceState deviceState;
    static const int MAX_ROUTINES = 100;
    Routine routines[MAX_ROUTINES];
    int routineCount;
    
public:
    StateManager();
    
    // Device state management
    DeviceState& getDeviceState();
    void updateSensorData(float temp, float hum);
    void updateDeviceConfiguration(int icaMin, int icaMax, float tempMin, float tempMax, float humMin, float humMax);
    void setDeviceStatus(bool status, String deviceType = "");
    void setApiError(String error);
    
    // Routine management
    void clearRoutines();
    bool addRoutine(const Routine& routine);
    int getRoutineCount() const;
    Routine* getRoutines();
    
    // Time utilities
    String getCurrentDay();
    String getCurrentTime();
    bool isDayInRoutine(const Routine& routine, const String& currentDay);
    bool isTimeInRange(const String& currentTime, const String& startTime, const String& endTime);
    
    // Environment checks
    bool isTemperatureInRange() const;
    bool isHumidityInRange() const;
    bool isEnvironmentSafe() const;
    
    // Routine logic
    void checkActiveRoutines();
};

#endif
