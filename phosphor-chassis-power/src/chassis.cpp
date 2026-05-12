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

#include "chassis.hpp"

#include "types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <format>

namespace phosphor::power::chassis
{

bool Chassis::initializePowerSystemInputsInterface(sdbusplus::bus_t& bus)
{
    auto chassisInputPowerStatusPath =
        std::format(CHASSIS_INPUT_POWER_STATUS_PATH, number);

    // Create the D-Bus interface object for this chassis
    try
    {
        // Determine initial status based on fault GPIO
        PowerSystemInputs::Status initialStatus =
            PowerSystemInputs::Status::Good;
        if (isPresent() && isFaultUnlatchedActive())
        {
            initialStatus = PowerSystemInputs::Status::Fault;
        }

        powerSystemInputsInterface =
            std::make_unique<ChassisPowerSystemInterface>(
                bus, chassisInputPowerStatusPath.c_str(), initialStatus);
        return true;
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to initialize PowerSystemInputs interface for chassis {CHASSIS}: {EXCEPTION}",
            "CHASSIS", number, "EXCEPTION", e.what());
        return false;
    }
}

bool Chassis::isPresent() const
{
    // Check if presence path exists
    if (presencePath.has_value())
    {
        return std::filesystem::exists(presencePath.value());
    }

    // Check presence GPIO if available
    const Gpio* presenceGpio =
        findGpioByName("presence-chassis" + std::to_string(number));
    if (presenceGpio != nullptr)
    {
        int value = readGpioInput(presenceGpio->getName());
        // GPIO is active when value matches polarity expectation
        bool isActive = (presenceGpio->getPolarity() == GpioPolarity::Low)
                            ? (value == 0)
                            : (value == 1);
        return isActive;
    }

    // If no presence detection method available, assume present
    return true;
}

bool Chassis::isPoweredOn() const
{
    // Chassis is powered on if fault-unlatched GPIO is NOT active
    return !isFaultUnlatchedActive();
}

bool Chassis::isFaultUnlatchedActive() const
{
    // Find fault-unlatched GPIO
    std::string gpioName =
        std::format("power-chs{}-sb-fault-unlatched", number);
    const Gpio* faultGpio = findGpioByName(gpioName);

    if (faultGpio == nullptr)
    {
        // If GPIO not found, assume no fault
        return false;
    }

    try
    {
        int value = readGpioInput(faultGpio->getName());
        // GPIO is active when value matches polarity expectation
        bool isActive = (faultGpio->getPolarity() == GpioPolarity::Low)
                            ? (value == 0)
                            : (value == 1);
        return isActive;
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to read fault-unlatched GPIO for chassis {CHASSIS}: {EXCEPTION}",
            "CHASSIS", number, "EXCEPTION", e.what());
        return false;
    }
}

const Gpio* Chassis::findGpioByName(const std::string& name) const
{
    for (const auto& gpio : gpios)
    {
        if (gpio->getName() == name)
        {
            return gpio.get();
        }
    }
    return nullptr;
}

void Chassis::setGpioOutput(const std::string& gpioName, bool enable)
{
    const Gpio* gpio = findGpioByName(gpioName);
    if (gpio == nullptr)
    {
        lg2::warning("GPIO {GPIO} not found for chassis {CHASSIS}", "GPIO",
                     gpioName, "CHASSIS", number);
        return;
    }

    if (gpio->getDirection() != GpioDirection::Output)
    {
        lg2::error("GPIO {GPIO} is not an output for chassis {CHASSIS}", "GPIO",
                   gpioName, "CHASSIS", number);
        return;
    }

    try
    {
        // Use GPIO object's write method
        const_cast<Gpio*>(gpio)->write(enable);
        lg2::info("Set GPIO {GPIO} (enable={ENABLE}) for chassis {CHASSIS}",
                  "GPIO", gpioName, "ENABLE", enable, "CHASSIS", number);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to set GPIO {GPIO} for chassis {CHASSIS}: {EXCEPTION}",
            "GPIO", gpioName, "CHASSIS", number, "EXCEPTION", e.what());
    }
}

int Chassis::readGpioInput(const std::string& gpioName) const
{
    const Gpio* gpio = findGpioByName(gpioName);
    if (gpio == nullptr)
    {
        lg2::warning("GPIO {GPIO} not found for chassis {CHASSIS}", "GPIO",
                     gpioName, "CHASSIS", number);
        return 0;
    }

    try
    {
        // Use GPIO object's read method
        return gpio->read();
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to read GPIO {GPIO} for chassis {CHASSIS}: {EXCEPTION}",
            "GPIO", gpioName, "CHASSIS", number, "EXCEPTION", e.what());
        return 0;
    }
}

} // namespace phosphor::power::chassis
