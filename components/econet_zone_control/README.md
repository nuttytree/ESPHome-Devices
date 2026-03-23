# Econet Zone Control

## Overview
I created this component for my Rheem furnace/air conditoner with 3 zones that I am controlling with the amazing [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) component.  The biggest reason for this component is because for a variety of reasons my main floor zone tends to run hot in the winter and my basement runs cold in the summer.  When actively heating or cooling I want the system to manage fan speeds/dampers but when idle I want to control the fan speeds/dampers to try and balance the temperatures across zones.  I also want all the zones to always be set to the same temperature, a single climate device in Home Assistant to control all zones, and I want to be able to change modes and temperature from the physical thermostat on the main floor.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ econet_zone_control ]
```

The component depends on the [esphome-econet](https://github.com/esphome-econet/esphome-econet) component being present and configured. Add and configure the climate entity:

```yaml
climate:
  - platform: econet_zone_control
    name: My All Zones Thermostat
    econet_id: econet_id
    request_mod: 10
    src_address: 0x380
    operating_mode_datapoint: HVACMODE
    mode_datapoint: STATMODE
    modes:
      0: heat
      1: cool
      2: heat_cool
      3: fan_only
      4: "off"
    current_temperature_datapoint: SPT
    target_temperature_low_datapoint: HEATSETP
    target_temperature_high_datapoint: COOLSETP
    fan_mode_datapoint: STAT_FAN
    fan_mode_no_schedule_datapoint: STATNFAN
    current_humidity_datapoint: RELH7005
    zones:
      - src_address: 0x380
        request_mod: 10
        is_primary: true
      - src_address: 0x680
        request_mod: 11
      - src_address: 0x681
        request_mod: 12
    automatic_fan_mode: 0
    fan_modes:
      - fan_mode: 1
        minimum_temperature_delta: 0°F
      - fan_mode: 2
        minimum_temperature_delta: 0.4°F
      - fan_mode: 3
        minimum_temperature_delta: 0.8°F
      - fan_mode: 4
        minimum_temperature_delta: 1.2°F
    visual:
      min_temperature: 60°F
      max_temperature: 80°F
      temperature_step:
        current_temperature: 0.1°F
        target_temperature: 1°F
```

## Configuration Variables (In addition to the [standard ESPHome climate options](https://esphome.io/components/climate/index.html) and the [ECONET_CLIENT_SCHEMA](https://github.com/esphome-econet/esphome-econet) fields)
* **operating_mode_datapoint** (Required, string): Econet datapoint ID reporting the current operating mode string (e.g. `HVACMODE`). Used to derive the HA action (heating/cooling/idle/fan).
* **mode_datapoint** (Required, string): Econet enum datapoint ID for the thermostat mode (e.g. `STATMODE`).
* **modes** (Required, map): Mapping of enum integer values to ESPHome climate modes (`heat`, `cool`, `heat_cool`, `fan_only`, `"off"`).
* **zones** (Required, list): List of zone entries. At least 2 zones, exactly one must have `is_primary: true`. Each zone has:
  * **src_address** (Required, uint32): Econet source address of this zone's thermostat.
  * **request_mod** (Required, int): Request modifier for this zone's polling.
  * **is_primary** (Optional, boolean, default: false): Designates this zone as primary. Exactly one zone must be primary. The primary zone drives the HA state and is the target of all `control()` writes.
* **automatic_fan_mode** (Required, uint8): Enum value to write when setting a zone to automatic fan control.
* **fan_modes** (Required, list): Fan mode entries for idle/fan temperature-balancing logic. Each entry has:
  * **fan_mode** (Required, uint8): Enum value to write to the thermostat for this fan speed.
  * **minimum_temperature_delta** (Required, Temperature Delta): Minimum spread between the hottest and coldest zone required to activate this speed. During idle/fan action the component finds the hottest and coldest zones, computes their spread, and selects the highest entry whose `minimum_temperature_delta` is still ≤ the spread. That speed is applied only to the hottest and coldest zones; all others are set to `automatic_fan_mode`. While actively heating or cooling all zones are set to `automatic_fan_mode`.
* **current_temperature_datapoint** (Optional, string, default: ""): Econet datapoint for current temperature (°F). Averaged across all zones for the HA state.
* **target_temperature_low_datapoint** (Optional, string, default: ""): Econet datapoint for the heat setpoint (°F).
* **target_temperature_high_datapoint** (Optional, string, default: ""): Econet datapoint for the cool setpoint (°F).
* **fan_mode_datapoint** (Optional, string, default: ""): Econet enum datapoint for fan mode when following a schedule (e.g. `STAT_FAN`).
* **fan_mode_no_schedule_datapoint** (Optional, string, default: ""): Econet enum datapoint for fan mode when not following a schedule (e.g. `STATNFAN`). Both `fan_mode_datapoint` and `fan_mode_no_schedule_datapoint` are written simultaneously to ensure the setting takes effect regardless of schedule state.
* **current_humidity_datapoint** (Optional, string, default: ""): Econet datapoint for current humidity (%). Averaged across all zones. Omit to disable humidity reporting.

