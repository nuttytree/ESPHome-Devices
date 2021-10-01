import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, output, pid, sensor
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
    CONF_NAME,
    CONF_ICON,
)

AUTO_LOAD = ["climate", "pid"]

garage_fridge_ns = cg.esphome_ns.namespace("garage_fridge")
GarageFridge = garage_fridge_ns.class_("GarageFridge", cg.Component)

CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"
CONF_HEAT_OUTPUT = "heat_output"
CONF_FRIDGE_HEAT_SENSOR_NAME = "fridge_heat_sensor_name"
CONF_MIN_INTEGRAL = "min_integral"
CONF_MAX_INTEGRAL = "max_integral"
CONF_FRIDGE_CONTROL = "fridge_control"
CONF_FREEZER_CONTROL = "freezer_control"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_TRIGGER_TEMPERATURE = "cool_trigger_temperature"

SECTION_CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_KP): cv.float_,
            cv.Optional(CONF_KI, default=0.0): cv.float_,
            cv.Optional(CONF_KD, default=0.0): cv.float_,
            cv.Optional(CONF_MIN_INTEGRAL, default=-1): cv.float_,
            cv.Optional(CONF_MAX_INTEGRAL, default=1): cv.float_,        }
    )
)

FRIDGE_CONFIG_SCHEMA = cv.All(
    SECTION_CONFIG_SCHEMA.extend(
        {
            cv.Required(CONF_MIN_TEMPERATURE): cv.temperature,
        }
    )
)

FREEZER_CONFIG_SCHEMA = cv.All(
    SECTION_CONFIG_SCHEMA.extend(
        {
            cv.Required(CONF_MAX_TEMPERATURE): cv.temperature,
            cv.Required(CONF_TRIGGER_TEMPERATURE): cv.temperature,
        }
    )
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(GarageFridge),
            cv.Required(CONF_HEAT_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Required(CONF_FRIDGE_HEAT_SENSOR_NAME): cv.string,
            cv.Required(CONF_FRIDGE_CONTROL): FRIDGE_CONFIG_SCHEMA,
            cv.Required(CONF_FREEZER_CONTROL): FREEZER_CONFIG_SCHEMA,
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    out = await cg.get_variable(config[CONF_HEAT_OUTPUT])
    cg.add(var.set_fridge_heat_output(out))
    cg.add(var.set_fridge_heat_sensor_name(config[CONF_FRIDGE_HEAT_SENSOR_NAME]))

    fridge = config[CONF_FRIDGE_CONTROL]
    fridge_sens = await cg.get_variable(fridge[CONF_SENSOR])
    cg.add(var.set_fridge_sensor(fridge_sens))
    cg.add(var.set_fridge_min_temp(fridge[CONF_MIN_TEMPERATURE]))
    cg.add(var.set_fridge_kp(fridge[CONF_KP]))
    cg.add(var.set_fridge_ki(fridge[CONF_KI]))
    cg.add(var.set_fridge_kd(fridge[CONF_KD]))
    cg.add(var.set_fridge_min_integral(fridge[CONF_MIN_INTEGRAL]))
    cg.add(var.set_fridge_max_integral(fridge[CONF_MAX_INTEGRAL]))

    freezer = config[CONF_FREEZER_CONTROL]
    freezer_sens = await cg.get_variable(freezer[CONF_SENSOR])
    cg.add(var.set_freezer_sensor(freezer_sens))
    cg.add(var.set_freezer_max_temp(freezer[CONF_MAX_TEMPERATURE]))
    cg.add(var.set_cool_trigger_temp(freezer[CONF_TRIGGER_TEMPERATURE]))
    cg.add(var.set_freezer_kp(freezer[CONF_KP]))
    cg.add(var.set_freezer_ki(freezer[CONF_KI]))
    cg.add(var.set_freezer_kd(freezer[CONF_KD]))
    cg.add(var.set_freezer_min_integral(freezer[CONF_MIN_INTEGRAL]))
    cg.add(var.set_freezer_max_integral(freezer[CONF_MAX_INTEGRAL]))
