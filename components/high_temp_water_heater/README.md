# High Temp Water Heater

## Overview
I created this component because my Rheem water heater that I am controlling with the amazing [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) component only allows setting the temperature above 140 °F from the control panel.  I am on a tiered electric rate so I want to be able to set the water heater to 140 °F during peak rates and 150 °F the rest of the time.  This component wraps around the Econet water heater component.  If the target temperature matches the physical setting it simply lets the water heater manage itself.  However if the target temperature is lower than the physical setting it maintains that lower temperature by turning the water heater on and off as needed.  I found that internally my water heater is determining when to run based on the lower tank temperature (the reported temperature seems to match the upper tank temperature) and that it tries to keep the lower tank temperature between target_temp - 15 and target_temp - 10 which is where I am getting the offset and dead band values below.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ high_temp_water_heater ]
```

Add and configure the High Temp Water Heater Component
```yaml
water_heater:
  - platform: high_temp_water_heater
    name: "My Water Heater"
    source_water_heater: econet_water_heater
    temperature_sensor: lower_tank_temp
    temperature_sensor_offset: "10 °F"
    dead_band: "5 °F"
    over_run: "0 °F"
    min_temperature: "110 °F"
    max_temperature: "150 °F"
    target_temperature_step: 1.0
```

## Configuration Variables (In addition to the standard variables)
* **source_water_heater** (Required, WaterHeater): The ID of the physical water heater
* **temperature_sensor** (Optional, Sensor): The temperature sensor to use instead of the temperature reported by the source_water_heater
* **temperature_sensor_offset** (Optiona, Temperature Delta): Degrees below the target temperature to target getting the temperature_sensor to (required if temperature_sensor is provided)
* **dead_band** (Required, Temperature Delta): Degrees below the target at which the water heater is turned on
* **over_run** (Required, Temperature Delta): Degrees above the target at which the water heater is turned off
* **min_temperature** (Optional, Temperature, default: 110 °F): The minimum temperature the water heater can get set to
* **max_temperature** (Optional, Temperature, default: 150 °F): The maximum temperature the water heater can get set to
* **target_temperature_step** (Optional, float, default: 1.0): The temperature steps shown in the frontend
