# PZEM-6L24 Three-Phase Energy Monitor

## Overview
The PZEM-6L24 is a three-phase AC energy monitor from Peacefair. It is similar to the single-phase PZEM-004T V3 (which ESPHome supports natively via the `pzemac` component) but measures all three phases simultaneously. It communicates over RS-485 using Modbus RTU and reports voltage, current, active power, reactive power, apparent power, power factor, and energy for each phase individually as well as combined totals. This is a native ESPHome component with no external library dependencies.

## Hardware Setup
The PZEM-6L24 exposes two physical interfaces and you can use either one:

### Option 1 — Direct UART (TTL header on the PCB)
The board has a 4-pin header labeled **GND / TX / RX / +5V**. This is the simplest connection when the ESP is close to the meter.

```
PZEM-6L24 GND ──── ESP GND
PZEM-6L24 TX  ──── ESP RX pin
PZEM-6L24 RX  ──── ESP TX pin
```

> **Note:** The TX and RX signal lines operate at **3.3 V** logic, so they connect directly to any ESP32 or ESP8266 GPIO without a level shifter. The PZEM-6L24 is self-powered from its AC inputs, so the +5V pin on the UART header is not needed.

### Option 2 — RS-485 (screw terminals on the meter)
Use a MAX485 or MAX3485 module when the ESP is further away from the meter, when the meter is on a shared RS-485 bus with multiple devices, or when you want proper differential signalling for noise immunity.

```
PZEM-6L24 A+ ──── MAX485 A
PZEM-6L24 B- ──── MAX485 B
MAX485 VCC   ──── 5 V supply
MAX485 GND   ──── GND
MAX485 DI    ──── ESP TX pin
MAX485 RO    ──── ESP RX pin
MAX485 DE    ──┐
MAX485 RE    ──┘── GND  (tie low for permanent receive mode)
```

If you need software control of the DE/RE direction pin instead of tying it low, set `flow_control_pin` on the `modbus` component.

---

The default baud rate is **9600 8N1** for both connection methods. When using RS-485 with multiple devices on the same bus, each device must have a unique Modbus address (1–247). Use the DIP switches on the meter for hardware addressing or configure a software address through the device's settings before integrating it into ESPHome.

## Example YAML Configuration
```yaml
external_components:
  - source:
      type: local
      path: components
    components: [pzem6l24]

uart:
  id: modbus_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

modbus:
  uart_id: modbus_uart

sensor:
  - platform: pzem6l24
    id: pzem
    address: 0x01
    update_interval: 10s

    # Per-phase voltage
    voltage_a:
      name: "Voltage Phase A"
    voltage_b:
      name: "Voltage Phase B"
    voltage_c:
      name: "Voltage Phase C"

    # Per-phase current
    current_a:
      name: "Current Phase A"
    current_b:
      name: "Current Phase B"
    current_c:
      name: "Current Phase C"

    # Per-phase active power
    active_power_a:
      name: "Active Power Phase A"
    active_power_b:
      name: "Active Power Phase B"
    active_power_c:
      name: "Active Power Phase C"

    # Per-phase power factor
    power_factor_a:
      name: "Power Factor Phase A"
    power_factor_b:
      name: "Power Factor Phase B"
    power_factor_c:
      name: "Power Factor Phase C"

    # Per-phase energy
    active_energy_a:
      name: "Energy Phase A"
    active_energy_b:
      name: "Energy Phase B"
    active_energy_c:
      name: "Energy Phase C"

    # Combined totals
    frequency:
      name: "Grid Frequency"
    total_active_power:
      name: "Total Active Power"
    total_active_energy:
      name: "Total Active Energy"

button:
  - platform: template
    name: "Reset Energy Counters"
    on_press:
      - pzem6l24.reset_energy:
          id: pzem
          phase: all
```

## Configuration Variables
* **id** (Optional, string): Manually specify the component ID used for code generation and for referencing the device in actions.
* **address** (Optional, int, default: `0x01`): Modbus slave address of the PZEM-6L24 device (1–247). Each device on the same RS-485 bus must have a unique address.
* **update_interval** (Optional, Time, default: `60s`): How often the component polls the device for new measurements.

### Per-Phase Sensors (phases A, B, and C)
Each per-phase key follows the pattern `<measurement>_a`, `<measurement>_b`, `<measurement>_c`. All are optional.

* **voltage_a / voltage_b / voltage_c** (Optional): Line voltage for the phase in volts (V), resolution 0.1 V.
* **current_a / current_b / current_c** (Optional): Line current for the phase in amperes (A), resolution 0.01 A.
* **active_power_a / active_power_b / active_power_c** (Optional): Active (real) power for the phase in watts (W), resolution 0.1 W.
* **reactive_power_a / reactive_power_b / reactive_power_c** (Optional): Reactive power for the phase in VAR, resolution 0.1 VAR.
* **apparent_power_a / apparent_power_b / apparent_power_c** (Optional): Apparent power for the phase in VA, resolution 0.1 VA.
* **power_factor_a / power_factor_b / power_factor_c** (Optional): Power factor for the phase (0.00–1.00), resolution 0.01.
* **active_energy_a / active_energy_b / active_energy_c** (Optional): Accumulated active energy for the phase in kWh, resolution 0.1 kWh.
* **reactive_energy_a / reactive_energy_b / reactive_energy_c** (Optional): Accumulated reactive energy for the phase in kVARh, resolution 0.1 kVARh.
* **apparent_energy_a / apparent_energy_b / apparent_energy_c** (Optional): Accumulated apparent energy for the phase in kVAh, resolution 0.1 kVAh.

### Combined Sensors
* **frequency** (Optional): Grid frequency in hertz (Hz), resolution 0.01 Hz. Derived from phase A (all phases share the same grid frequency).
* **total_active_power** (Optional): Sum of active power across all three phases in watts (W), resolution 0.1 W.
* **total_reactive_power** (Optional): Sum of reactive power across all three phases in VAR, resolution 0.1 VAR.
* **total_apparent_power** (Optional): Sum of apparent power across all three phases in VA, resolution 0.1 VA.
* **total_power_factor** (Optional): Combined power factor across all three phases (0.00–1.00), resolution 0.01.
* **total_active_energy** (Optional): Total accumulated active energy across all three phases in kWh, resolution 0.1 kWh.
* **total_reactive_energy** (Optional): Total accumulated reactive energy across all three phases in kVARh, resolution 0.1 kVARh.
* **total_apparent_energy** (Optional): Total accumulated apparent energy across all three phases in kVAh, resolution 0.1 kVAh.

### Actions
* **pzem6l24.reset_energy**: Resets the energy counters on the device.
  * **id** (Required): The ID of the PZEM-6L24 component to reset.
  * **phase** (Optional, default: `all`): Which energy counter(s) to reset. Options:
    * `all` — Reset all counters (phases A, B, C, and the combined total)
    * `a` — Reset phase A only
    * `b` — Reset phase B only
    * `c` — Reset phase C only
    * `combined` — Reset the combined total only
