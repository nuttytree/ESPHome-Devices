# ESPHome-Devices
## Overview
This is a collection of [ESPHome](https://ESPHome.io) custom components, configuration files, and custom code for my various ESP8266/ESP32 devices that integrate with [Home Assistant](https://www.home-assistant.io/).  I am using includes and packages pretty extensively in order to prevent duplication and allow for easy changing of common settings.

### What is ESPHome
ESPHome is a system to control your ESP8266/ESP32 by simple yet powerful configuration files and control them remotely through Home Automation systems.  For more information checkout [ESPHome.io](https://ESPHome.io).

### What is Home Assistant
Home Assistant is open source home automation that puts local control and privacy first. Powered by a worldwide community of tinkerers and DIY enthusiasts. Perfect to run on a Raspberry Pi or a local server.  For more information check out [Home-Assistant.io](https://www.home-assistant.io/).


## Custom Components
I have been working on updating most of my custom code into components that can easily be pulled directly from GitHub into your device configuration using the [external components](https://esphome.io/components/external_components.html) component. I have run into frequent issues with changes in ESPHome breaking my components so I am now tagging my repo with the version of ESPHome it is compatible with. I generally upgrade pretty quickly so as soon as I have confirmed things are working and/or made the neccessary changes I will add a tag for the new version of ESPHome.

### Binary Light With Power
This an enhanced version of the standard [binary light](https://esphome.io/components/light/binary.html) component that adds an option to include a sensor to report current power usage based on a configured wattage of the light(s) it controls. More details on how to use this component are available [here](./components/binary_light_with_power/README.md).

### GPIO Light With Power
This an enhanced version of the standard [gpio switch](https://esphome.io/components/switch/gpio.html) component that adds an option to include a sensor to report current power usage based on a configured wattage of the device(s) it controls. More details on how to use this component are available [here](./components/gpio_switch_with_power/README.md).

### TREO LED Pool Light
This is a custom light component that works with [TREO LED Pool Lights](https://www.srsmith.com/en-us/products/pool-lighting/treo-led-pool-light/) and exposes the different colors as "effects" so thay can be selected from Home Assistant. More details on how to use this component are available [here](./components/treo_led_pool_light/README.md).

### Tuya Dimmer as Fan
This a modified version of the [Tuya fan](https://esphome.io/components/fan/tuya.html) component I use with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) (but it will likely work with other Tuya dimmers) to control bathroom fans and adds several features. I created this component because I couldn't find a regular on/off switch with the same look and feel as the Feit dimmers so I decided to use the Feit dimmers but use this component to prevent "dimming" the fan. More details on features and how to use this component are available [here](./components/tuya_dimmer_as_fan/README.md).

### Tuya Light Plus
This an enhanced version of the standard [Tuya light](https://esphome.io/components/light/tuya.html) component that adds a bunch of extra features. I use this component with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) but it will likely work with other Tuya dimmers. More details on features and how to use this component are available [here](./components/tuya_light_plus/README.md).

### ESPSense
This excellent component is not mine and doesn't live in this repository but most of my devices are using it so I felt it was worthy of a mention here. More details are available [here](https://github.com/cbpowell/ESPSense).


## Misc Devices
### [Basement Bathroom Sensor](./devices/basement_bathroom_sensor.yaml)
This is a [WEMOS D1 Mini clone](https://www.amazon.com/gp/product/B076F52NQD/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a [motion sensor](https://www.amazon.com/gp/product/B07GJDJV63/ref=ppx_yo_dt_b_asin_title_o06_s01?ie=UTF8&psc=1), a [temperature/humidity/pressure sensor](https://www.amazon.com/gp/product/B07KYJNFMD/ref=ppx_yo_dt_b_asin_title_o06_s01?ie=UTF8&psc=1), and a [door sensor](https://www.amazon.com/gp/product/B07YBGZNNW/ref=ppx_yo_dt_b_asin_title_o09_s00?ie=UTF8&psc=1) and is used to control the lights, fan, and heater (heat lamps in the fan) in my basement bathroom.

### [Coffee Maker](./devices/coffee_maker.yaml)
This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I installed in my [Cuisinart Coffee Maker](https://www.amazon.com/gp/product/B01N6T5QNO/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).  It has GPIO's connected to the indicator lights for the bold setting and power and has a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) connected to the bold setting button and the power button.  Aside from the ability to trigger automations based on the state of the coffee maker and automate turning on the coffee maker I also added a "bloom" feature.  Coffee tastes better if you let the carbon dioxide escape (bloom) after getting the coffee grounds wet before continuing the brew cycle.

### [Fire Pit and Fountain](./devices/fire_pit_fountain.yaml)
This is a [WEMOS D1 Mini Pro](https://www.amazon.com/gp/product/B07G9HZ5LM/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) and a couple of water proof push buttons [red](https://www.amazon.com/gp/product/B079GNNSRP/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) and [blue](https://www.amazon.com/gp/product/B079GK565N/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).  The multi-click config on the fire pit switch is so that any press will turn it off but to turn it on you have to press it for 3 seconds (to try and prevent kids from turning it on).  This is used to control the combination fire pit and fountain in my backyard.  I just got this hooked up and is working good other then the fire pit doesn't always come on on the first try (I think I am getting some bounce in the switch but haven't had a chance to troubleshoot further). 

### [Garage Fridge](./devices/garage_fridge.yaml)
This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I have installed in the back of my garage fridge and is connected to several [temperature sensors](https://www.amazon.com/gp/product/B012C597T0/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that monitor the temperature of the compressor, the fridge, and the freezer and a pair of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that control a [heat tape](https://www.amazon.com/gp/product/B07HHB1R5S/ref=ox_sc_act_title_1?smid=A1KEJ1ZBUGV6FW&psc=1) wrapped around the compressor and a couple of [heaters](https://www.amazon.com/gp/product/B07GXSDMR2/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that are inside the fridge.  This is to prevent the compressor from getting to cold (when it is cold the oil thickens up which is hard on the compressor), the fridge from dropping below freezing (frozen beer is no fun), and the freezer from getting to warm (when temperatures in the garage are around the desired fridge temperature the freezer tends to get to warm because of lack of cooling demand from the fridge so we warm the fridge up a little to get it to kick in the compressor).

### [Master Bed](./devices/master_bed.yaml)
This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I have installed under my bed and is connected to a pair of [Force Sensitive Resistors](https://www.kr4.us/force-sensitive-resistor-long.html?gclid=COqn7qrNxdICFVQlgQodeLEMKw) that are placed between the mattress and box spring of my bed, one for my side and one for my wife's side.  Because the ESP8266 has only one analog input I had to add some transistors connected to pins D0 and D1 that are used to select which FSR is connected to the A0 analog input.  The custom sensors in [bed_sensor.h](./custom/bed_sensor.h) handles switching between the FSR's and reading the values from the FSR's.  Generally when the bed is empty the reading is the full 1024 (or at least close to this).  If someone is on one side of the bed or the other the reading is typically around 100.  However if you lay in the middle of the bed I tend to get readings of around 600-700 on both sides.  Thus the 3 different binary sensors (Chris is in Bed, Melissa is in Bed, Someone is in Bed).  There is also a Master Bed Count that reports the total number of people in bed, I compare this to a sensor in Home Assistant that tracks the number of "masters" (my wife and me) that are home so that I can activate night mode when everyone is in bed.

### [Patio Lights](./devices/patio_lights.yaml)
This is a [WEMOS D1 Mini Pro](https://www.amazon.com/gp/product/B07G9HZ5LM/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a [relay](https://www.amazon.com/gp/product/B00VRUAHLE/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) to control the power to the landscape lights around my patio.  I used the WEMOS D1 Mini Pro because it has an external antenna.  I initially used a [WEMOS D1 Mini clone](https://www.amazon.com/gp/product/B076F52NQD/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) but the power supply for my lights was to far from the house and I kept having connectivity issues.

### [Pool Pumps](./devices/pool_pumps.yaml)
This is a [Shelly 2.5 Double Relay Switch](https://www.amazon.com/gp/product/B07RRD13DJ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is controlling the main pump on my pool and the auxillary pump that runs the pool cleaner.  It turns the pump on and off through out the day and off at night (when we are least likely to be using the pool and when you get the most heat/water loss if the pump is running) and turns on the cleaner for a couple of hours in the morning.  It also has some modes for Off (during the winter) and Continous when I need extra cleaning.

### [Scripture of the Day Melissa](./devices/scripture_of_the_day_melissa.yaml)/[Scripture of the Day Mayson](./devices/scripture_of_the_day_mayson.yaml)
This project was one of the first projects I have done that I would call woodworking.  I made 2 of these as Christmas presents for my wife and son.  They consist of an [ESP32 e-Paper Panel Driver Board](https://www.waveshare.com/product/displays/e-paper-esp32-driver-board.htm) powered by a [lithium battery](https://www.amazon.com/dp/B0867KDMY7/?coliid=IE0LBNACFHG4U&colid=1LOK862UA8LA0&psc=1&ref_=lv_ov_lig_dp_it) and [TP4056 lithium battery charger module](https://www.amazon.com/dp/B06XQRQR3Q/?coliid=I1B2ELKKLLKAK0&colid=1LOK862UA8LA0&psc=1&ref_=lv_ov_lig_dp_it) and driving a [Waveshare 7.5inch 800×480 E-Ink display](https://www.waveshare.com/7.5inch-e-paper.htm).  The ESP32 sits in deep-sleep most of the time but wakes up every night, grabs a random scripture from the [Our Manna Daily Verses API](http://www.ourmanna.com/verses/api/), updates the display with the verse, and goes back to sleep.
<br/><br/>
<img src="./images/scripture_of_the_day/front.jpg" alt="Front" width="600" /><img src="./images/scripture_of_the_day/back.jpg" alt="Back" width="600" />


## Switches
I plan to use dimmer switches for anything that is dimmable just for consistency and you never know when you might want to have the ability to dim a light.  However there are times when a dimmer isn't an option.

### [SANA Dual Switch](https://www.amazon.com/gp/product/B07QC5ZCHP/ref=ppx_yo_dt_b_asin_title_o03_s01?ie=UTF8&th=1)
My basement bathroom has 4 devices (main light, fan, shower light, and heat lamps) but only 2 single gang boxes (and not enough room to swith to dual gang) so these were a perfect fit.  The price is right, the buttons feel solid, and I was able to flash them using [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert).  They do seem to have corners that are squarer then typical so I had a little bit of trouble getting a standard cover to fit but nothing I couldn't fix with a file.
* [Basement Bathroom Light and Fan](./devices/basement_bathroom_light_fan.yaml)
* [Basement Bathroom Shower Light and Heater](./devices/basement_bathroom_shower_light_heater.yaml)

### [SANA Triple Switch](https://www.amazon.com/gp/product/B07Q5XPRKD/ref=ox_sc_act_title_1?smid=A3EOKYTNCLEIKH&psc=1)
This is installed in place of the switch that controled my [TREO LED Pool Lights](https://www.amazon.com/gp/product/B06XTRLM5M/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).  These lights have the option to select different colors by briefly turning them off and back on again and they do remember the last color when turned on again.  The custom [TreoLedPoolLight.h](./custom/TreoLedPoolLight.h) component handles tracking the current color and exposes custom "effects" for each of the colors to [Home Assistant](https://www.home-assistant.io/).  I went with the triple switch so that I could use the third button to control my [Patio Lights](#patio-lights) which otherwise do not have a physical switch.  The one issue I am having with this is occasionally the "effect" on the switch gets out of sync with the actual color of the lights.  It has only happened a couple of times so I haven't gotten around to investigating why it happens.
* [Patio and Pool Lights](./devices/patio_and_pool_lights.yaml)

## Dimmer Switches
### [Feit Dimmers](https://www.feit.com/product/smart-wi-fi-dimmer/)
After trying several dimmers I finally decided to standardize on the Feit dimmers.  
Things I like about these dimmers:
* ~~Can be flashed using [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert)~~ Unfortunately Feit has started shipping these with updated firmware that does not currently work with Tuya Convert. Hopefully the excellent Tuya Convert team can figure out how to work around the new firmware. Until then I did find this [tutorial](https://community.smartthings.com/t/costco-cheap-feit-smart-dimmer-wifi/208142) on flashing these devices (I have not tried this yet).
* Have a solid feel to them
* They can be linked via a traveler wire (this works even when flashed with ESPHome and while not mentioned in the documentation you can link more than 2 switches this way)
* Are readily available from [Amazon](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) or [Costco](https://www.costco.com/feit-electric-smart-dimmer%2c-3-pack.product.100518151.html) for an affordable price.
  
Things I don't like about these dimmers:
* Have to click repeatedly to change the brightness (can't hold to change)
* All buttons are managed by the Tuya MCU so adding things like double-taps is kind of a hack
  
All of my dimmers are using my custom [Tuya Light Plus](./components/tuya_light_plus/README.md) component.
* [Basement Stair Lights](./devices/basement_stair_lights_1.yaml)/[Basement Stair Lights 2](./devices/basement_stair_lights_2.yaml)
* [Computer Light](./devices/computer_light.yaml)
* [Dining Room Light](./devices/dining_room_light.yaml)
* [Family Room Light](./devices/family_room_light.yaml)
* [Front Entry Lights](./devices/front_entry_lights_1.yaml)/[Front Entry Lights 2](./devices/front_entry_lights_2.yaml)
* [Front Lights](./devices/front_lights.yaml)
* [Kitchen Bar Lights](./devices/kitchen_bar_lights.yaml)
* [Kitchen Table Light](./devices/kitchen_table_light.yaml)
* [Living Room Lights](./devices/living_room_lights.yaml)
* [Master Bathroom Lights](./devices/master_bath_lights_1.yaml)/[Master Bathroom Lights 2](./devices/master_bath_lights_2.yaml)
* [Office Light](./devices/office_light.yaml)

## Dimmer Switches as On/Off Fan Switches
### [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1)
I tried to find an on/off switch that looked/felt like the Feit dimmers to use for controlling bathroom fans but didn't find anything so I decided to use the Feit dimmers. I don't want the fans "dimmable" so I created a custom [Tuya Dimmer as Fan](./components/tuya_dimmer_as_fan/README.md) component that changes the "brightness" back to the maximum value if changed at the switch.
* [Master Bath Fan](./devices/master_bath_fan.yaml)