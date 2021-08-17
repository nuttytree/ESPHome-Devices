# Tuya Light Plus Component
## Overview
This an enhanced version of the standard [Tuya light](https://esphome.io/components/light/tuya.html) component that adds a bunch of extra features. I use this component with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) but it will likely work with other Tuya dimmers. Extra features include the following:
* Resets the brightness level back to a default level when turned off so that it always comes on at the same level instead of the level it was at when turned off.
* The default level can be different during the "day" vs at "night" when everyone is in bed.  Day and Night are based on a sensor (binary or text)in Home Assistant.
* The default level can be updated via a service that is added to Home Assistant by this component.
* Provides an option to automatically turn off the light after a specified period of time.
* The automatic turn off time can be different during the "day" vs at "night" when everyone is in bed.  Day and Night are based on a sensor (binary or text)in Home Assistant.
* The automatic turn off time can be updated via a service that is added to Home Assistant by this component.
* Adds an option to configure action(s) to run when the dimmer is double clicked while off.
* Double clicking the dimmer while off can be configured to leave the light in an off or on state.
* Adds an option to configure action(s) to run when the dimmer is double clicked while on (this double click always turns the light off otherwise you get strange flash when double clicking).
* Allows you to "link" other light(s) in Home Assistant that will be controlled by this dimmer (on/off and level).
* Can add a sensor to report current power usage based on a configured wattage of the light(s) it controls. Currently this reports the specified wattage regardless of the dimmer level (my lights run at the max level 95% of the time so for me this is pretty accurate).  Eventually I want to determine approximately what the dimmer level to power reduction ratio is so that it can more accurately report the power.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.  Note currently this component requires pulling in my custom version of the Tuya component as well to prevent communication issues between the ESP8266 and the Tuya MCU.
```yaml
external_components:
  - source: github://nuttytree/esphome
    components: [ tuya, tuya_light_plus ]
```

Like the standard Tuya Light component you need to have the [UART](https://esphome.io/components/uart.html) and [Tuya](https://esphome.io/components/tuya.html) components.
```yaml
uart:
  rx_pin: GPIO3
  tx_pin: GPIO1
  baud_rate: 9600

tuya:
```

Add and configure the Tuya Light Plus component
```yaml
light:
  - platform: tuya_light_plus
    name: My Light
    switch_datapoint: 1
    dimmer_datapoint: 2
    max_value: 1000
    linked_lights:
      - light.my_linked_light
    day_night:
      sensor_id: sensor.day_night
      sensor_type: text
      sensor_day_value: Day
      sensor_night_value: Night
      day_default_brightness: 255
      night_default_brightness: 1
      day_auto_off_time: 0 min
      night_auto_off_time: 15 min
    on_double_click_while_off:
      - script.execute: double_click
    double_click_while_off_stays_off: false
    on_double_click_while_on:
      - script.execute: double_click
    power:
      id: my_light_power
      name: My Light Power
      light_wattage: 21.6

```

## Added Configuration Variables
* default_brightness (Optional, int 1-255): The default brightness level for the light.
* auto_off_time (Optional, Time): The amount of time to wait before automatically turning the light off, 0 disables auto off
* linked_lights (Optional, string): List of lights that will be controlled by this dimmer (note this one direction, changes to the linked light will not be applied to this light).
* day_night.sensor_id (Optional, string): Entity Id of a sensor in Home Assistant that indicates day/night mode.
* day_night.sensor_type (Optional, binary or text, default: binary): The type of the sensor in Home Assistant that indicates day/night mode.
* day_night.sensor_day_value (Optional, string, default: "on" for binary sensors and "Day" for text): The value of the sensor that indicates day mode.
* day_night.sensor_night_value (Optional, string, default: "off" for binary sensors and "Night" for text): The value of the sensor that indicates night mode.
* day_night.day_default_brightness (Optional, int 1-255): The default brightness level for the light during day mode.
* day_night.night_default_brightness (Optional, int 1-255): The default brightness level for the light during night mode.
* day_night.day_auto_off_time (Optional, Time): The amount of time to wait before automatically turning the light off during day mode, 0 disables auto off
* day_night.night_auto_off_time (Optional, Time): The amount of time to wait before automatically turning the light off during night mode, 0 disables auto off
* on_double_click_while_off (Optional): List of actions to run when the dimmer is double clicked while off
* double_click_while_off_stays_off (Optional, bool, default: true): Determines if the light remains off or turns on after a double click while off
* on_double_click_while_on (Optional): List of actions to run when the dimmer is double clicked while on
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor
* power.light_wattage (Optional, float) The total wattage of the light(s) controled by this dimmer

## Operation
This component adds 2 services to Home Assistant that can be used to update the settings of the dimmer:
* esphome.{device_name}_set_auto_off_time - This service has a single parameter "auto_off_time" that is in milliseconds, 0 will disable the auto off function. Note this value will get updated when the day/night mode changes if that option is configured.
* esphome.{device_name}_set_default_brightness - This service has a single parameter "brightness" (valid values 0 - 255), 0 will disable the default brightness feature. Note this value will get updated when the day/night mode changes if that option is configured.
