import esphome.codegen as cg
from esphome.components import output, sensor, water_heater
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT

from .types import PoolHeater

# Constants are defined inline here because they are only used for the pool heater.
CONF_POOL_HEATER = "pool_heater"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_DEADBAND = "deadband"
CONF_OVERRUN = "overrun"

# Deadband / overrun default: 0.5 °F expressed in °C.
_HEATER_DELTA_C = 0.5 * 5.0 / 9.0

# Temperature range and step defaults live in the C++ field initializers
# (60 °F / 15.56 °C min, 90 °F / 32.22 °C max, 1 °F / 0.556 °C step).
# They can be overridden via the standard `visual:` block in the YAML.
POOL_HEATER_SCHEMA = (
    water_heater.water_heater_schema(PoolHeater)
    .extend(
        {
            cv.Required(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_DEADBAND, default=_HEATER_DELTA_C): cv.temperature_delta,
            cv.Optional(CONF_OVERRUN, default=_HEATER_DELTA_C): cv.temperature_delta,
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def heater_to_code(controller_var, heater_config, primary_pump_var):
    """Register the pool heater and wire it to the primary pump and controller."""
    heater = await water_heater.new_water_heater(heater_config)
    await cg.register_component(heater, heater_config)

    temp_sens = await cg.get_variable(heater_config[CONF_TEMPERATURE_SENSOR])
    cg.add(heater.set_temperature_sensor(temp_sens))

    cg.add(heater.set_deadband(heater_config[CONF_DEADBAND]))
    cg.add(heater.set_overrun(heater_config[CONF_OVERRUN]))

    heater_out = await cg.get_variable(heater_config[CONF_OUTPUT])
    cg.add(heater.set_heater_output(heater_out))

    # Wire heater to primary pump and to both shutdown coordinators.
    cg.add(heater.set_primary_pump(primary_pump_var))
    cg.add(primary_pump_var.set_pool_heater(heater))
    cg.add(controller_var.set_pool_heater(heater))
