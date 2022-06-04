import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, output, sensor, binary_sensor, rtttl
from esphome.const import (
    CONF_ID,
    CONF_OPEN_DURATION,
    CONF_CLOSE_DURATION,
    UNIT_MILLISECOND,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
    ICON_TIMER,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

AUTO_LOAD = ["lock"]

CONF_CONTROL_OUTPUT = "control_output"
CONF_BUTTON_SENSOR = "button_sensor"
CONF_CLOSED_SENSOR = "closed_sensor"
CONF_OPEN_SENSOR = "open_sensor"
CONF_REMOTE_SENSOR = "remote_sensor"
CONF_REMOTE_LIGHT_SENSOR = "remote_light_sensor"
CONF_WARNING_RTTTL = "warning_rtttl"
CONF_CLOSE_WARNING_TONES = "close_warning_tones"
DEFAULT_CLOSE_WARNING_TONES = "Imperial:d=4, o=5, b=100:e, e, e, 8c, 16p, 16g, e, 8c, 16p, 16g, e, p, b, b, b, 8c6, 16p, 16g, d#, 8c, 16p, 16g, e, 8p"
CONF_LAST_OPEN_TIME_SENSOR = "last_open_time_sensor"
CONF_LAST_CLOSE_TIME_SENSOR = "last_close_time_sensor"
DEVICE_CLASS = "garage"

cover_ns = cg.esphome_ns.namespace("cover")
GarageDoor = cover_ns.class_("GarageDoor", cover.Cover, cg.PollingComponent)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GarageDoor),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CONTROL_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Required(CONF_BUTTON_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CLOSED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_OPEN_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_REMOTE_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_REMOTE_LIGHT_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_WARNING_RTTTL): cv.use_id(rtttl.Rtttl),
            cv.Optional(CONF_CLOSE_WARNING_TONES, default=DEFAULT_CLOSE_WARNING_TONES): cv.string,
            cv.Optional(CONF_LAST_OPEN_TIME_SENSOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLISECOND,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_TIMER,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LAST_CLOSE_TIME_SENSOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLISECOND,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_TIMER,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)

    cg.add(var.set_device_class(DEVICE_CLASS))

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))

    control_output = await cg.get_variable(config[CONF_CONTROL_OUTPUT])
    cg.add(var.set_control_output(control_output))

    button_sensor = await cg.get_variable(config[CONF_BUTTON_SENSOR])
    cg.add(var.set_button_sensor(button_sensor))

    closed_sensor = await cg.get_variable(config[CONF_CLOSED_SENSOR])
    cg.add(var.set_closed_sensor(closed_sensor))

    open_sensor = await cg.get_variable(config[CONF_OPEN_SENSOR])
    cg.add(var.set_open_sensor(open_sensor))

    remote_sensor = await cg.get_variable(config[CONF_REMOTE_SENSOR])
    cg.add(var.set_remote_sensor(remote_sensor))

    remote_light_sensor = await cg.get_variable(config[CONF_REMOTE_LIGHT_SENSOR])
    cg.add(var.set_remote_light_sensor(remote_light_sensor))

    warning_rtttl = await cg.get_variable(config[CONF_WARNING_RTTTL])
    cg.add(var.set_warning_rtttl(warning_rtttl))

    close_warning_tones = config[CONF_CLOSE_WARNING_TONES]
    cg.add(var.set_close_warning_tones(close_warning_tones))

    if CONF_LAST_OPEN_TIME_SENSOR in config:
        last_open_time_config = config[CONF_LAST_OPEN_TIME_SENSOR]
        last_open_time_sensor = await sensor.new_sensor(last_open_time_config)
        cg.add(var.set_last_open_time_sensor(last_open_time_sensor))

    if CONF_LAST_CLOSE_TIME_SENSOR in config:
        last_close_time_config = config[CONF_LAST_CLOSE_TIME_SENSOR]
        last_close_time_sensor = await sensor.new_sensor(last_close_time_config)
        cg.add(var.set_last_close_time_sensor(last_close_time_sensor))
