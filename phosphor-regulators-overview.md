# Overview of phosphor-regulators Application

## Purpose
The `phosphor-regulators` application configures and monitors voltage regulators in OpenBMC systems. It is controlled by a JSON configuration file that defines regulator devices, rails, and monitoring parameters.

## Key Architecture Components

### Class Hierarchy
1. **Manager** - Top-level class that:
   - Loads JSON configuration files
   - Implements D-Bus `configure` and `monitor` methods
   - Handles SIGHUP signals for config reload
   - Contains a System object

2. **System** - Represents the computer system:
   - Contains one or more Chassis objects
   - Coordinates configuration and monitoring across all chassis

3. **Chassis** - Represents a physical enclosure:
   - Can be powered on/off by BMC
   - Contains Device objects

4. **Device** - Represents hardware (voltage regulators, I/O expanders):
   - Contains Rail objects

5. **Rail** - Represents a voltage rail (e.g., 1.1V output)

### D-Bus Interface
- **Service Name**: `xyz.openbmc_project.Power.Regulators`
- **Object Path**: `/xyz/openbmc_project/power/regulators/manager`
- **Methods**:
  - `Configure()` - Configures all voltage regulators
  - `Monitor(bool enable)` - Enables/disables monitoring

## BMC Reset Handling and Results

### During BMC Reset/Reboot

1. **Service Restart**:
   - The systemd service has `Restart=on-failure` policy
   - Service is Type=dbus, so it's managed by D-Bus activation
   - On BMC reset, the application restarts automatically

2. **Initialization Sequence** (in Manager constructor):
   ```
   - Finds compatible system types for current system
   - Attempts to load JSON configuration file
   - Requests D-Bus service name
   - If system is already powered on, enables monitoring automatically
   ```

3. **SIGHUP Handler**:
   - Application registers a SIGHUP signal handler
   - When SIGHUP is received, it reloads the JSON configuration file
   - This allows runtime config updates without full restart

### Configuration Phase (Boot Time)

**Triggered by**: `phosphor-regulators-config.service` during system boot

**Process**:
1. Clears cached hardware data (presence, VPD, device cache, error history)
2. Waits for config file to load (max 5 minutes)
3. Calls `system->configure()` to configure all regulators
4. Configuration happens **before** regulators are enabled/turned on
5. Only configures chassis that are:
   - Present
   - Enabled (if interface exists)
   - Available (if interface exists)

**Error Handling**:
- Errors are logged but don't stop boot
- Remaining regulators still get configured
- Critical errors logged if config file not loaded

### Monitoring Phase (After Power On)

**Enabled by**: `phosphor-regulators-monitor-enable.service` after regulators are on

**Process**:
1. Sets `isMonitoringEnabled = true`
2. Starts two timers:
   - Phase fault detection: 15-second intervals
   - Sensor monitoring: 1-second intervals
3. Enables sensors service (puts sensors in active state)
4. Only monitors chassis that are:
   - Present
   - pgood = 1 (powered on)
   - Available (if interface exists)

**Disabled by**: `phosphor-regulators-monitor-disable.service` during shutdown

### Results After BMC Reset

1. **Configuration State**:
   - All cached data is cleared
   - Configuration must be reapplied via `configure()` method
   - Hardware state is re-discovered

2. **Monitoring State**:
   - If system is powered on when BMC comes up, monitoring auto-enables
   - Timers restart and monitoring resumes
   - Sensor data collection continues

3. **Error History**:
   - Previous error history is cleared
   - Fresh monitoring cycle begins

4. **Config File**:
   - Reloaded from filesystem
   - Can be updated and reloaded via SIGHUP without full restart

### Key Design Features

- **Stateless on Reset**: Clears all cached data on configuration
- **Auto-Recovery**: Automatically resumes monitoring if system is powered on
- **Graceful Degradation**: Errors don't prevent other regulators from being configured
- **Runtime Reload**: SIGHUP allows config updates without service restart
- **Multi-Chassis Support**: Handles systems with multiple chassis/drawers

## File Locations

- **Source Code**: `phosphor-regulators/src/`
- **Configuration Files**: `phosphor-regulators/config_files/`
- **Documentation**: `phosphor-regulators/docs/`
- **Service File**: `services/phosphor-regulators.service`
- **Standard Config Directory**: `/usr/share/phosphor-regulators/`
- **Test Config Directory**: `/etc/phosphor-regulators/`

## Related Documentation

- [Configuration Documentation](phosphor-regulators/docs/configuration.md)
- [Monitoring Documentation](phosphor-regulators/docs/monitoring.md)
- [Internal Design](phosphor-regulators/docs/internal_design.md)
- [Config File Format](phosphor-regulators/docs/config_file/README.md)