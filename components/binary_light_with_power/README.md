# Binary Light With Power Component
## Overview
This an enhanced version of the standard [binary light](https://esphome.io/components/light/binary.html) component that adds an option to include a sensor to report current power usage based on a configured wattage of the light(s) it controls.


## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/esphome
    components: [ binary_light_with_power ]
```

Like the standard binary light component you need to have an [output](https://esphome.io/components/output.html).
```yaml
output:
  - platform: gpio
    id: my_light_output
    pin: 13
```

Add and configure the Binary Light With Power Component
```yaml
light:
  - platform: binary_light_with_power
    id: my_light
    name: My Light
    output: my_light_output
    power:
      id: my_light_power
      name: My Light Power
      light_wattage: 48.0
```

## Added Configuration Variables
* power.id (Optional, string) Manually specify the power sensor ID used for code generation.
* power.name (Optional, string) The name for the power sensor
* power.light_wattage (Optional, float) The total wattage of the light(s) controled by this dimmer
