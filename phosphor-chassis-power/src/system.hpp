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
#pragma once

#include "chassis.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phosphor::power::chassis
{

/**
 * @class System
 *
 * The computer system being controlled and monitored by the BMC.
 *
 * The system contains one or more chassis.
 */
class System
{
  public:
    System() = delete;
    System(const System&) = delete;
    System(System&&) = delete;
    System& operator=(const System&) = delete;
    System& operator=(System&&) = delete;
    ~System() = default;

    /**
     * Constructor.
     *
     * @param chassis Chassis in the system
     */
    explicit System(std::vector<std::unique_ptr<Chassis>> chassis) :
        chassis{std::move(chassis)}
    {}

    /**
     * Returns the chassis in the system.
     *
     * @return chassis
     */
    const std::vector<std::unique_ptr<Chassis>>& getChassis() const
    {
        return chassis;
    }

    /**
     * Initializes each chassis power system inputs status to be good.
     *
     * @param bus D-Bus bus object
     */
    void initializePowerSystemInputs(sdbusplus::bus_t& bus);

    /**
     * Handle BMC reset - R-PCP-2.
     *
     * After BMC reset, configure GPIOs based on chassis presence and power
     * state:
     * - For missing sleds: disable reset-enable and fault-reset GPIOs
     * - For present sleds:
     *   - If fault-unlatched active: disable GPIOs, set Status=Fault
     *   - If sled off: disable GPIOs
     *   - If sled on: enable GPIOs
     */
    void handleBmcReset();

    /**
     * Handle chassis state change - R-PCP-3, R-PCP-4, R-PCP-5, R-PCP-7.
     *
     * Called when a chassis state property changes on D-Bus.
     *
     * @param chassisNumber chassis number that changed
     * @param propertyName name of the property that changed
     * @param value new property value
     */
    void handleChassisStateChange(unsigned int chassisNumber,
                                  const std::string& propertyName,
                                  const std::string& value);

    /**
     * Monitor chassis state changes via D-Bus.
     *
     * Sets up D-Bus matches to monitor chassis state property changes.
     *
     * @param bus D-Bus bus object
     */
    void monitorChassisStateChanges(sdbusplus::bus_t& bus);

    /**
     * Disable GPIOs for a chassis.
     *
     * Disables reset-enable and fault-reset GPIOs.
     *
     * @param chassis chassis to disable GPIOs for
     */
    void disableChassisGpios(Chassis& chassis);

    /**
     * Enable GPIOs for a chassis.
     *
     * Enables reset-enable and fault-reset GPIOs.
     *
     * @param chassis chassis to enable GPIOs for
     */
    void enableChassisGpios(Chassis& chassis);

    /**
     * Create PEL for power fault.
     *
     * Creates PEL with ID 110074F0 for power fault scenarios.
     *
     * @param chassisNumber chassis number with fault
     * @param message fault message
     */
    void createPowerFaultPEL(unsigned int chassisNumber,
                             const std::string& message);

  private:
    /**
     * Chassis in the system.
     */
    std::vector<std::unique_ptr<Chassis>> chassis{};

    /**
     * D-Bus matches for monitoring chassis state changes.
     */
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>> stateMatches{};
};

} // namespace phosphor::power::chassis
