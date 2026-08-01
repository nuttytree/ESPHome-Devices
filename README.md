# ESPHome-Devices
## Overview
This is a collection of [ESPHome](https://ESPHome.io) custom components, configuration files, and custom code for my various ESP8266/ESP32 devices that integrate with [Home Assistant](https://www.home-assistant.io/).  I am using includes and packages pretty extensively in order to prevent duplication and allow for easy changing of common settings.

### What is ESPHome
ESPHome is a system to control your ESP8266/ESP32 by simple yet powerful configuration files and control them remotely through Home Automation systems.  For more information checkout [ESPHome.io](https://ESPHome.io).

### What is Home Assistant
Home Assistant is open source home automation that puts local control and privacy first. Powered by a worldwide community of tinkerers and DIY enthusiasts. Perfect to run on a Raspberry Pi or a local server.  For more information check out [Home-Assistant.io](https://www.home-assistant.io/).


## Folder Structure
* `/` - Repo level files (linting/formatting config, docs, license)
* `/components` - Custom components
* `/devices` - Yaml files for my devices; this is the folder mounted as the config folder in the ESPHome container
* `/devices/packages` - Shared packages used by my devices
* `/docs` - Detailed write-ups (photos, schematics, notes) for my more complex/custom devices
* `/images` - Misc pictures not tied to a specific device doc
* `/scripts` - Scripts for managing the repo

Because `/devices` is what the ESPHome container sees as `/config`, the custom components in `/components` are mounted alongside it so that `path: ../components` resolves from either the container or a local ESPHome CLI run.


## Secrets Management
You will notice that throughout the various folders I have secrets.yaml files. These files all do an include of the secrets.yaml file in the `/devices` folder (that for obvious reasons is not included in the repo).


## Custom Components
I have been working on updating most of my custom code into components that can easily be pulled directly from GitHub into your device configuration using the [external components](https://esphome.io/components/external_components.html) component. I have run into frequent issues with changes in ESPHome breaking my components so I am now tagging my repo with the version of ESPHome it is compatible with. I generally upgrade pretty quickly so as soon as I have confirmed things are working and/or made the neccessary changes I will add a tag for the new version of ESPHome. While I primarily design these components for my own personal use cases I hope that at least some of them are useful for others. If you are using one of my components and have an enhancement/feature you would like to see feel free to add an issue and I will see what I can do to get it added.

### Pool Controller
Schedule-based control for a pool's primary circulation pump, an auxiliary cleaner pump, and an optional heater, with current-sensed anomaly detection. Runs on the ESP32-S3 in my [Pool](./docs/pool/README.md) controller. Details: [components/pool_controller/README.md](./components/pool_controller/README.md).

### TREO LED Pool Light
A light component for [TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/) that exposes each color as a selectable "effect" in Home Assistant. Details: [components/treo_led_pool_light/README.md](./components/treo_led_pool_light/README.md).

### PZEM-6L24 Three-Phase Energy Monitor
A native ESPHome component for the PZEM-6L24 three-phase AC energy meter (voltage/current/power/energy per phase over Modbus RTU). Used by my [Pool](./docs/pool/README.md) controller to monitor the pump and cleaner. Details: [components/pzem6l24/README.md](./components/pzem6l24/README.md).

### Bed Sensor
Detects per-side bed occupancy using resistive pressure sensors multiplexed onto a single ADC input. Used by [Master Bed](./docs/master-bed/README.md). Details: [components/bed_sensor/README.md](./components/bed_sensor/README.md).

### Econet Zone Control
A single Home Assistant climate entity that manages a multi-zone Econet-connected furnace/AC, balancing airflow across zones when idle and syncing mode/temperature with the physical thermostat. Used by [HVAC](./docs/hvac/README.md). Details: [components/econet_zone_control/README.md](./components/econet_zone_control/README.md).

### High Temp Water Heater
Sits in front of an Econet water heater so any target temperature can be set from Home Assistant while the physical unit itself stays pinned at a fixed setting (150°F) — the physical control panel only lets you adjust it to values above 140°F, so this is how I get finer/lower control (useful for time-of-use electric rates). Used by [Water Heater](./devices/water-heater.yaml). Details: [components/high_temp_water_heater/README.md](./components/high_temp_water_heater/README.md).


## Misc Devices
### [Coffee Maker](./devices/coffee-maker.yaml)
This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK) that I installed in my [Cuisinart Coffee Maker](https://www.amazon.com/gp/product/B01N6T5QNO).  It has GPIO's connected to the indicator lights for the bold setting and power and has a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8) connected to the bold setting button and the power button.

### [Fire Pit and Fountain](./devices/fire-pit-fountain.yaml)
This is a [WEMOS D1 Mini Pro](https://www.amazon.com/gp/product/B07G9HZ5LM) that is connected to a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8) and a couple of water proof push buttons [red](https://www.amazon.com/gp/product/B079GNNSRP) and [blue](https://www.amazon.com/gp/product/B079GK565N).  The multi-click config on the fire pit switch is so that any press will turn it off but to turn it on you have to press it for 3 seconds (to try and prevent kids from turning it on).  This is used to control the combination fire pit and fountain in my backyard.  I just got this hooked up and is working good other then the fire pit doesn't always come on on the first try (I think I am getting some bounce in the switch but haven't had a chance to troubleshoot further).

### [Garage Fridge](./devices/garage-fridge.yaml)
A [M5Stack ATOM HUB SwitchD](https://shop.m5stack.com/products/atom-hub-switchd-2-relay-kit) with power and temperature monitoring plus PID-controlled freeze-protection heaters. Full write-up and photos: [docs/garage-fridge](./docs/garage-fridge/README.md).

### [HVAC](./devices/hvac.yaml)
A [M5Stack ATOMIC RS485 Base](https://shop.m5stack.com/products/atomic-rs485-base) with an ESP32 connected to a 3-zone Rheem furnace/AC system (Main Floor, Upstairs, Basement) integrated via [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) and the custom [Econet Zone Control](./components/econet_zone_control/README.md) component, which presents all zones as a single Home Assistant climate entity and balances airflow across zones when idle. Full write-up: [docs/hvac](./docs/hvac/README.md).

### [Master Bed](./devices/master-bed.yaml)
A [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK) under the mattress that detects per-side occupancy using a pair of Force Sensitive Resistors and the custom [bed_sensor](./components/bed_sensor/README.md) component. Full write-up: [docs/master-bed](./docs/master-bed/README.md).

### [Pool](./devices/pool.yaml)
A [Waveshare ESP32-S3 7" Touch LCD B](https://www.waveshare.com/product/esp32-s3-lcd-7b.htm) running the custom [Pool Controller](./components/pool_controller/README.md) and [PZEM-6L24](./components/pzem6l24/README.md) components to manage pump scheduling, water chemistry, and (eventually) heat/fill/drain. Full write-up: [docs/pool](./docs/pool/README.md).

### [Water Heater](./devices/water-heater.yaml)
A [M5Stack ATOMIC RS485 Base](https://shop.m5stack.com/products/atomic-rs485-base) with an ESP32 connected to a Rheem hybrid water heater managed through the [ESPHome Econet](https://github.com/esphome-econet/esphome-econet) component. The custom [High Temp Water Heater](./components/high_temp_water_heater/README.md) component sits in front of it so I can set any target temperature I want (e.g. 140°F during peak electric rate hours) while the physical water heater setting itself stays fixed at 150°F — the physical control panel only lets you adjust it to values above 140°F.


## Energy Monitor
### [Emporia Gen 2 Vue](https://www.emporiaenergy.com/how-the-vue-energy-monitor-works)
This device is sinificantly cheaper then a Sense Energy Monitor, can monitor 16 circuits in addition to the mains (Sense can only do 2 additional), and comes with all of the additional CT clamps.  Like the Sense it is intended to work with a cloud service but because it is based on an ESP32 it can be flashed with ESPHome and made a local only device.  More details can be found [here](https://gist.github.com/flaviut/93a1212c7b165c7674693a45ad52c512).


## Smart Plugs
### [TOPGREENER TGWF115PQM Smart Plugs](https://www.amazon.com/gp/product/B07D8ZVJN2)
I have a bunch of these smart plugs that I use for various things where I want to monitor the power consumption and/or be able to completely turn off the power (because they have a significant standby power draw). The TOPGREENER units are cheap, can be flashed with [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert) (or they could last time I bought one), and have power monitoring.
* [Basement Fridge](./devices/basement-fridge.yaml)
* [Basement TV](./devices/basement-tv.yaml)
* [Family Room TV](./devices/family-room-tv.yaml)
* [Kitchen Fridge](./devices/kitchen-fridge.yaml)
* [Network Equipment](./devices/network-equipment.yaml)
* [Sump Pump](./devices/sump-pump.yaml)
* [Washing Machine](./devices/washing-machine.yaml)


## Switches
### [SANA Dual Switch](https://www.amazon.com/gp/product/B07QC5ZCHP)
My basement bathroom has a single gang box (and not enough room to swith to dual gang) for the switches to control the shower light and heat lamps so this is a perfect fit.  The price is right, the buttons feel solid, and I was able to flash it using [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert).  It does seem to have corners that are squarer then typical so I had a little bit of trouble getting a standard cover to fit but nothing I couldn't fix with a file.
* [Basement Bathroom Shower Light and Heater](./devices/basement-bath-shower-light-heat.yaml)

### [SANA Triple Switch](https://www.amazon.com/gp/product/B07Q5XPRKD)
This is installed in place of the switch that controled my [TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/).  These lights have the option to select different colors by briefly turning them off and back on again and they do remember the last color when turned on again.  The custom [Treo Led Pool Lights](./components/treo_led_pool_light/README.md) component handles tracking the current color and exposes custom "effects" for each of the colors to [Home Assistant](https://www.home-assistant.io/).  I have this configured to turn the lights on/off with the top/bottum buttons and use the middle button to change colors.
* [Pool Lights](./devices/pool-lights.yaml)

### [Shelly 1L](https://shelly.cloud/products/shelly-1l-single-wire-smart-home-automation-relay/)
My garage lights are not dimmable so I decided to try out a Shelly device, I have this connected to a [Leviton 5657-2W Momentary Center-Off Decora Rocker](https://www.amazon.com/gp/product/B000U3BU56). Overall I am fairly happy with it I just wish the rocker switch wasn't so expensive or there was another similar option.
* [Garage Lights](./devices/garage-lights.yaml)


## Garage Doors
### [Ratgdo](https://paulwieland.github.io/ratgdo/)
Both garage door openers are controlled by [ratgdo](https://paulwieland.github.io/ratgdo/) boards running the [esphome-ratgdo](https://github.com/ratgdo/esphome-ratgdo) external package, which taps into the opener's built-in security+2.0 wall panel/dry-contact wiring instead of a relay-across-the-button hack.
* [Main Garage Door](./devices/main-garage-door.yaml) — v2.5i board variant connected to a Chambelain MyQ opener
* [Second Garage Door](./devices/second-garage-door.yaml) — dry-contact variant connected to an old opener


## Network
### Bluetooth Proxy
An ESP32 running ESPHome's [Bluetooth Proxy](https://esphome.io/components/bluetooth_proxy.html) to extend Home Assistant's Bluetooth range.
* [Basement Bluetooth Proxy](./devices/bt-proxy-basement.yaml)
