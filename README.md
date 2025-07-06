# Chakiy IoT Device - Refactored Architecture

This project has been refactored following IoT best practices with a modular architecture.

## Project Structure

```
src/
├── main.cpp              # Main entry point - only contains setup() and loop()
├── StateManager.cpp      # Manages device state and routines
├── ActuatorManager.cpp   # Controls LCD, LED, and device outputs
└── DeviceManager.cpp     # Orchestrates all components and API communication

include/
├── StateManager.h        # Device state and routine management
├── ActuatorManager.h     # Hardware actuator control
└── DeviceManager.h       # Main device coordination
```

## Architecture Overview

### StateManager
- **Purpose**: Centralized state management for the IoT device
- **Responsibilities**:
  - Device state (temperature, humidity, ICA, device status)
  - Device configuration (thresholds, ranges)
  - Routine management (add, clear, check routines)
  - Time utilities for routine scheduling
  - Environment safety checks

### ActuatorManager
- **Purpose**: Hardware abstraction layer for outputs
- **Responsibilities**:
  - LCD display control (20x4 I2C LCD)
  - LED control for device status indication
  - Device activation/deactivation with safety checks
  - Display formatting for sensor data and status

### DeviceManager
- **Purpose**: Main orchestrator and API communication
- **Responsibilities**:
  - WiFi connectivity management
  - API communication (device info, routines, data sending)
  - Sensor data reading (DHT22)
  - Timing coordination for different update intervals
  - Serial command processing
  - Component initialization and coordination

## Key Features

1. **Separation of Concerns**: Each manager has a specific responsibility
2. **Modular Design**: Components can be easily modified or replaced
3. **Centralized State**: All device state is managed in one place
4. **Hardware Abstraction**: Actuator control is separated from business logic
5. **API Management**: All external communication is handled by DeviceManager

## Usage

The main.cpp file is now extremely simple:

```cpp
#include "DeviceManager.h"

DeviceManager device;

void setup() {
    device.setup();
}

void loop() {
    device.loop();
}
```

## Configuration

Default settings:
- DHT22 sensor on pin 33
- LED on pin 32
- LCD I2C address: 0x27 (20x4 display)
- WiFi: "Wokwi-GUEST"
- Server IP: "host.wokwi.internal"
- Device ID: "PruebaOtraVes"

## Serial Commands

- `IP:x.x.x.x` - Change server IP address
- `HELP` - Show available commands
- `INFO` - Show connection information

## Benefits of This Architecture

1. **Maintainability**: Each component can be updated independently
2. **Testability**: Components can be tested in isolation
3. **Scalability**: New features can be added without affecting existing code
4. **Readability**: Code is organized by functionality
5. **Reusability**: Components can be reused in other projects
