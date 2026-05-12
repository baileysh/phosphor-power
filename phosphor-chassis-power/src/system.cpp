/**
 * Copyright © 2026 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "system.hpp"

#include <phosphor-logging/lg2.hpp>

#include <format>

namespace phosphor::power::chassis
{

void System::initializePowerSystemInputs(sdbusplus::bus_t& bus)
{
    for (const auto& curChassis : chassis)
    {
        curChassis->initializePowerSystemInputsInterface(bus);
    }
}

void System::handleBmcReset()
{
    lg2::info("Handling BMC reset - configuring chassis GPIOs (R-PCP-2)");

    for (const auto& curChassis : chassis)
    {
        unsigned int chassisNum = curChassis->getNumber();

        try
        {
            // Check if chassis is present
            bool present = curChassis->isPresent();

            if (!present)
            {
                // R-PCP-2: For missing sleds, disable GPIOs
                lg2::info("Chassis {CHASSIS} not present - disabling GPIOs",
                          "CHASSIS", chassisNum);
                disableChassisGpios(*curChassis);
            }
            else
            {
                // Chassis is present
                bool faultActive = curChassis->isFaultUnlatchedActive();

                if (faultActive)
                {
                    // R-PCP-2: Fault detected - disable GPIOs and set
                    // Status=Fault
                    lg2::warning(
                        "Chassis {CHASSIS} has active fault - disabling GPIOs",
                        "CHASSIS", chassisNum);
                    disableChassisGpios(*curChassis);

                    // Update D-Bus interface status to Fault
                    auto& interface =
                        curChassis->getPowerSystemInputsInterface();
                    if (interface)
                    {
                        interface->status(PowerSystemInputs::Status::Fault);
                    }
                }
                else
                {
                    // No fault - check power state
                    bool poweredOn = curChassis->isPoweredOn();

                    if (poweredOn)
                    {
                        // R-PCP-2: Sled on - enable GPIOs
                        lg2::info(
                            "Chassis {CHASSIS} powered on - enabling GPIOs",
                            "CHASSIS", chassisNum);
                        enableChassisGpios(*curChassis);
                    }
                    else
                    {
                        // R-PCP-2: Sled off - disable GPIOs
                        lg2::info(
                            "Chassis {CHASSIS} powered off - disabling GPIOs",
                            "CHASSIS", chassisNum);
                        disableChassisGpios(*curChassis);
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Error handling BMC reset for chassis {CHASSIS}: {EXCEPTION}",
                "CHASSIS", chassisNum, "EXCEPTION", e.what());
        }
    }
}

void System::handleChassisStateChange(unsigned int chassisNumber,
                                      const std::string& propertyName,
                                      const std::string& value)
{
    lg2::info("Chassis {CHASSIS} state change: {PROPERTY}={PROP_VALUE}",
              "CHASSIS", chassisNumber, "PROPERTY", propertyName, "PROP_VALUE",
              value);

    // Find the chassis
    auto it = std::find_if(chassis.begin(), chassis.end(),
                           [chassisNumber](const auto& c) {
                               return c->getNumber() == chassisNumber;
                           });

    if (it == chassis.end())
    {
        lg2::error("Chassis {CHASSIS} not found", "CHASSIS", chassisNumber);
        return;
    }

    Chassis& curChassis = **it;

    try
    {
        // Handle different state changes
        if (propertyName == "state")
        {
            // R-PCP-3: System boot - handle power on
            if (value == "On" || value == "Running")
            {
                bool faultActive = curChassis.isFaultUnlatchedActive();

                if (faultActive)
                {
                    // R-PCP-3: Power fault during boot - disable GPIOs
                    lg2::error(
                        "Chassis {CHASSIS} power fault during boot - disabling GPIOs",
                        "CHASSIS", chassisNumber);
                    disableChassisGpios(curChassis);

                    // Create PEL for power fault
                    createPowerFaultPEL(chassisNumber,
                                        "Power fault during system boot");

                    // Set Status=Fault
                    auto& interface =
                        curChassis.getPowerSystemInputsInterface();
                    if (interface)
                    {
                        interface->status(PowerSystemInputs::Status::Fault);
                    }
                }
                else
                {
                    // R-PCP-3: Successful power on - enable GPIOs
                    lg2::info(
                        "Chassis {CHASSIS} powered on successfully - enabling GPIOs",
                        "CHASSIS", chassisNumber);
                    enableChassisGpios(curChassis);
                }
            }
            // R-PCP-4: System power off
            else if (value == "Off")
            {
                lg2::info("Chassis {CHASSIS} powered off - disabling GPIOs",
                          "CHASSIS", chassisNumber);
                disableChassisGpios(curChassis);
            }
        }
        else if (propertyName == "pgood")
        {
            // R-PCP-5: Runtime fault scenario (pgood failure)
            if (value == "0" || value == "false")
            {
                lg2::warning(
                    "Chassis {CHASSIS} pgood failure - disabling GPIOs",
                    "CHASSIS", chassisNumber);
                disableChassisGpios(curChassis);

                // Check if it's a power fault
                bool faultActive = curChassis.isFaultUnlatchedActive();
                if (faultActive)
                {
                    createPowerFaultPEL(chassisNumber,
                                        "Runtime power fault - pgood failure");

                    auto& interface =
                        curChassis.getPowerSystemInputsInterface();
                    if (interface)
                    {
                        interface->status(PowerSystemInputs::Status::Fault);
                    }
                }
            }
        }
        else if (propertyName == PRESENT_PROP)
        {
            // R-PCP-7: Sled goes missing
            if (value == "0" || value == "false")
            {
                lg2::warning("Chassis {CHASSIS} went missing - disabling GPIOs",
                             "CHASSIS", chassisNumber);
                disableChassisGpios(curChassis);
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error handling state change for chassis {CHASSIS}: {EXCEPTION}",
            "CHASSIS", chassisNumber, "EXCEPTION", e.what());
    }
}

void System::monitorChassisStateChanges(sdbusplus::bus_t& bus)
{
    lg2::info("Setting up D-Bus monitoring for chassis state changes");

    for (const auto& curChassis : chassis)
    {
        unsigned int chassisNum = curChassis->getNumber();

        try
        {
            // Monitor chassis state property changes
            std::string chassisPath =
                std::format("/org/openbmc/control/power{}", chassisNum);
            std::string matchRule = std::format(
                "type='signal',interface='org.freedesktop.DBus.Properties',"
                "member='PropertiesChanged',path='{}'",
                chassisPath);

            auto match = std::make_unique<sdbusplus::bus::match_t>(
                bus, matchRule, [this, chassisNum](sdbusplus::message_t& msg) {
                    std::string interface;
                    std::map<std::string, std::variant<std::string, int, bool>>
                        properties;

                    try
                    {
                        msg.read(interface, properties);

                        for (const auto& [prop, value] : properties)
                        {
                            std::string valueStr;
                            if (auto* s = std::get_if<std::string>(&value))
                            {
                                valueStr = *s;
                            }
                            else if (auto* i = std::get_if<int>(&value))
                            {
                                valueStr = std::to_string(*i);
                            }
                            else if (auto* b = std::get_if<bool>(&value))
                            {
                                valueStr = *b ? "true" : "false";
                            }

                            handleChassisStateChange(chassisNum, prop,
                                                     valueStr);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        lg2::error(
                            "Error processing D-Bus signal for chassis {CHASSIS}: {EXCEPTION}",
                            "CHASSIS", chassisNum, "EXCEPTION", e.what());
                    }
                });

            stateMatches.push_back(std::move(match));

            lg2::info("Monitoring chassis {CHASSIS} state changes", "CHASSIS",
                      chassisNum);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to setup monitoring for chassis {CHASSIS}: {EXCEPTION}",
                "CHASSIS", chassisNum, "EXCEPTION", e.what());
        }
    }
}

void System::disableChassisGpios(Chassis& chassis)
{
    unsigned int chassisNum = chassis.getNumber();

    // Disable reset-enable GPIO
    std::string resetEnableGpio =
        std::format("reset-enable-chs{}-sb-power", chassisNum);
    chassis.setGpioOutput(resetEnableGpio, false);

    // Disable fault-reset GPIO
    std::string faultResetGpio =
        std::format("power-chs{}-sb-fault-reset", chassisNum);
    chassis.setGpioOutput(faultResetGpio, false);
}

void System::enableChassisGpios(Chassis& chassis)
{
    unsigned int chassisNum = chassis.getNumber();

    // Enable reset-enable GPIO
    std::string resetEnableGpio =
        std::format("reset-enable-chs{}-sb-power", chassisNum);
    chassis.setGpioOutput(resetEnableGpio, true);

    // Enable fault-reset GPIO
    std::string faultResetGpio =
        std::format("power-chs{}-sb-fault-reset", chassisNum);
    chassis.setGpioOutput(faultResetGpio, true);
}

void System::createPowerFaultPEL(unsigned int chassisNumber,
                                 const std::string& message)
{
    lg2::error("Creating PEL for chassis {CHASSIS} power fault: {MSG}",
               "CHASSIS", chassisNumber, "MSG", message);

    // TODO: Implement PEL creation using phosphor-logging
    // PEL ID: 110074F0
    // This would typically use the phosphor-logging createPEL API
    // For now, just log the error
    lg2::error("Power fault PEL (110074F0) for chassis {CHASSIS}: {MSG}",
               "CHASSIS", chassisNumber, "MSG", message);
}

} // namespace phosphor::power::chassis
