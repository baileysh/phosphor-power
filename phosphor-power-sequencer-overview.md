# phosphor-power-sequencer Application Overview

## Purpose
The `phosphor-power-sequencer` application is responsible for:
1. **Powering the system on/off** - Controls all chassis in the system
2. **Monitoring power good (pgood) state** - Continuously monitors each chassis
3. **Detecting and handling power faults** - Identifies voltage rail failures and takes corrective action

## Architecture

### Key Components

**Manager Class**
- Top-level class created in `main()`
- Finds and loads JSON configuration file based on system type
- Runs a 1-second timer that calls `System::monitor()` to continuously monitor the system

**System Class**
- Represents the entire computer system controlled by the BMC
- Contains one or more `Chassis` objects
- Manages system-level power state and power good values
- Publishes D-Bus interface at `/org/openbmc/control/power0`
- Coordinates power on/off across multiple chassis

**Chassis Class**
- Represents a physical chassis (enclosure with CPUs, fans, power supplies, PCIe cards)
- Contains one or more `PowerSequencerDevice` objects
- Monitors chassis status (present, available, enabled, input power good)
- Publishes D-Bus interface at `/org/openbmc/control/power<N>` where N is chassis number
- Powers voltage rails on/off via GPIOs to the power sequencer device

**PowerSequencerDevice Classes**
- Hierarchy of classes representing different power sequencer hardware (UCD90160, UCD90320, etc.)
- Enables voltage rails in correct order
- Monitors for power good faults
- Can identify which specific voltage rail caused a fault

## BMC Reset Handling and Results

### What Happens During BMC Reset

When the BMC resets (reboots), the `phosphor-power-sequencer` application:

1. **Restarts from scratch** - The application is launched by systemd when BMC reaches Ready state

2. **Discovers initial state** (in `System::monitor()` and `Chassis::monitor()`):
   - Reads current chassis power good signals from GPIOs
   - Determines which chassis are powered on vs off
   - Sets initial `selectedChassis` based on current power state
   - Infers the last requested power state from current power good values
   - Creates D-Bus interfaces once power state/power good are determined

3. **Key initialization logic** (from `system.cpp`):
   ```cpp
   // Sets initial selected chassis based on which are currently on/off
   setInitialSelectedChassisIfNeeded()

   // Sets system power good based on selected chassis
   setPowerGood(services)

   // Infers power state from power good (if pgood=on, assumes state was on)
   setInitialPowerStateIfNeeded(services)
   ```

4. **Resumes monitoring** - Begins 1-second monitoring cycle to:
   - Track power good changes
   - Detect chassis status changes
   - Check for power good faults

### Results After BMC Reset

**System Remains Powered On:**
- If chassis were powered on before BMC reset, they remain on
- Application detects power good = on, infers power state = on
- Continues monitoring without disrupting running system
- No power cycle occurs

**System Remains Powered Off:**
- If chassis were powered off before BMC reset, they remain off
- Application detects power good = off, infers power state = off
- Ready to accept new power on requests

**Power State Delays:**
- **First power on delay**: 15 seconds after application starts (configurable)
- **Next power on delay**: 25 seconds after power off (configurable)
- **Power off delay**: 2 seconds after power off requested (configurable)
- These delays are enforced in `System::handleDelays()` to prevent rapid power cycling

**Error Recovery:**
- BMC reset clears error history (`hasRequestedDump`, `hasRequestedPowerOff`, `powerGoodFault`)
- Fresh monitoring begins, can detect new faults
- Previous fault state is not preserved across BMC reset

### Critical Design Points

1. **Non-disruptive**: BMC reset does NOT power cycle the system - the hardware state is preserved
2. **State inference**: Application infers previous power state from current hardware state
3. **Continuous monitoring**: Resumes fault detection immediately after initialization
4. **Multiple chassis support**: Handles systems with multiple chassis independently
5. **D-Bus integration**: Publishes `org.openbmc.control.Power` interface for system and each chassis

## Power On Process

1. User/system sets `state` property to 1 on D-Bus interface `/org/openbmc/control/power0`
2. `System::setPowerState()` determines which chassis can be powered on based on status
3. Selected chassis have their `state` property set to 1
4. GPIOs are toggled to power sequencer devices in each chassis
5. Power sequencer devices enable voltage rails in correct order
6. When all rails are on, chassis power good signal goes true
7. Application reads chassis pgood from GPIO and sets `pgood` property to 1
8. When all selected chassis are powered on, system `pgood` property is set to 1

## Power Off Process

1. User/system sets `state` property to 0 on D-Bus interface `/org/openbmc/control/power0`
2. `System::setPowerState()` determines which chassis to power off
3. After power off delay (2 seconds), GPIOs are toggled to power sequencer devices
4. Power sequencer devices disable voltage rails in correct order
5. Chassis power good signal goes false
6. Application reads chassis pgood from GPIO and sets `pgood` property to 0
7. When all selected chassis are powered off, system `pgood` property is set to 0
8. Next power on is blocked for 25 seconds (next power on delay)

## Fault Handling

### Power Good Faults

Two types of power good faults are detected:

**Timeout Faults** (during power on):
- Power on attempt exceeds timeout (default: configurable via PGOOD_TIMEOUT)
- Handled by `phosphor-chassis-state-manager` application
- Results in power off/cycle and BMC dump request

**Non-Timeout Faults** (after successful power on):
- Power good unexpectedly changes from true to false
- Handled by `phosphor-power-sequencer` application
- Error logging delayed 7 seconds to allow hardware failure processing
- Results in:
  - BMC dump creation (`System::createBMCDump()`)
  - Hard power off/cycle (`System::hardPowerOff()`)
  - Single chassis: power off via systemd
  - Multiple chassis: power cycle via systemd

### Invalid Chassis Status

During power on, if a selected chassis status becomes invalid:
- Not present
- No input power
- Not available

Results in:
- BMC dump creation
- Hard power off/cycle

## Monitoring

The `Manager::monitor()` method is called every 1 second and:
1. Calls `System::monitor()` which:
   - Monitors all chassis status via `Chassis::monitor()`
   - Sets initial selected chassis if needed
   - Updates system power good based on chassis power good values
   - Sets initial power state if needed
   - Updates power state transition status
   - Checks for power good faults
   - Checks for invalid chassis status

2. Each `Chassis::monitor()`:
   - Reads power good from power sequencer devices
   - Updates chassis power good value
   - Checks for power good errors
   - Checks for invalid status
   - Reacts to chassis status D-Bus property changes

## Configuration

The application is driven by JSON configuration files located in `phosphor-power-sequencer/config_files/`:
- Defines chassis in the system
- Defines power sequencer devices
- Defines voltage rails
- Defines GPIOs for power control and monitoring
- System-specific files selected based on compatible system types

## Related Applications

- **phosphor-chassis-state-manager**: Handles user power on/off requests, publishes `xyz.openbmc_project.State.Chassis` interface
- **phosphor-chassis-power**: Monitors chassis input power state using GPIOs
- **phosphor-power-supply**: Monitors power supply devices within each chassis

The application is designed to be resilient to BMC resets, maintaining system availability while resuming monitoring and control capabilities.