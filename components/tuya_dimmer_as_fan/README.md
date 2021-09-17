# Tuya Dimmer as Fan Component
## Overview
This a modified version of the [Tuya fan](https://esphome.io/components/fan/tuya.html) component I use with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) (but it will likely work with other Tuya dimmers) to control bathroom fans. Changes from the standard Tuya fan component include the following:
* Remove options for speed, oscillation, and direction as they don't apply.
* Always change the "brightness" back to the maximum value effectively making this only an on/off device.
* Can add a sensor to report current power usage based on a configured wattage of the fan(s) it controls.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/esphome
    components: [ tuya_dimmer_as_fan ]
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
      fan_wattage: 21.6
      update_interval: 60s
```

## Configuration Variables (In addition to the standard variables)
* dimmer_datapoint (Required, int): The datapoint id number of the dimmer value.
* dimmer_max_value (Optional, int, default 255): The highest dimmer value allowed.
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor.
* power.fan_wattage (Optional, float) The total wattage of the fan(s) controled by this dimmer.
* power.update_interval (Optional, Time, default: 60s) Amount of time between updates of the power value while on.
