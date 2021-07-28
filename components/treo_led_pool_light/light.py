from esphome.components import light
from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_PIN,
)

light_ns = cg.esphome_ns.namespace("light")
TreoLight = light_ns.class_("TreoLedPoolLight", light.LightOutput, cg.Component)

CONFIG_SCHEMA = cv.All(
    light.BINARY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TreoLight),
            cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # cg.add(var.set_gpio(config[CONF_PIN]))
