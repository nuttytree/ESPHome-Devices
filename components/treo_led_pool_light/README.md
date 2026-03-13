# TREO LED Pool Light Component
## Overview
This is a custom light component that works with [TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/) and exposes the different colors as "effects" so thay can be selected from Home Assistant.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ treo_led_pool_light ]
```

Add and configure the Treo LED Pool Lights
```yaml
light:
  - platform: treo_led_pool_light
    id: my_pool_lights
    name: My Pool Lights
    output: my_pool_lights_output
```

## Configuration Variables (In addition to the standard variables)
* output (Required, ID) The id of the binary Output Component to use for this light.

## Operation
It is possible for the color of the lights to get out of sync with each other and/or this component. To resolve this issue this component adds a service named esphome.{device_name}_color_reset that goes through the series of power cycles defined in the lights user guide that will reset all lights and this component back to the slow color change "effect".
