# ESPHome-Devices
## Overview
> This is a collection of [ESPHome](https://ESPHome.io) configuration files and code for my various ESP8266 devices that integrate with [Home Assistant](https://www.home-assistant.io/).

> **What is ESPHome**<br>
> ESPHome is a system to control your ESP8266/ESP32 by simple yet powerful configuration files and control them remotely through Home Automation systems.  For more information checkout [ESPHome.io](https://ESPHome.io).

> **What is Home Assistant**<br>
> Home Assistant is open source home automation that puts local control and privacy first. Powered by a worldwide community of tinkerers and DIY enthusiasts. Perfect to run on a Raspberry Pi or a local server.  For more information check out [Home-Assistant.io](https://www.home-assistant.io/).

## Custom Devices
> ### Basement Bathroom Sensor
> This is a [WEMOS D1 Mini clone](https://www.amazon.com/gp/product/B076F52NQD/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a [motion sensor](https://www.amazon.com/gp/product/B07GJDJV63/ref=ppx_yo_dt_b_asin_title_o06_s01?ie=UTF8&psc=1), a [temperature/humidity/pressure sensor](https://www.amazon.com/gp/product/B07KYJNFMD/ref=ppx_yo_dt_b_asin_title_o06_s01?ie=UTF8&psc=1), and a [door sensor](https://www.amazon.com/gp/product/B07YBGZNNW/ref=ppx_yo_dt_b_asin_title_o09_s00?ie=UTF8&psc=1) and is used to control the lights, fan, and heater (heat lamps in the fan) in my basement bathroom.


> ### Coffee Maker
> This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I installed in my [Cuisinart Coffee Maker](https://www.amazon.com/gp/product/B01N6T5QNO/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).  It has GPIO's connected to the indicator lights for the bold setting and power and has a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) connected to the bold setting button and the power button.  Aside from the ability to trigger automations based on the state of the coffee maker and automate turning on the coffee maker I also added a "bloom" feature.  Coffee tastes better if you let the carbon dioxide escape (bloom) after getting the coffee grounds wet before continuing the brew cycle.

> ### Fire Pit and Fountain
> This is a [WEMOS D1 Mini Pro](https://www.amazon.com/gp/product/B07G9HZ5LM/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a couple of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) and a couple of water proof push buttons [red](https://www.amazon.com/gp/product/B079GNNSRP/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) and [blue](https://www.amazon.com/gp/product/B079GK565N/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).  The multi-click config on the fire pit switch is so that any press will turn it off but to turn it on you have to press it for 5 seconds (to try and prevent kids from turning it on).  This will be used to control the combination fire pit and fountain in my backyard when I am done rebuilding it.  I have done some testing at my desk but this is not fully hooked up yet so there may be some bugs (winter came to fast).  

> ### Garage Fridge
> This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I have installed in the back of my garage fridge and is connected to several [temperature sensors](https://www.amazon.com/gp/product/B012C597T0/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that monitor the temperature of the compressor, the fridge, and the freezer and a pair of [relays](https://www.amazon.com/gp/product/B0057OC6D8/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that control a [heat tape](https://www.amazon.com/gp/product/B07HHB1R5S/ref=ox_sc_act_title_1?smid=A1KEJ1ZBUGV6FW&psc=1) wrapped around the compressor and a couple of [heaters](https://www.amazon.com/gp/product/B07GXSDMR2/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that are inside the fridge.  This is to prevent the compressor from getting to cold (when it is cold the oil thickens up which is hard on the compressor), the fridge from dropping below freezing (frozen beer is no fun), and the freezer from getting to warm (when temperatures in the garage are around the desired fridge temperature the freezer tends to get to warm because of lack of cooling demand from the fridge so we warm the fridge up a little to get it to kick in the compressor).

> ### Master Bed
> This is a [NodeMCU](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that I have installed under my bed and is connected to a pair of [Force Sensitive Resistors](https://www.kr4.us/force-sensitive-resistor-long.html?gclid=COqn7qrNxdICFVQlgQodeLEMKw) that are placed between the mattress and box spring of my bed, one for my side and one for my wife's side.  Because the ESP8266 has only one analog input I had to add some transistors connected to pins D0 and D1 that are used to select which FSR is connected to the A0 analog input.  The custom binary sensors in [bed_sensor.h](https://github.com/nuttytree/ESPHome-Devices/blob/master/bed_sensor.h) handles switching between the FSR's and calculating who is in bed.  Generally when the bed is empty the reading is the full 1024 (or at least close to this).  If someone is on one side of the bed or the other the reading is typically around 100.  However if you lay in the middle of the bed I tend to get readings of around 600-700 on both sides.  Thus the 3 different binary sensors (Chris is in Bed, Melissa is in Bed, Someone is in Bed).  There is also a Master Bed Count that reports the total number of people in bed, I compare this to a sensor in Home Assistant that tracks the number of "masters" (my wife and me) that are home so that I can activate night mode when everyone is in bed.

> ### Patio Lights
> This is a [WEMOS D1 Mini Pro](https://www.amazon.com/gp/product/B07G9HZ5LM/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) that is connected to a [relay](https://www.amazon.com/gp/product/B00VRUAHLE/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) to control the power to the landscape lights around my patio.  I used the WEMOS D1 Mini Pro because it has an external antenna.  I initially used a [WEMOS D1 Mini clone](https://www.amazon.com/gp/product/B076F52NQD/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) but the power supply for my lights was to far from the house and I kept having connectivity issues.

## Switches
> I plan to use dimmer switches for anything that is dimmable just for consistency and you never know when you might want to have the ability to dim a light.  However there are times when a dimmer isn't an option.

> ### [SANA Dual Switch](https://www.amazon.com/gp/product/B07QC5ZCHP/ref=ppx_yo_dt_b_asin_title_o03_s01?ie=UTF8&th=1)
> My basement bathroom has 4 devices (main light, fan, shower light, heat lamps) but only 2 single gang boxes (and not enough room to swith to dual gang) so these were a perfect fit.  The prices is right, the buttons feel solid, and I was able to flash them using [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert).  They do seem to have corners that are squarer then typical so I had a little bit of trouble getting a standard cover to fit but nothing I couldn't fix with a file.
> > * Basement Bathroom Light and Fan
> > * Basement Bathroom Shower Light and Heater

> ### [SANA Triple Switch](https://www.amazon.com/gp/product/B07Q5XPRKD/ref=ox_sc_act_title_1?smid=A3EOKYTNCLEIKH&psc=1)
> COMING SOON!  I plan to use this in conjuction with a custom light component I wrote to turn on/off and change the color of my [TREO LED Pool Lights](https://www.amazon.com/gp/product/B06XTRLM5M/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) and control my [Patio Lights](#patio-lights) which currently I don't have a way to control other than through [Home Assistant](https://www.home-assistant.io/).

## Dimmer Switches
> I have been trying to find a dimmer switch I realy like to standardize on through out my home but haven't found it yet.  These are the dimmers I have tried so far.

> ### [Feit Dimmer](https://www.costco.com/feit-electric-smart-dimmer%2c-3-pack.product.100518151.html)
> I bought these as a 2 pack from Costco.  The price is right, they seem to be a fairly solid device, and I was able to flash them using [Tuya-Convert](https://github.com/ct-Open-Source/tuya-convert).  Things I don't like are the fact that you have to click repeatedly to change the brightness (can't hold to change) and all buttons are managed by the Tuya MCU so no ability to do things like double-taps or change the default brightness based on day/night mode.
> > * Kitchen Table Light
> > * Office Light
