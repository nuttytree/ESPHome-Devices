# Tuya Light Plus Component
## Overview
This a modified version of the Tuya fan component I use with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) (but it will likely work with other Tuya dimmers) to control bathroom fans. The major change from the standard Tuya fan component (other than removing options for speed, oscillation, and direction) is adding a function to always change the dimmer back to the maximum "brightness" effectively making this only an on/off device. Similar to the Tuya Light Plus component this component can also add a power sensor based on configured wattage of the fan, this could be done with a templat sensor and automations but it was easy to add here so I figured why not.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.  Note currently this component requires pulling in my custom version of the Tuya component as well to prevent communication issues between the ESP8266 and the Tuya MCU.
```yaml
external_components:
  - source: github://nuttytree/esphome
    components: [ tuya, tuya_dimmer_as_fan ]
```

Like the standard Tuya fan component you need to have the [UART](https://esphome.io/components/uart.html) and [Tuya](https://esphome.io/components/tuya.html) components.
```yaml
uart:
  rx_pin: GPIO3
  tx_pin: GPIO1
  baud_rate: 9600

tuya:
```

Add and configure the Tuya Dimmer as Fan component
```yaml
fan:
  - platform: tuya_dimmer_as_fan
    name: my_fan
    switch_datapoint: 1
    dimmer_datapoint: 2
    dimmer_max_value: 1000
    power:
      id: my_fan_power
      name: My Fan Power
      light_wattage: 21.6
```

## Configuration Variables
* id (Optional, ID): Manually specify the ID used for code generation.
* name (Required, string): The name of the light.
* switch_datapoint (Required, int): The datapoint id number of the power switch.
* dimmer_datapoint (Required, int): The datapoint id number of the dimmer value.
* dimmer_max_value (Optional, int, default 255): The highest dimmer value allowed.
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor.
* power.fan_wattage (Optional, float) The total wattage of the fan(s) controled by this dimmer.
