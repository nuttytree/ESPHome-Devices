# Econet Zones Climate

## Overview
I created this component for my Rheem furnace/air conditoner with 3 zones that I am controlling with the amazing [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) component.  The biggest reason for this component is because for a variety of reasons my main floor zone tends to run hot in the winter and my basement runs cold in the summer.  When actively heating or cooling I want the system to manage fan speeds/dampers but when idle I want to control the fan speeds/dampers to try and balance the temperatures across zones.  I also want all the zones to always be set to the same temperature, a single climate device in Home Assistant to control all zones, and I want to be able to change modes and temperature from the physical thermostats.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ econet_zones_climate ]
```


Add and configure the Econet Zones Climate Component
```yaml
climate:
  - platform: econet_zones_climate
    name: "My Zoned Climate"
    operating_mode_sensor: operating_mode
    zones:
      - zone1_climate
      - zone2_climate
      - zone3_climate
    automatic_fan_mode: Automatic
    fan_modes:
      - fan_mode: Speed 1 (Low)
        minimum_temperature_delta: 0°F
      - fan_mode: Speed 2 (Medium Low)
        minimum_temperature_delta: .5°F
      - fan_mode: Speed 3 (Medium)
        minimum_temperature_delta: 1.0°F
```

## Configuration Variables (In addition to the standard variables)
* **operating_mode_sensor** (Required, TextSensor): The ID of the text sensor that reports the current operating mode.
* **zones** (Required, List of Climate): List of at least 2 climate entities, one for each zone.
* **automatic_fan_mode** (Required, string): The name of the fan mode to use for automatic balancing.
* **fan_modes** (Required, List): List of fan modes and their minimum temperature deltas.
  * **minimum_temperature_delta** (Required, temperature delta): The minimum temperature difference required to activate this fan mode.
  * **fan_mode** (Required, string): The name of the fan mode.
* **supports_heat_mode** (Optional, boolean, default: true): Whether to support heat mode.
* **supports_cool_mode** (Optional, boolean, default: true): Whether to support cool mode.
* **supports_current_humidity** (Optional, boolean, default: true): Whether to support reporting current humidity.
* **supports_target_humidity** (Optional, boolean, default: true): Whether to support setting target humidity.
