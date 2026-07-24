# HVAC

## Overview
The house HVAC (a Rheem furnace + air conditioner) is a 3-zone system (Main Floor, Upstairs, Basement) integrated with Home Assistant via the [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) component, plus a custom [Econet Zone Control](../../components/econet_zone_control/README.md) component. Configuration lives in [hvac.yaml](../../hvac.yaml).

## Why a Custom Component
Rather than 3 independent thermostats, the goals were:
- A single climate entity in Home Assistant controlling all zones at once, sharing one target temperature
- Active balancing of airflow/dampers across zones when idle (the main floor tends to run hot in winter, the basement runs cold in summer), while still deferring to the furnace/AC's own zone control while actively heating or cooling
- The ability to change mode/temperature from the physical thermostat on the main floor and have it reflected in Home Assistant

See the [Econet Zone Control README](../../components/econet_zone_control/README.md) for the full configuration schema.

## Photos / Schematics
TODO — equipment photos, damper/zone board wiring, thermostat photo.

## History / Notes
TODO
