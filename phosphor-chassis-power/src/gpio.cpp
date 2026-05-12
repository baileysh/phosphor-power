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

#include "gpio.hpp"

#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <stdexcept>

namespace phosphor::power::chassis
{

int Gpio::read() const
{
    if (direction != GpioDirection::Input)
    {
        lg2::error("Attempting to read from output GPIO {GPIO_NAME}",
                   "GPIO_NAME", name);
        if (defaultValue.has_value())
        {
            return defaultValue.value();
        }
        return 0;
    }

    try
    {
        // Open GPIO chip and line
        ::gpiod::chip chip{name, ::gpiod::chip::OPEN_BY_NAME};
        ::gpiod::line line = chip.get_line(0);

        // Request line as input
        line.request({"phosphor-chassis-power",
                      ::gpiod::line_request::DIRECTION_INPUT, 0});

        // Read value
        int value = line.get_value();
        return value;
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to read GPIO {GPIO_NAME}: {EXCEPTION}", "GPIO_NAME",
                   name, "EXCEPTION", e.what());

        // Return default value if available
        if (defaultValue.has_value())
        {
            return defaultValue.value();
        }
        return 0;
    }
}

void Gpio::write(bool enable)
{
    if (direction != GpioDirection::Output)
    {
        lg2::error("Attempting to write to input GPIO {GPIO_NAME}", "GPIO_NAME",
                   name);
        return;
    }

    try
    {
        // Determine the value to write based on polarity and enable flag
        int value;
        if (polarity == GpioPolarity::Low)
        {
            // Active low: enable=true means write 0, enable=false means write 1
            value = enable ? 0 : 1;
        }
        else
        {
            // Active high: enable=true means write 1, enable=false means write
            // 0
            value = enable ? 1 : 0;
        }

        // Open GPIO chip and line
        ::gpiod::chip chip{name, ::gpiod::chip::OPEN_BY_NAME};
        ::gpiod::line line = chip.get_line(0);

        // Request line as output and set value
        line.request({"phosphor-chassis-power",
                      ::gpiod::line_request::DIRECTION_OUTPUT, 0},
                     value);

        lg2::info("Set GPIO {GPIO_NAME} to {GPIO_VALUE} (enable={ENABLE})",
                  "GPIO_NAME", name, "GPIO_VALUE", value, "ENABLE", enable);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to write GPIO {GPIO_NAME}: {EXCEPTION}", "GPIO_NAME",
                   name, "EXCEPTION", e.what());
    }
}

} // namespace phosphor::power::chassis
