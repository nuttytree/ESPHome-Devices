from typing import Optional
from esphome import core
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
    CONF_SENSOR_ID,
)
from esphome.components.tuya import CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_MIN_VALUE_DATAPOINT = "min_value_datapoint"
CONF_DEFAULT_BRIGHTNESS = "default_brightness"
CONF_AUTO_OFF_TIME = "auto_off_time"
CONF_LINKED_LIGHTS = "linked_lights"
CONF_DAY_NIGHT = "day_night"
CONF_SENSOR_TYPE = "sensor_type"
CONF_SENSOR_DAY_VALUE = "sensor_day_value"
CONF_SENSOR_NIGHT_VALUE = "sensor_night_value"
CONF_DAY_DEFAULT_BRIGHTNESS = "day_default_brightness"
CONF_NIGHT_DEFAULT_BRIGHTNESS = "night_default_brightness"
CONF_DAY_AUTO_OFF_TIME = "day_auto_off_time"
CONF_NIGHT_AUTO_OFF_TIME = "night_auto_off_time"

tuya_ns = cg.esphome_ns.namespace("tuya")
api_ns = cg.esphome_ns.namespace("api")
APIServer = api_ns.class_("APIServer", cg.Component, cg.Controller)
TuyaLight = tuya_ns.class_("TuyaLightPlus", light.LightOutput, cg.Component, APIServer)

DayNightSensorType = tuya_ns.enum("DayNightSensorType")
DAY_NIGHT_SENSOR_TYPE = {
    "BINARY": DayNightSensorType.BINARY,
    "TEXT": DayNightSensorType.TEXT,
}

DAY_NIGHT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR_ID): cv.entity_id,
        cv.Optional(CONF_SENSOR_TYPE, default="BINARY"): cv.enum(DAY_NIGHT_SENSOR_TYPE, upper=True),
        cv.Optional(CONF_SENSOR_DAY_VALUE): cv.string,
        cv.Optional(CONF_SENSOR_NIGHT_VALUE): cv.string,
        cv.Optional(CONF_DAY_DEFAULT_BRIGHTNESS): cv.float_range(.01, 1.0),
        cv.Optional(CONF_NIGHT_DEFAULT_BRIGHTNESS): cv.float_range(.01, 1.0),
        cv.Optional(CONF_DAY_AUTO_OFF_TIME): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NIGHT_AUTO_OFF_TIME): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaLight),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_DIMMER_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MIN_VALUE_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MIN_VALUE): cv.int_,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
            cv.Optional(CONF_DEFAULT_BRIGHTNESS): cv.float_range(.01, 1.0),
            cv.Optional(CONF_AUTO_OFF_TIME): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LINKED_LIGHTS): cv.ensure_list(cv.entity_id),
            cv.Optional(CONF_DAY_NIGHT): DAY_NIGHT_SCHEMA,
            # Change the default gamma_correct and default transition length settings.
            # The Tuya MCU handles transitions and gamma correction on its own.
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
            cv.Optional(
                CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    cg.add(var.set_dimmer_id(config[CONF_DIMMER_DATAPOINT]))
    if CONF_MIN_VALUE_DATAPOINT in config:
        cg.add(var.set_min_value_datapoint_id(config[CONF_MIN_VALUE_DATAPOINT]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    if CONF_DEFAULT_BRIGHTNESS in config:
        cg.add(var.set_default_brightness(config[CONF_DEFAULT_BRIGHTNESS]))
    if CONF_AUTO_OFF_TIME in config:
        cg.add(var.set_auto_off_time(config[CONF_AUTO_OFF_TIME]))
    if CONF_LINKED_LIGHTS in config and len(config[CONF_LINKED_LIGHTS]) > 0:
        cg.add(var.set_linked_lights(",".join(config[CONF_LINKED_LIGHTS])))
    if CONF_DAY_NIGHT in config:
        day_night = config[CONF_DAY_NIGHT]
        cg.add(var.set_day_night_sensor(day_night[CONF_SENSOR_ID]))
        cg.add(var.set_day_night_sensor_type(day_night[CONF_SENSOR_TYPE]))
        if day_night[CONF_SENSOR_TYPE].endswith("BINARY"):
            if CONF_SENSOR_DAY_VALUE not in day_night:
                day_night[CONF_SENSOR_DAY_VALUE] = "on"
            if CONF_SENSOR_NIGHT_VALUE not in day_night:
                day_night[CONF_SENSOR_NIGHT_VALUE] = "off"
        elif day_night[CONF_SENSOR_TYPE].endswith("TEXT"):
            if CONF_SENSOR_DAY_VALUE not in day_night:
                day_night[CONF_SENSOR_DAY_VALUE] = "Day"
            if CONF_SENSOR_NIGHT_VALUE not in day_night:
                day_night[CONF_SENSOR_NIGHT_VALUE] = "Night"
        cg.add(var.set_day_night_day_value(day_night[CONF_SENSOR_DAY_VALUE]))
        cg.add(var.set_day_night_night_value(day_night[CONF_SENSOR_NIGHT_VALUE]))
        if CONF_DAY_DEFAULT_BRIGHTNESS in day_night:
            cg.add(var.set_day_default_brightness(day_night[CONF_DAY_DEFAULT_BRIGHTNESS]))
        if CONF_NIGHT_DEFAULT_BRIGHTNESS in day_night:
            cg.add(var.set_night_default_brightness(day_night[CONF_NIGHT_DEFAULT_BRIGHTNESS]))
        if CONF_DAY_AUTO_OFF_TIME in day_night:
            cg.add(var.set_day_auto_off_time(day_night[CONF_DAY_AUTO_OFF_TIME]))
        if CONF_NIGHT_AUTO_OFF_TIME in day_night:
            cg.add(var.set_night_auto_off_time(day_night[CONF_NIGHT_AUTO_OFF_TIME]))
    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
