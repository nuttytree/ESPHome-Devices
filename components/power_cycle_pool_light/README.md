# Power Cycle Pool Light Component

## Overview
Many LED pool lights have no data connection at all: they are wired to a plain relay and advance to the next color in a fixed list every time power is briefly interrupted. That makes them awkward to control from Home Assistant, because "turn on" and "pick a color" are the same physical action and nothing reports back which color is currently showing.

This component wraps a binary output and turns that behavior into a normal ESPHome light: each color is exposed as a selectable "effect", the current color is tracked (and persisted across reboots), and asking for a color power cycles the output as many times as needed to walk the light to it.

I built and use this with [SR Smith TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/), but the colors, the reset sequence, and the timings are all configurable, so it should work with any light that changes color this way — Moov pool lights, Pentair IntelliBrite / GloBrite / MicroBrite, J&J Electronics ColorSplash (now owned by Hayward), Jandy / Zodiac WaterColors, and similar. Check your light's user guide for its color order and reset procedure, then fill in the configuration below.

## Setup
Using the [External Components](https://esphome.io/components/external_components.html) feature in ESPHome you can add this component to your devices directly from my GitHub repo.
```yaml
external_components:
  - source: github://nuttytree/ESPHome-Devices
    components: [ power_cycle_pool_light ]
```

The color reset service requires custom services to be enabled on the API:
```yaml
api:
  custom_services: True
```

Add and configure the light (the values below are the ones I use for TREO lights):
```yaml
light:
  - platform: power_cycle_pool_light
    id: pool_lights
    name: "Pool Lights"
    output: pool_lights_relay
    colors:
      - "Slow Change"
      - "White"
      - "Blue"
      - "Green"
      - "Red"
      - "Amber"
      - "Magenta"
      - "Fast Change"
    min_off_time: 5.5s
    reset_sequence:
      - state: "off"
        duration: 5.5s
      - state: "on"
        duration: 250ms
      - state: "off"
        duration: 250ms
      - state: "on"
        duration: 250ms
      - state: "off"
        duration: 250ms
      - state: "on"
        duration: 250ms
      - state: "off"
        duration: 5.5s
```

## Configuration Variables
Accepts all standard [ESPHome Binary Light options](https://esphome.io/components/light/binary.html) plus:
* **output** (Required, id): The ID of the binary output that switches power to the light.
* **colors** (Required, list of strings): The names of the colors in the order the light cycles through them. The first entry must be the color the light shows after the reset sequence, since that is the color the component assumes the light is on once a reset completes. Each color becomes a selectable effect on the light, so these names are what you see in Home Assistant. At least two colors are required and the names must be unique.
* **reset_sequence** (Required, list): The power sequence from the light's user guide that puts it back on the first color. Each step holds the output in one state for a duration, and the steps run in order:
  * **state** (Required, boolean): `"on"` or `"off"` — quote them so YAML doesn't fold them into `true`/`false` differently than you intend.
  * **duration** (Required, Time): How long to hold that state before moving to the next step.
* **min_off_time** (Required, Time): How long power has to be off before the light treats it as a real power off rather than a color change. The component will not switch the output back on until this much time has passed since it went off, so quickly turning the light off and back on won't leave you on an unexpected color. Consult your light's user guide; for TREO lights `5.5s` matches the dwell used by the documented reset procedure.
* **color_change_time** (Optional, Time, default: `200ms`): How long the output is held off, and then held on, for each step of a color change.
* **effects** (Not allowed): The effects list is generated from `colors`. Setting an `effects:` key raises a config validation error.

## Operation
**Changing colors.** Selecting an effect walks the light forward to that color by pulsing the output off and on (`color_change_time` in each state) once for each color between the current one and the target, wrapping around the end of the list. Turning the light on without selecting an effect leaves it on the color it was already showing.

**Turning on and off.** Turning the light off switches the output off immediately. Turning it back on within `min_off_time` is deferred until that window has elapsed — the light still comes on, just a moment later, and on the same color it was showing before.

**Color reset.** Because the component can only infer the color from the power cycles it performs, the tracked color can drift from what the light is actually showing (a power outage, a tripped breaker, or someone switching the circuit at the panel will all do it), and lights sharing a relay can end up out of sync with each other. This component adds a service named `esphome.{device_name}_color_reset` that runs the configured `reset_sequence` to put every light back on the first color, then steps the light back to the color it is supposed to be showing.
