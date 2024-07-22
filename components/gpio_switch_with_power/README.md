# GPIO Switch With Power Component
## Overview
This an enhanced version of the standard [gpio switch](https://esphome.io/components/switch/gpio.html) component that adds an option to include a sensor to report current power usage based on a configured wattage of the device(s) it controls.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ gpio_switch_with_power ]
```

Add and configure the GPIO Switch With Power Component
```yaml
switch:
  - platform: gpio_switch_with_power
    id: my_switch
    name: My Switch
    pin: 4
    power:
      id: my_switch_power
      name: My Switch Power
      device_wattage: 48.0
      update_interval: 60s
```

## Configuration Variables (In addition to the standard variables)
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor
* power.device_wattage (Optional, float) The total wattage of the device(s) controled by this dimmer
* power.update_interval (Optional, Time, default: 60s) Amount of time between updates of the power value while on.
