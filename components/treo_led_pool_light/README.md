# TREO LED Pool Light Component
## Overview
This is a custom light component that works with [TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/) and exposes the different colors as "effects" so thay can be selected from Home Assistant. It also has an option to include a sensor to report current power usage based on a configured wattage of the light(s) it controls.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/esphome
    components: [ treo_led_pool_light ]
```

Add and configure the Binary Light With Power Component
```yaml
light:
  - platform: treo_led_pool_light
    id: my_pool_lights
    name: My Pool Lights
    pin: 15
    power:
      id: my_pool_lights_power
      name: My Pool Lights Power
      light_wattage: 10.0
      update_interval: 60s
```

## Configuration Variables (In addition to the standard variables)
* pin (Required, Pin) The output pin that controls power to the lights
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor
* power.light_wattage (Optional, float) The total wattage of the light(s) controled by this dimmer
* power.update_interval (Optional, Time, default: 60s) Amount of time between updates of the power value while on.

## Operation
It is possible for the color of the lights to get out of sync with each other and/or this component. To resolve this issue this component adds a service named esphome.{device_name}_color_sync_reset that goes through the series of power cycles defined in the lights user guide that will reset all lights and this component back to the slow color change "effect".
