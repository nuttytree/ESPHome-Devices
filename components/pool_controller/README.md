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
* **state_save_interval** (Optional, Time, default: `5min`): How often each pump's operating state is written to flash so a restart can resume where it left off. This bounds how much accumulated runtime a sudden power loss can lose. See [Restart Recovery](#restart-recovery).
* **max_resume_age** (Optional, Time, default: `15min`): How stale a saved state may be and still be resumed. Beyond this the outage is treated as extended, the saved state is discarded, and pumps start cold. See [Restart Recovery](#restart-recovery).
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
* **current_sensor** (Optional, id): ID of a sensor reporting current draw in amps. Required when `on_anomaly` is configured. Turning the switch on always publishes an on state and energizes the output; current then confirms whether the motor is actually turning, which drives the anomaly startup window, the no-current sensor, and the flow-loss watchdog. It also accumulates runtime, but only when there is no `flow_sensor` — see [Runtime Accounting](#runtime-accounting).
* **current_on_threshold** (Optional, float, default: `0.5`): Current in amps above which the pump is considered running (used whenever `current_sensor` is configured).
* **no_current_binary_sensor** (Optional): Problem binary sensor, automatically added whenever `current_sensor` is configured. Turns on whenever the switch is commanded on but no current is detected (e.g. a manual override switch has the pump physically off). Suppressed while the current reading is stale — see [Stale Current Readings](#stale-current-readings). Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> No Current`.
* **current_timeout** (Optional, Time, default: `30s`): How long `current_sensor` may go without publishing before its last value stops being treated as evidence. See [Stale Current Readings](#stale-current-readings).
* **current_stale_binary_sensor** (Optional): Problem binary sensor, automatically added whenever `current_sensor` is configured. Turns on when the current sensor has published nothing for `current_timeout`, meaning its readings are frozen rather than merely low. Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> Current Sensor Stale`.
* **anomaly_detection_switch** (Optional): A Switch entity, created whenever `current_sensor` is configured, that enables or disables anomaly detection at runtime. Supports all standard [ESPHome Switch options](https://esphome.io/components/switch/index.html); restores to on by default and defaults to `<pump name> Anomaly Detection`.
* **anomaly_status_binary_sensor** (Optional): Problem binary sensor, created whenever `current_sensor` is configured, that turns on when a current-draw anomaly is currently active. Supports all standard [ESPHome Binary Sensor options](https://esphome.io/components/binary_sensor/index.html); defaults to `<pump name> Anomaly Status`.
* **anomaly_baseline_reset_button** (Optional): A Button entity, created whenever `current_sensor` is configured, that discards the learned current baseline (steady state, drift, variance, startup inrush) and clears any latched anomaly. The press is never refused and can be made at any time; relearning does not start immediately but at the pump's next turn-on, so the new baseline is always built from a complete run — see [Baseline Reset](#baseline-reset). Supports all standard [ESPHome Button options](https://esphome.io/components/button/index.html); defaults to `<pump name> Reset Anomaly Baseline`.
* **anomaly_reason_text_sensor** (Optional): Text sensor, created whenever `current_sensor` is configured, publishing the reason string of the most recent anomaly — lets a single `anomaly_status_binary_sensor` on-state be disambiguated in the UI/history. Supports all standard [ESPHome Text Sensor options](https://esphome.io/components/text_sensor/index.html); defaults to `<pump name> Anomaly Reason`.
* **anomaly_threshold_pct** (Optional, int 1–100, default: `10`): Percentage deviation from baseline required to trigger an anomaly event.
* **learning_samples** (Optional, int 10–10000, default: `200`): Number of steady-state current samples to collect before the baseline is locked.
* **on_anomaly** (Optional, automation): Automation triggered when an anomaly is detected. The trigger variable `x` is a string describing the anomaly: `CURRENT_HIGH`, `CURRENT_LOW`, `NO_STARTUP_SPIKE`, `BASELINE_DRIFT` (either direction — rising suggests bearing wear, falling suggests prime/flow loss), or `VARIANCE_SPIKE` (current spread has grown well beyond its learned baseline, often the earliest sign of trouble).
* **flow_sensor** (Optional, id): ID of a binary sensor that detects water flow. Whenever it is configured it owns runtime accounting, current sensor or not — see [Runtime Accounting](#runtime-accounting). If a `current_sensor` is also configured, the pump is additionally shut down when current confirms the motor is running but flow is lost for longer than `flow_timeout`. Without a `current_sensor`, no-flow alone never shuts the pump down — a manual override switch physically holding the pump off would otherwise look identical to a real no-flow fault. Flow also gates anomaly statistics — see [Current-based Anomaly Detection](#current-based-anomaly-detection).
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

### Restart Recovery
`millis()` restarts at zero on boot and says nothing about how long the device was down, so without persisted state a restart looks identical to a cold first start. Each pump therefore writes a small timestamped snapshot — accumulated runtime, the hour slot that runtime belongs to, and when it last stopped — every `state_save_interval` and on every start and stop.

On the first tick after the clock is valid, and before any schedule decision is made, each pump applies its snapshot:

* **Snapshot older than `max_resume_age`** — treated as an extended outage. It is discarded and the pump starts cold: zero runtime and a full minimum off-time from boot.
* **Runtime** carries over only when the snapshot belongs to the hour slot we are now in. If a slot boundary passed while the device was down, the runtime resets to zero — that is the hourly reset nobody was running to perform. Without it, a stale count would be measured against the new slot's target and could suppress the run for that whole hour.
* **Minimum off-time.** If the pump was running when the snapshot was taken, the stop was the restart itself rather than the end of a duty cycle, so there is nothing to protect against and the schedule may bring it straight back up. If it was already off, the off-time it has served — including the outage, which the wall clock covers — is credited against the minimum, and only the remainder is waited out.

The snapshot always includes the in-flight portion of a run in progress, so an unexpected restart loses at most `state_save_interval` worth of runtime rather than the entire run.

#### Flash Wear
Snapshots are committed to flash immediately rather than being left to the global `preferences: flash_write_interval`. That interval is meant for chatty components; leaving these writes to it would let an unrelated global setting decide how much runtime a restart can recover. Writes are instead bounded by `state_save_interval` plus pump start/stop events — at the 5-minute default, under 300 writes a day, which is comfortably within the endurance of the wear-levelled NVS partition. Raise `state_save_interval` to trade resume accuracy for fewer writes.

### Pump Disable Sensor
When `disable_pumps_sensor` is active (on), all pumps are turned off immediately and no pump is allowed to turn on until the sensor clears. This is useful for wiring in an external interlock (e.g. a cover sensor or maintenance switch).

### Current-based Anomaly Detection
When enabled, the component learns the pump's normal steady-state current draw over `learning_samples` samples. After learning is complete, any reading that deviates by more than `anomaly_threshold_pct` percent triggers the `on_anomaly` automation with a string describing the event. A separate startup-inrush baseline is maintained to avoid false positives during the motor startup window.

Each run is split into a 15 s startup window, during which the inrush peak is captured, and a steady-state phase, which feeds the baseline while learning and is compared against it afterwards.

#### What Counts as a Valid Sample
Current confirms the motor is energized, but not that the pump is doing useful work, so a `flow_sensor` — when configured — decides whether a sample means anything:

* **Steady-state samples** are only taken while flow is present. A pump that is spinning but moving no water draws an atypical current that would otherwise be learned as normal or misread as a current anomaly; the flow-loss watchdog is what reports that condition. The out-of-band debounce streak is dropped across a no-flow gap so it can never span one.
* **The startup inrush peak** is only folded into the startup reference if flow was established at some point during the startup window. Flow is never required *at* the moment of inrush — a pump needs a moment to prime — but a start that never moved water is discarded (logged as a warning) rather than averaged in, and cannot raise `NO_STARTUP_SPIKE` either.

With no `flow_sensor` configured there is nothing better to go on, so current alone gates capture, exactly as before.

#### Baseline Reset
Pressing `anomaly_baseline_reset_button` discards everything learned — steady-state EMA, drift EMA, variance accumulators, startup inrush reference and run count — clears the latched anomaly status and the alert cooldown, and persists the cleared baseline immediately.

Capture is then *armed rather than started*. Nothing is sampled until the pump next turns on, at which point learning begins with the startup window still ahead of it. This holds no matter when the button is pressed: a run already in progress has no startup window left to observe, and sampling it would produce a baseline with no inrush reference and a steady-state figure taken from a partially-observed run. The cost is that a pump running continuously (the `Always` schedule, with nothing to cycle it off) relearns nothing until its next off/on cycle.

Relearning then follows the normal path: the first run seeds the startup reference, `NO_STARTUP_SPIKE` checks resume once three starts have accumulated, and the steady-state baseline locks after `learning_samples` valid samples.

### Runtime Accounting
Two different questions get asked about a running pump, and they want different answers:

* **How long did the pump do useful work?** Runtime, which the fractional-runtime scheduler spends against `minutes_per_hour`. Moving water is what a pool schedule is actually buying, so this is measured from `flow_sensor` whenever one is configured, falling back to `current_sensor`, and finally to the output command. Flow is also the more robust source: a current sensor that stalls while reading `0 A` would otherwise stop the clock on a pump that is running perfectly well.
* **When was the motor energised?** The anchor for the anomaly startup window, the auxiliary `sequence_delay`, and the heater's warm-up gate. This is measured from `current_sensor` whenever one is configured, because flow lags the motor by a second or more — easily long enough to miss the inrush peak the startup check exists to measure.

The two are tracked independently. Between the output being commanded on and the first sensor confirming the motor, the turn-on timestamp reads as "no run time yet" rather than as an elapsed time, so nothing that gates on it fires early.

### Stale Current Readings
An ESPHome sensor keeps serving its last value indefinitely when its source stops responding — there is no outward difference between a live reading and a frozen one. A meter that has gone silent while reporting `0 A` is byte-identical to a pump that genuinely isn't drawing current, and everything downstream believes it: the no-current sensor reports a fault that isn't happening, runtime stops accumulating for a pump that is actually running, and anomaly detection measures a flat line.

Each pump therefore timestamps every publish from its `current_sensor`. If nothing arrives for `current_timeout`, the reading is declared stale and:

* `current_stale_binary_sensor` turns on, so the real fault — a dead sensor — is visible and distinguishable in history from a pump fault.
* `motor_running` is no longer recomputed. The last known state is held rather than inferred from a frozen number, so runtime tracking neither invents a stop nor invents a start.
* `no_current_binary_sensor` is suppressed. "Commanded on but drawing nothing" is a claim about the pump, and a sensor that stopped publishing supports no such claim.
* Anomaly detection stops sampling, since every sample would be the same stale number.

When readings resume, the first value is a step change away from whatever was frozen — potentially a very large one. Feeding that straight into the detector reads as an enormous deviation and sets the variance trip chattering while the estimate re-converges, so the out-of-band debounce streak and the live spread estimate are both cleared on recovery. A run whose startup window contained a gap also has its inrush peak discarded (logged as a warning) rather than averaged into the startup reference, since the window was only partly observed.

Note that the flow-loss watchdog still acts on the held `motor_running` state. That is deliberate: dry-running is the expensive failure, so a stale current reading should not disarm the protection that stops it.

### Flow Sensor Protection
If a `flow_sensor` is configured and reports no flow for longer than `flow_timeout` while the pump output is commanded on, the pump is shut down to protect against dry-running or blockage.
