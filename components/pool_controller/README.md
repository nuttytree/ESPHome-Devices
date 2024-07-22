# Pool Controller Component
## Overview
This is component is curently running on a [Shelly 2.5 Double Relay Switch](https://shelly.cloud/products/shelly-25-smart-home-automation-relay/) and is used to control the main pump and the auxiliary pump (that runs a pool cleaner) on my pool. Eventually I want to expand this to run on an ESP32 and manage all aspects of my pool (pumps, lights, heat, fill, drain, pH, ORP, etc.).  Right now it is providing the followong features:
* Provides different modes/schedules for the main pump: Off, Normal (on a schedule), Always Except Peak (all day except during the peak electric rate times), and Always
* Provides different modes/schedules for the auxilary (cleaner) pump: Off, Normal (2 hours in the morning), and When Pump Is On (anytime the main pump is running)
* Prevents the auxillary pump from running unless the main pump has been running for at least 5 seconds to prevent sucking chlorine into the auxillary pump
* Prevents the main pump from shutting down unless the auxillary pump has been off for at least 5 seconds to prevent sucking chlorine into the auxillary pump
* Prevents quick cycling of the pumps
* Doesn't automatically turn the pumps on unless they have been off for at least 5 minutes so that you can turn them off during a scheduled run time to empty filters, etc without having them come right back on
* Monitors the electric current of the pumps and shuts them down if they are out of the normal range (which likely indicates an issue with the pump or a valve that was accidentally left closed)


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ pool_controller ]
```

Add and configure the Pool Controller Component
```yaml
pool_controller:
  pump:
    switch_id: pool_pump
    current_id: pool_pump_current
    min_current: 6.50
    max_current: 6.70
    max_out_of_range_duration: 5s
  cleaner:
    switch_id: pool_cleaner
    current_id: pool_cleaner_current
    min_current: 4.55
    max_current: 5.10
    max_out_of_range_duration: 5s

```

## Configuration Variables
* pump.switch_id (Required, Switch) The ID of the switch controlling the main pump
* pump.current_id (Required, Sensor) The ID of the current sensor for the main pump
* pump.min_current (Required, float) The minimum expected current of the main pump
* pump.max_current (Required, float) The maximum expected current of the main pump
* pump.max_out_of_range_duration (Required, Time) The maximum time the current of the main pump can be out of range before shutting off the pump
* cleaner.switch_id (Required, Switch) The ID of the switch controlling the cleaner pump
* cleaner.current_id (Required, Sensor) The ID of the current sensor for the cleaner pump
* cleaner.min_current (Required, float) The minimum expected current of the cleaner pump
* cleaner.max_current (Required, float) The maximum expected current of the cleaner pump
* cleaner.max_out_of_range_duration (Required, Time) The maximum time the current of the cleaner pump can be out of range before shutting off the pump
