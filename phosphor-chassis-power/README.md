# phosphor-chassis-power

Design document: <https://gerrit.openbmc.org/c/openbmc/docs/+/81614>

## Overview

The `phosphor-chassis-power` application monitors chassis power on multi-chassis
systems to handle chassis-level fault monitoring and recovery. The application
is controlled by a JSON configuration file.

## Implementation Status

All design requirements (R-PCP-2 through R-PCP-7) have been implemented:

- **R-PCP-2**: BMC reset handling with GPIO configuration
- **R-PCP-3**: System boot power fault handling
- **R-PCP-4**: System power off GPIO disable
- **R-PCP-5**: Runtime fault scenario handling
- **R-PCP-6**: PowerSystemInputs status updates
- **R-PCP-7**: Missing sled GPIO handling

See [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) for detailed
implementation information.

## Features

- GPIO-based chassis presence detection
- Power fault monitoring via fault-unlatched and fault-latched GPIOs
- Automatic GPIO enable/disable based on chassis state
- D-Bus monitoring for chassis state changes
- PowerSystemInputs D-Bus interface for fault status
- PEL creation for power faults (ID: 110074F0)
- Support for up to 8 chassis/sleds per system
