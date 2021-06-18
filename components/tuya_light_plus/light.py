from esphome.components import light
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_GAMMA_CORRECT,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_SWITCH_DATAPOINT,
)
from esphome.components.tuya import CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_MIN_VALUE_DATAPOINT = "min_value_datapoint"
CONF_DEFAULT_DAY_BRIGHTNESS = "default_day_brightness"
CONF_DEFAULT_NIGHT_BRIGHTNESS = "default_night_brightness"

tuya_ns = cg.esphome_ns.namespace("tuya")
api_ns = cg.esphome_ns.namespace("api")
APIServer = api_ns.class_("APIServer", cg.Component, cg.Controller)
TuyaLight = tuya_ns.class_("TuyaLightPlus", light.LightOutput, cg.Component, APIServer)

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaLight),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_DIMMER_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MIN_VALUE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MIN_VALUE): cv.int_,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
            # Change the default gamma_correct and default transition length settings.
            # The Tuya MCU handles transitions and gamma correction on its own.
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
            cv.Optional(
                CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DEFAULT_DAY_BRIGHTNESS, default=1.0): cv.positive_float,
            cv.Optional(CONF_DEFAULT_NIGHT_BRIGHTNESS, default=.03): cv.positive_float,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_DIMMER_DATAPOINT, CONF_SWITCH_DATAPOINT),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    if CONF_DIMMER_DATAPOINT in config:
        cg.add(var.set_dimmer_id(config[CONF_DIMMER_DATAPOINT]))
    if CONF_MIN_VALUE_DATAPOINT in config:
        cg.add(var.set_min_value_datapoint_id(config[CONF_MIN_VALUE_DATAPOINT]))
    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    if CONF_DEFAULT_DAY_BRIGHTNESS in config:
        cg.add(var.set_day_default_brightness(config[CONF_DEFAULT_DAY_BRIGHTNESS]))
    if CONF_DEFAULT_NIGHT_BRIGHTNESS in config:
        cg.add(var.set_night_default_brightness(config[CONF_DEFAULT_NIGHT_BRIGHTNESS]))
    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
