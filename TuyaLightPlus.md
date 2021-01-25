# Tuya Light Plus
## Overview
> The custom [tuya_light_plus.h](./custom/tuya_light_plus.h) component extends the standard Tuya Light component. I have only used this with [Feit Dimmers](https://www.amazon.com/gp/product/B07SXDFH38/ref=ppx_yo_dt_b_asin_title_o02_s00?ie=UTF8&psc=1) but it should work with most (if not all) Tuya dimmers and provides the following extra features:
> > * Resets the brightness level back to a default level when turned off so that it always comes on at the same level instead of the level it was at when turned off
> > * The default level is configurable as different levels when my house is in "Day" mode vs "Night" mode (based on a sensor in Home Assistant) and via a service in Home Assistant
> > * Provides an option to auto turn off the light after a period of time
> > * The auto turn off time is configurable to be different when my house is in "Day" mode vs "Night" mode (based on a sensor in Home Assistant) and via a service in Home Assistant
> > * Adds ability to specify a function to call when the dimmer is double tapped while off
> > * Double tapping while off can be configured to leave the light in an off or on state
> > * Adds ability to specify a function to call when the dimmer is double tapped while on
> > * Allows you to "link" other light(s) in Home Assistant that will be controlled by this dimmer


## Setup
> 1. Include the custom component
> > ```yaml
> > esphome:
> >   includes:
> >     - ../custom/tuya_light_plus.h
> > ```
> 2. Setup Dependencies
> > Like the standard Tuya Light component we need the UART and Tuya components
> > ```yaml
> > uart:
> >   rx_pin: GPIO3
> >   tx_pin: GPIO1
> >   baud_rate: 9600
> >
> > tuya:
> > ```
> 3. Setup the Custom Light Component
> > ```yaml
> > light:
> >   - platform: custom
> >     lambda: |-
> >       # Required - Creates the custom light component.
> >       TuyaLight = new TuyaLightPlus();
> >       # Required - Sets the Tuya component used to communicate with the light.
> >       TuyaLight->set_tuya_parent(tuya_tuya);
> >       # Required - Sets the API Server object needed to configure the services in Home Assistant and add the Day/Night sensor listener.
> >       TuyaLight->set_api_server(api_apiserver);
> >       # Required - Sets the switch datapoint (same as switch_datapoint option in the standard Tuya light component).
> >       TuyaLight->set_switch_id(1);
> >       # Required - Sets the dimmer datapoint (same as dimmer_datapoint option in the standard Tuya light component).
> >       TuyaLight->set_dimmer_id(2);
> >       # Optional(default: 0) - Sets the lowest dimmer value allowed (same as min_value option in the standard Tuya light component).
> >       TuyaLight->set_min_value(0);
> >       # Optional(default: 255) - Sets the highest dimmer value allowed (same as max_value option in the standard Tuya light component).
> >       TuyaLight->set_max_value(1000);
> >       # Optional(default: "") - Sets the name of the sensor in Home Assistant that tracks day and night mode (This needs to be a Text sensor with Day and Night as the possible values).
> >       TuyaLight->set_day_night_sensor("sensor.day_night");
> >       # Optional(default: 1) - Sets the default brightness to turn the light on at (valid values are .01 - 1). Note this value will get updated with the Day/Night values below if the Day/Night sensor is set.
> >       TuyaLight->set_default_brightness(1);
> >       # Optional(default: 1) - Sets the default brightness to turn the light on at during the day (valid values are .01 - 1).
> >       TuyaLight->set_day_default_brightness(1);
> >       # Optional(default: .03) - Sets the default brightness to turn the light on at during the night (valid values are .01 - 1).
> >       TuyaLight->set_night_default_brightness(.03);
> >       # Optional(default: 0) - Sets the number of minutes before the light is turned off automatically (0 disables auto off). Note this value will get updated with the Day/Night values below if the Day/Night sensor is set.
> >       TuyaLight->set_auto_off_minutes(0);
> >       # Optional(default: 0) - Sets the number of minutes before the light is turned off automatically during the day (0 disables auto off during the day).
> >       TuyaLight->set_day_auto_off_minutes(0);
> >       # Optional(default: 0) - Sets the number of minutes before the light is turned off automatically during the night (0 disables auto off during the night).
> >       TuyaLight->set_night_auto_off_minutes(15);
> >       # Optional(default: "") - Sets the comma delimited list of lights in Home Assistant that are "linked" to this light and will be controlled by this dimmer.
> >       TuyaLight->set_linked_lights("light.some_light,light.some_other_light");
> >       # Optional - Adds a function to run when double tapped while off. This can be any valid C++ code similar to a lambda funtion. Multiple functions can be added by calling this method again.
> >       TuyaLight->add_double_tap_while_off_callback([]() { 
> >         ESP_LOGD("tuya_light_plus", "Light was double tapped while off"); 
> >       });
> >       # Optional - Adds a function to run when double tapped while on. This can be any valid C++ code similar to a lambda funtion. Multiple functions can be added by calling this method again.
> >       TuyaLight->add_double_tap_while_on_callback([]() { 
> >         ESP_LOGD("tuya_light_plus", "Light was double tapped while on"); 
> >       });
> >       # Optional - Adds a function to run when double tapped (both while on and while off). This can be any valid C++ code similar to a lambda funtion. Multiple functions can be added by calling this method again.
> >       TuyaLight->add_double_tap_callback([]() { 
> >         ESP_LOGD("tuya_light_plus", "Light was double tapped"); 
> >       });
> >       # Optional(default: true) - If true and 1 or more functions are registered to run when double tap while off then the light will be turned on when double tapped while off, if false the light will remain off after the double tap.
> >       TuyaLight->set_double_tap_while_off_stays_on(true);
> >       # Required - Registers the custom component with ESPHome
> >       App.register_component(TuyaLight);
> >       # Required - Returns the configured component to be added as a light
> >       return {TuyaLight};
> >     lights:
> >       - id: tuya_light
> >         name: Tuya Light
> >         gamma_correct: 1.0
> >         default_transition_length: 0s
> > ```


## Operation
> 1. Update from Home Assistant
> > After connecting to Home Assistant 2 services will be created that can be used to update the lights behavior:
> > * esphome.{device_name}_set_auto_off_minutes - This service takes a parameter "minutes" and updates the current minutes to auto turn off the light.  Note this value will get updated if the Day/Night sensor is set.
> > * esphome.{device_name}_set_default_brightness - This service takes a parameter "brightness" (valid values .01 - 1) and updates the current default brightness to turn the light on at.  Note this value will get updated if the Day/Night sensor is set.
> 2. Update from code on the dimmer
> > The following methods can be called from a lambda function (or other custom code) at anytime to update the settings on the light:
> > * TuyaLight->set_default_brightness(float brightness);
> > * TuyaLight->set_day_default_brightness(float brightness);
> > * TuyaLight->set_night_default_brightness(float brightness);
> > * TuyaLight->set_auto_off_minutes(int minutes);
> > * TuyaLight->set_day_auto_off_minutes(int minutes);
> > * TuyaLight->set_night_auto_off_minutes(int minutes);
> > * TuyaLight->set_double_tap_while_off_stays_on(bool stays_on);
