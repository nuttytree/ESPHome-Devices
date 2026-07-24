# Master Bed

## Overview
This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK) installed under the bed, connected to a pair of [Force Sensitive Resistors](https://www.adafruit.com/product/1071) placed between the mattress and box spring — one per side. Configuration lives in [master-bed.yaml](../../master-bed.yaml), and the sensor logic is implemented in the custom [bed_sensor](../../components/bed_sensor/README.md) component.

## Hardware Notes
The ESP8266 only has one analog input, so transistors on pins D0 and D1 select which FSR is connected to the A0 analog input. The `bed_sensor` component handles switching between the FSRs and reading their values.

## Occupancy Thresholds
- Empty bed: reading close to the full-scale 1024
- One person on their own side: typically ~100
- Someone in the middle of the bed: ~600-700 on both sides

This is why there are 3 binary sensors (Chris is in Bed, Melissa is in Bed, Someone is in Bed) plus a Master Bed Count sensor, which is compared against a Home Assistant sensor tracking how many "masters" are home to trigger night mode once everyone is in bed.

## Photos / Schematics
Wiring schematic: [Schematic.PDF](./Schematic.PDF)

TODO — photo of FSR placement.
