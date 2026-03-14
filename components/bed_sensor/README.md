# Bed Sensor
## Overview
The Bed Sensor component detects occupancy for each side of a bed using resistive pressure sensors connected to an ADC input. It alternates power between two sides using binary outputs, reads the ADC value after each switch, and publishes per-side occupancy, a combined "someone in bed" sensor, an occupancy count, and a status text sensor.

## Hardware Setup
Each side of the bed needs a resistive pressure sensor wired between a GPIO output pin and a shared analog input pin. The component drives each side's output high in turn, reads the ADC value, then drives it low before switching to the other side.

## Example YAML Configuration
```yaml
output:
  - platform: gpio
    id: bed_side_one_output
    pin: GPIO16

  - platform: gpio
    id: bed_side_two_output
    pin: GPIO17

sensor:
  - platform: adc
    id: bed_adc
    pin: A0
    raw: true
    update_interval: never

bed_sensor:
  id: my_bed
  adc_sensor: bed_adc
  update_interval: 2s
  side_one:
    status_name: "Person One"
    output: bed_side_one_output
    name: "Bed Side One Occupied"
    value_sensor:
      name: "Bed Side One Value"
  side_two:
    status_name: "Person Two"
    output: bed_side_two_output
    name: "Bed Side Two Occupied"
    value_sensor:
      name: "Bed Side Two Value"
  someone_sensor:
    status_name: "Someone"
    name: "Someone In Bed"
  count_sensor:
    name: "Bed Occupancy Count"
  status_sensor:
    name: "Bed Status"
```

## Configuration Variables
* **id** (Optional, string): Manually specify the component ID used for code generation.
* **adc_sensor** (Required, id): The ID of an ADC sensor used to read the pressure values. The ADC sensor should have `update_interval: never` so that the bed sensor controls when it is read and `raw: true` to get the raw values instead of the voltage.
* **update_interval** (Optional, Time, default: `2s`): How often the bed sensor alternates sides and updates occupancy state.
* **side_one** (Required): Configuration for side one of the bed.
  * **status_name** (Required, string): The name used in the status text sensor for this side (e.g. "Alice").
  * **output** (Required, id): The ID of a binary output used to power the pressure sensor on this side.
  * **name** / other binary sensor options: Standard binary sensor configuration for the occupancy sensor.
  * **value_sensor**: Sub-sensor reporting the raw ADC value for this side. Supports standard sensor options.
* **side_two** (Required): Configuration for side two of the bed. Same options as `side_one`.
* **someone_sensor** (Required): Binary sensor that is `true` when someone is in the bed but in the middle not one side or the other.
  * **status_name** (Required, string): The name used in the status text sensor for this state.
  * Other standard binary sensor options.
* **count_sensor** (Required): Numeric sensor reporting the total number of occupants detected (0–3).
* **status_sensor** (Required): Text sensor reporting a human-readable occupancy status (e.g. "Empty", "Alice", "Alice and Bob").
