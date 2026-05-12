# phosphor-chassis-power Application Overview

## Purpose
The `phosphor-chassis-power` application monitors chassis power on multi-chassis systems to handle chassis-level fault monitoring and recovery. It's designed for large rack-mounted systems where each drawer/sled corresponds to a chassis.

## Architecture

### Key Components

1. **Manager** (`manager.hpp/cpp`)
   - Main orchestrator that initializes the application
   - Finds compatible system types via D-Bus
   - Loads system-specific JSON configuration files
   - Creates and manages the System object

2. **System** (`system.hpp/cpp`)
   - Represents the entire computer system
   - Contains one or more Chassis objects
   - Initializes PowerSystemInputs D-Bus interfaces for all chassis

3. **Chassis** (`chassis.hpp/cpp`)
   - Represents individual chassis/sleds in the system
   - Manages GPIO pins for presence detection and fault monitoring
   - Implements PowerSystemInputs D-Bus interface to expose chassis power status

4. **GPIO** (`gpio.hpp`)
   - Represents individual GPIO hardware pins
   - Supports input/output direction and high/low polarity
   - Optional default values for deglitching

5. **ChassisPowerSystemInterface** (`chassis_power_system_interface.hpp/cpp`)
   - D-Bus interface implementation for `xyz.openbmc_project.State.Decorator.PowerSystemInputs`
   - Exposes chassis power input status (Good/Fault)

### Application Flow

1. **Startup** (`main.cpp`):
   - Creates D-Bus connection and event loop
   - Instantiates Manager object
   - Enters event loop

2. **Initialization** (Manager):
   - Starts timer to wait for compatible system types
   - Searches for D-Bus objects with Compatible interface
   - When found, loads appropriate JSON config file
   - Parses config and creates System with Chassis objects
   - Initializes PowerSystemInputs interfaces

3. **Configuration**:
   - JSON files define chassis GPIOs (presence, fault detection, reset control)
   - Located in `/usr/share/phosphor-chassis-power/` or `/tmp/phosphor-chassis-power/` (test)
   - Example: `Huygens.json` defines 8 chassis with various GPIOs

### GPIO Types (per Chassis)

Based on Huygens.json configuration:
- **PresenceGpio**: Detects if chassis/sled is physically present
- **FaultUnlatchedGpio**: Detects unlatched power faults
- **FaultLatchedGpio**: Detects latched power faults
- **FaultLatchResetGpio**: Output to reset latched faults
- **EnableSystemResetGpio**: Output to enable/disable system reset

## Clearing Data on BMC Reset or IPL

### Current Implementation Status

Based on the code review and `IMPLEMENTATION_R-PCP-2.md` document, the application **has a design for BMC reset handling** but the actual implementation appears to be **planned but not yet present** in the current codebase.

### Planned BMC Reset Handling (R-PCP-2)

The design document describes how data should be cleared/initialized on BMC reset:

**For Missing Chassis:**
- Disable `reset-enable-chassisX-standby-power` GPIO
- Disable `power-chassisX-standby-fault-reset` GPIO

**For Present Chassis:**
- If fault-unlatched GPIO is active:
  - Disable both GPIOs
  - Set PowerSystemInputs.Status = Fault
- If chassis is powered off:
  - Disable both GPIOs
- If chassis is powered on:
  - Enable both GPIOs

### Implementation Approach

To implement data clearing on BMC reset/IPL:

1. **Add `handleBmcReset()` method to System class** that:
   - Iterates through all chassis
   - Checks presence status
   - Reads fault GPIO states
   - Configures output GPIOs appropriately
   - Updates D-Bus interface status

2. **Call from Manager during initialization** after:
   - Config file is parsed
   - System object is created
   - PowerSystemInputs interfaces are initialized

3. **Use libgpiod** for GPIO operations:
   - Read input GPIOs (presence, fault detection)
   - Write output GPIOs (enable/disable reset controls)

### Service Configuration

The systemd service (`phosphor-chassis-power.service`):
- Runs continuously with `Restart=always`
- Starts during multi-user.target
- No special BMC reset dependencies currently configured

### Recommendation

To properly clear data on BMC reset, you should:

1. **Implement the `handleBmcReset()` method in System class**
   - Add method declaration in `system.hpp`
   - Implement logic in `system.cpp`

2. **Add GPIO read/write methods to Chassis class** using libgpiod:
   - `isPresent()` - Check chassis presence
   - `isPoweredOn()` - Determine power state
   - `isFaultUnlatchedActive()` - Read fault GPIO
   - `setGpioOutput()` - Enable/disable output GPIOs
   - `readGpioInput()` - Read input GPIO values

3. **Call `handleBmcReset()` from Manager** after config loading:
   - In `loadConfigFile()` method after system object creation
   - After `initializePowerSystemInputs()` call

4. **Add libgpiodcxx dependency** to `meson.build`:
   ```meson
   libgpiodcxx = dependency('libgpiodcxx')
   ```

5. **Consider systemd service dependencies** if specific timing is needed:
   - Add `After=` directives for services that must start first
   - Add `Wants=` or `Requires=` for critical dependencies

### Current Behavior

The application currently:
- Initializes PowerSystemInputs interfaces to "Good" status on startup
- Does NOT perform the full GPIO configuration logic described in R-PCP-2
- Does NOT read GPIO states to determine actual chassis status
- Does NOT configure output GPIOs based on presence/power state

### Example Configuration (Huygens.json)

```json
{
  "ChassisNumber": 1,
  "PresenceGpio": {
    "Name": "presence-chassis1",
    "Direction": "Input",
    "Polarity": "Low",
    "DefaultValue": 0
  },
  "FaultUnlatchedGpio": {
    "Name": "power-chs1-sb-fault-unlatched",
    "Direction": "Input",
    "Polarity": "Low",
    "DefaultValue": 0
  },
  "FaultLatchResetGpio": {
    "Name": "power-chs1-sb-fault-reset",
    "Direction": "Output",
    "Polarity": "Low"
  },
  "EnableSystemResetGpio": {
    "Name": "reset-enable-chs1-sb-power",
    "Direction": "Output",
    "Polarity": "High"
  },
  "PresencePath": "/dev/i2c-159"
}
```

## D-Bus Interface

The application exposes chassis power status via D-Bus:

**Interface**: `xyz.openbmc_project.State.Decorator.PowerSystemInputs`

**Object Path**: `/xyz/openbmc_project/state/chassis{N}`

**Properties**:
- `Status` - Enum: `Good` or `Fault`

**Service Name**: `xyz.openbmc_project.Power.Chassis`

## Files and Directories

### Source Files
- `phosphor-chassis-power/src/main.cpp` - Application entry point
- `phosphor-chassis-power/src/manager.{hpp,cpp}` - Main manager class
- `phosphor-chassis-power/src/system.{hpp,cpp}` - System representation
- `phosphor-chassis-power/src/chassis.{hpp,cpp}` - Chassis representation
- `phosphor-chassis-power/src/gpio.{hpp,cpp}` - GPIO abstraction
- `phosphor-chassis-power/src/chassis_power_system_interface.{hpp,cpp}` - D-Bus interface
- `phosphor-chassis-power/src/config_file_parser.{hpp,cpp}` - JSON parser

### Configuration
- `phosphor-chassis-power/config_files/` - System-specific JSON configs
- `services/phosphor-chassis-power.service` - Systemd service file

### Documentation
- `phosphor-chassis-power/README.md` - Basic overview
- `phosphor-chassis-power/docs/README.md` - Detailed documentation
- `phosphor-chassis-power/IMPLEMENTATION_R-PCP-2.md` - BMC reset design

## Dependencies

- **sdbusplus** - D-Bus communication
- **sdeventplus** - Event loop
- **phosphor-logging** - Logging framework
- **nlohmann_json** - JSON parsing
- **libgpiodcxx** - GPIO operations (planned for R-PCP-2)

## Summary

The `phosphor-chassis-power` application provides chassis-level power monitoring for multi-chassis systems. While the basic infrastructure is in place, the full BMC reset handling logic (R-PCP-2) needs to be implemented to properly initialize GPIO states and D-Bus interfaces based on actual hardware status after a BMC reset or IPL.