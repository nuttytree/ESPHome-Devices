import esphome.codegen as cg
from esphome.components import sensor, water_heater
import esphome.config_validation as cv
from esphome.const import CONF_MAX_TEMPERATURE, CONF_MIN_TEMPERATURE

CONF_SOURCE_WATER_HEATER = "source_water_heater"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_TEMPERATURE_SENSOR_OFFSET = "temperature_sensor_offset"
CONF_DEAD_BAND = "dead_band"
CONF_OVER_RUN = "over_run"
CONF_TARGET_TEMPERATURE_STEP = "target_temperature_step"

_MIN_TEMP_MIN_C = 30.0
_MIN_TEMP_MAX_C = 50.0
_MIN_TEMP_DEFAULT_C = 43.333333333333336
_MAX_TEMP_MIN_C = 45.0
_MAX_TEMP_MAX_C = 75.0
_MAX_TEMP_DEFAULT_C = 65.55555555555556
_MIN_MAX_TEMP_GAP_C = 5.0

_TEMP_OFFSET_MIN_C = 0.0
_TEMP_OFFSET_MAX_C = 20.0

_TEMP_STEP_MIN = 0.5
_TEMP_STEP_MAX = 2.0
_TEMP_STEP_DEFAULT = 1.0

_DEAD_BAND_OVER_RUN_MIN_SUM_C = 0.5
_HEATING_DELTA_MIN_C = 0.0
_HEATING_DELTA_MAX_C = 20.0

high_temp_water_heater_ns = cg.esphome_ns.namespace("high_temp_water_heater")
HighTempWaterHeater = high_temp_water_heater_ns.class_(
    "HighTempWaterHeater", water_heater.WaterHeater, cg.Component
)


def _validate_config(config):
    if CONF_TEMPERATURE_SENSOR in config and CONF_TEMPERATURE_SENSOR_OFFSET not in config:
        raise cv.Invalid(
            f"{CONF_TEMPERATURE_SENSOR_OFFSET} is required when {CONF_TEMPERATURE_SENSOR} is set"
        )
    min_temp = config[CONF_MIN_TEMPERATURE]
    max_temp = config[CONF_MAX_TEMPERATURE]
    if max_temp < min_temp + _MIN_MAX_TEMP_GAP_C:
        raise cv.Invalid(
            f"max_temperature ({max_temp}) must be at least min_temperature ({min_temp}) + {_MIN_MAX_TEMP_GAP_C}"
        )
    dead_band = config[CONF_DEAD_BAND]
    over_run = config[CONF_OVER_RUN]
    if dead_band + over_run < _DEAD_BAND_OVER_RUN_MIN_SUM_C:
        raise cv.Invalid(
            f"The sum of dead_band ({dead_band}) and over_run ({over_run}) must be at least {_DEAD_BAND_OVER_RUN_MIN_SUM_C}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    water_heater.water_heater_schema(HighTempWaterHeater)
    .extend(
        {
            cv.Required(CONF_SOURCE_WATER_HEATER): cv.use_id(water_heater.WaterHeater),
            cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_TEMPERATURE_SENSOR_OFFSET): cv.All(
                cv.temperature_delta, cv.float_range(min=_TEMP_OFFSET_MIN_C, max=_TEMP_OFFSET_MAX_C)
            ),
            cv.Optional(CONF_MIN_TEMPERATURE, default=_MIN_TEMP_DEFAULT_C): cv.All(
                cv.temperature, cv.float_range(min=_MIN_TEMP_MIN_C, max=_MIN_TEMP_MAX_C)
            ),
            cv.Optional(CONF_MAX_TEMPERATURE, default=_MAX_TEMP_DEFAULT_C): cv.All(
                cv.temperature, cv.float_range(min=_MAX_TEMP_MIN_C, max=_MAX_TEMP_MAX_C)
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE_STEP, default=_TEMP_STEP_DEFAULT): cv.All(
                cv.float_, cv.float_range(min=_TEMP_STEP_MIN, max=_TEMP_STEP_MAX)
            ),
            cv.Required(CONF_DEAD_BAND): cv.All(
                cv.temperature_delta, cv.float_range(min=_HEATING_DELTA_MIN_C, max=_HEATING_DELTA_MAX_C)
            ),
            cv.Required(CONF_OVER_RUN): cv.All(
                cv.temperature_delta, cv.float_range(min=_HEATING_DELTA_MIN_C, max=_HEATING_DELTA_MAX_C)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_config,
)


async def to_code(config):
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_WATER_HEATER])
    cg.add(var.set_source_water_heater(source))

    if lower_tank_sens := config.get(CONF_TEMPERATURE_SENSOR):
        sens = await cg.get_variable(lower_tank_sens)
        cg.add(var.set_temperature_sensor(sens))

    if CONF_TEMPERATURE_SENSOR_OFFSET in config:
        cg.add(var.set_temperature_sensor_offset(config[CONF_TEMPERATURE_SENSOR_OFFSET]))
    cg.add(var.set_min_temperature(config[CONF_MIN_TEMPERATURE]))
    cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))
    cg.add(var.set_target_temperature_step(config[CONF_TARGET_TEMPERATURE_STEP]))
    cg.add(var.set_dead_band(config[CONF_DEAD_BAND]))
    cg.add(var.set_over_run(config[CONF_OVER_RUN]))
