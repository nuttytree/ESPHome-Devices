from esphome.components import light, sensor
from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_POWER,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    ICON_POWER,
    CONF_UPDATE_INTERVAL,
)

CONF_LIGHT_WATTAGE = "light_wattage"

light_ns = cg.esphome_ns.namespace("light")
api_ns = cg.esphome_ns.namespace("api")
APIServer = api_ns.class_("APIServer", cg.Component, cg.Controller)
TreoLight = light_ns.class_("TreoLedPoolLight", light.LightOutput, cg.PollingComponent, APIServer)

CONFIG_SCHEMA = cv.All(
    light.LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TreoLight),
            cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_POWER,
            ).extend(
                {
                    cv.Optional(CONF_LIGHT_WATTAGE): cv.positive_float,
                    cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    if CONF_POWER in config:
        power_config = config[CONF_POWER]
        power_sensor = await sensor.new_sensor(power_config)
        cg.add(var.set_light_wattage(power_config[CONF_LIGHT_WATTAGE]))
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_update_interval(power_config[CONF_UPDATE_INTERVAL]))
    else:
        cg.add(var.set_update_interval(4294967295)) # uint32_t max
