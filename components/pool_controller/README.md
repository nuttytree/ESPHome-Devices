# Pool Controller

## Overview
Note: This component is a work in progress. I created this component to automate my pool equipment — a primary circulation pump, an auxiliary cleaner pump, and an optional pool heater. The core idea is schedule-based pump control with a fractional runtime model: instead of running a pump continuously for a long window, you configure a `minutes_per_hour` value so the pump cycles on and off proportionally within each hour-long slot. The primary and auxiliary pumps are sequenced so the primary always starts first and stops last, protecting the heater and cleaner from running without water flow. An optional current sensor enables anomaly detection that can fire automations when the pump draws significantly more or less current than its learned baseline.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ pool_controller ]
```

Add and configure the Pool Controller:
```yaml
pool_controller:
  primary_pump:
    name: "Pool Pump"
    id: pool_pump
    output: pump_output
    restore_mode: Always Off
    schedule_select:
      id: pump_schedule_select
      name: Pump Schedule
    schedules:
      - name: Normal
        runtimes:
          - start_time: '4:00'
            end_time: '6:00'
            minutes_per_hour: 60
          - start_time: '6:00'
            end_time: '18:00'
            minutes_per_hour: 40
            days_of_week: MON-FRI
          - start_time: '18:00'
            end_time: '22:00'
            minutes_per_hour: 20
            days_of_week: SAT,SUN
  auxiliary_pumps:
    - name: "Pool Cleaner"
      id: pool_cleaner
      output: cleaner_output
      restore_mode: Always Off
      schedule_select:
        id: cleaner_schedule_select
        name: Cleaner Schedule
      schedules:
        - name: Normal
          runtimes:
            - start_time: '4:00'
              end_time: '5:00'
              minutes_per_hour: 60
  pool_heater:
    name: "Pool Heater"
    id: pool_heater
    output: heater_output
    temperature_sensor: water_temp_sensor
```

## Configuration Variables

### Top-level `pool_controller`
* **primary_pump** (Required): Configuration for the primary circulation pump. See [Primary Pump](#primary-pump) below.
* **auxiliary_pumps** (Optional, list): Zero or more auxiliary pumps (e.g. cleaner, fill valve). See [Auxiliary Pump](#auxiliary-pump) below.
* **sequence_delay** (Optional, Time, default: `2s`): How long to wait between primary and auxiliary pump state changes during sequenced startup and shutdown.
* **disable_pumps_sensor** (Optional, id): ID of a binary sensor that, when active, immediately shuts all pumps off and prevents any pump from turning on.
* **pool_heater** (Optional): Configuration for an optional pool heater. See [Pool Heater](#pool-heater) below.

### Primary Pump
Accepts all standard [ESPHome Switch options](https://esphome.io/components/switch/index.html) plus:
* **output** (Required, id): The ID of a binary output that physically switches the pump relay.
* **schedule_select** (Required): A Select entity whose options mirror the configured schedules. Supports all standard [ESPHome Select options](https://esphome.io/components/select/index.html). The built-in options are `Off` (always first) and `Always` (always last); user-defined schedules appear in between.
* **schedules** (Optional, list): Named schedules available for selection. Each schedule has:
  * **name** (Required, string): Unique display name shown in the schedule select entity.
  * **runtimes** (Required, list): One or more time windows. Each runtime has:
    * **start_time** (Required, time): Window start on the hour (e.g. `'6:00'`).
    * **end_time** (Required, time): Window end on the hour; use `'24:00'` for midnight end-of-day.
    * **minutes_per_hour** (Required, int, 1–60): How many minutes out of each 60 the pump should run within this window.
    * **days_of_week** (Optional): Days this runtime applies to (e.g. `MON-FRI`, `SAT,SUN`). Defaults to every day.
* **current_sensor** (Optional, id): ID of a sensor reporting current draw in amps. Required when `on_anomaly` is configured. When set, turning the switch on always publishes an on state and energizes the output, but the pump's runtime is only accumulated while current confirms the motor is actually running.
* **current_on_threshold** (Optional, float, default: `0.5`): Current in amps above which the pump is considered running (used whenever `current_sensor` is configured).
* **no_current_binary_sensor** (Optional): Problem binary sensor, automatically added whenever `current_sensor` is configured. Turns on whenever the switch is commanded on but no current is detected (e.g. a manual override switch has the pump physically off). Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> No Current`.
* **anomaly_detection_switch** (Optional): A Switch entity, created whenever `current_sensor` is configured, that enables or disables anomaly detection at runtime. Supports all standard [ESPHome Switch options](https://esphome.io/components/switch/index.html); restores to on by default and defaults to `<pump name> Anomaly Detection`.
* **anomaly_status_binary_sensor** (Optional): Problem binary sensor, created whenever `current_sensor` is configured, that turns on when a current-draw anomaly is currently active. Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> Anomaly Status`.
* **anomaly_baseline_reset_button** (Optional): A Button entity, created whenever `current_sensor` is configured, that discards the learned current baseline and begins relearning when pressed. Ignored (with a warning logged) while `flow_loss_binary_sensor` is active, so a reset can't lock in a baseline contaminated by an in-progress flow-loss fault. Supports all standard [ESPHome Button options](https://esphome.io/components/button/index.html); defaults to `<pump name> Reset Anomaly Baseline`.
* **anomaly_reason_text_sensor** (Optional): Text sensor, created whenever `current_sensor` is configured, publishing the reason string of the most recent anomaly — lets a single `anomaly_status_binary_sensor` on-state be disambiguated in the UI/history. Supports all standard [ESPHome Text Sensor options](https://esphome.io/components/text_sensor/index.html); defaults to `<pump name> Anomaly Reason`.
* **anomaly_threshold_pct** (Optional, int 1–100, default: `10`): Percentage deviation from baseline required to trigger an anomaly event.
* **learning_samples** (Optional, int 10–10000, default: `200`): Number of steady-state current samples to collect before the baseline is locked.
* **on_anomaly** (Optional, automation): Automation triggered when an anomaly is detected. The trigger variable `x` is a string describing the anomaly: `CURRENT_HIGH`, `CURRENT_LOW`, `NO_STARTUP_SPIKE`, `BASELINE_DRIFT` (either direction — rising suggests bearing wear, falling suggests prime/flow loss), or `VARIANCE_SPIKE` (current spread has grown well beyond its learned baseline, often the earliest sign of trouble).
* **flow_sensor** (Optional, id): ID of a binary sensor that detects water flow. If no `current_sensor` is configured, the pump's runtime is only accumulated while flow is actually detected. If a `current_sensor` is configured, runtime is controlled by current instead (see above), and the pump is additionally shut down if current confirms the motor is running but flow is lost for longer than `flow_timeout`. Without a `current_sensor`, no-flow alone never shuts the pump down — a manual override switch physically holding the pump off would otherwise look identical to a real no-flow fault.
* **flow_timeout** (Optional, Time, default: `2s`): How long flow must be absent (while a current sensor confirms the motor is running) before the pump is shut down.
* **flow_loss_binary_sensor** (Optional): Problem binary sensor, automatically added whenever both `current_sensor` and `flow_sensor` are configured. Latches on when the pump is shut down due to lost flow, and only clears once the pump is on, current confirms the motor is running, and flow is detected again. Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> Flow Loss`.
* **unexpected_flow_binary_sensor** (Optional): Problem binary sensor, automatically added whenever `flow_sensor` is configured. Turns on when flow is detected while the pump is commanded off for longer than `flow_timeout` (e.g. a stuck valve, a neighboring pump pushing water through this branch, or a manual override running the pump), and clears as soon as flow stops or the pump turns on. Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> Unexpected Flow`.

### Auxiliary Pump
Accepts the same options as [Primary Pump](#primary-pump) above. The built-in last schedule option for an auxiliary pump is `When <primary pump name> is Running` rather than `Always`.

### Pool Heater
Accepts all standard [ESPHome Water Heater options](https://esphome.io/components/water_heater/) plus:
* **output** (Required, id): The ID of a binary output connected to the heater relay.
* **temperature_sensor** (Required, id): ID of a sensor reporting the current water temperature. The component uses this to drive bang-bang control.
* **deadband** (Optional, Temperature Delta, default: `0.5 °F`): Degrees below the target temperature at which the heater turns on.
* **overrun** (Optional, Temperature Delta, default: `0.5 °F`): Degrees above the target temperature at which the heater turns off.

## Operation

### Schedule-based Fractional Runtime
Each hour-long slot is evaluated independently. Within a slot the pump runs for `minutes_per_hour` minutes, evenly distributed. This lets you approximate variable-speed pump behavior with a single-speed pump by reducing run time during lower-demand periods.

### Sequenced Startup and Shutdown
When starting, the primary pump turns on first and auxiliary pumps wait for `sequence_delay` before turning on. When stopping, auxiliary pumps turn off immediately and the primary pump follows after `sequence_delay`. If a pool heater is configured it is turned off before the primary pump during shutdown to avoid running the heater without water flow.

### Pump Disable Sensor
When `disable_pumps_sensor` is active (on), all pumps are turned off immediately and no pump is allowed to turn on until the sensor clears. This is useful for wiring in an external interlock (e.g. a cover sensor or maintenance switch).

### Current-based Anomaly Detection
When enabled, the component learns the pump's normal steady-state current draw over `learning_samples` samples. After learning is complete, any reading that deviates by more than `anomaly_threshold_pct` percent triggers the `on_anomaly` automation with a string describing the event. A separate startup-inrush baseline is maintained to avoid false positives during the motor startup window.

### Flow Sensor Protection
If a `flow_sensor` is configured and reports no flow for longer than `flow_timeout` while the pump output is commanded on, the pump is shut down to protect against dry-running or blockage.
