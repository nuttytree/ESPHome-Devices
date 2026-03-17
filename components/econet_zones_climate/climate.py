import esphome.codegen as cg
from esphome.components import climate, text_sensor
import esphome.config_validation as cv

CONF_OPERATING_MODE_SENSOR = "operating_mode_sensor"
CONF_ZONES = "zones"
CONF_AUTOMATIC_FAN_MODE = "automatic_fan_mode"
CONF_FAN_MODES = "fan_modes"
CONF_FAN_MODE = "fan_mode"
CONF_MINIMUM_TEMPERATURE_DELTA = "minimum_temperature_delta"
CONF_SUPPORTS_HEAT_MODE = "supports_heat_mode"
CONF_SUPPORTS_COOL_MODE = "supports_cool_mode"
CONF_SUPPORTS_CURRENT_HUMIDITY = "supports_current_humidity"
CONF_SUPPORTS_TARGET_HUMIDITY = "supports_target_humidity"

FAN_MODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FAN_MODE): cv.string,
        cv.Required(CONF_MINIMUM_TEMPERATURE_DELTA): cv.All(
            cv.temperature_delta, cv.Range(min=0.0)
        ),
    }
)

econet_zones_climate_ns = cg.esphome_ns.namespace("econet_zones_climate")
EcoNetZonesClimate = econet_zones_climate_ns.class_(
    "EcoNetZonesClimate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = (
    climate.climate_schema(EcoNetZonesClimate)
    .extend(
        {
            cv.Optional(CONF_SUPPORTS_HEAT_MODE, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_COOL_MODE, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_CURRENT_HUMIDITY, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_TARGET_HUMIDITY, default=True): cv.boolean,
            cv.Required(CONF_OPERATING_MODE_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_ZONES): cv.All(
                cv.ensure_list(cv.use_id(climate.Climate)),
                cv.Length(min=2),
            ),
            cv.Required(CONF_AUTOMATIC_FAN_MODE): cv.string,
            cv.Required(CONF_FAN_MODES): cv.All(
                cv.ensure_list(FAN_MODE_SCHEMA),
                cv.Length(min=1),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    sensor = await cg.get_variable(config[CONF_OPERATING_MODE_SENSOR])
    cg.add(var.set_operating_mode_sensor(sensor))

    for zone_id in config[CONF_ZONES]:
        zone = await cg.get_variable(zone_id)
        cg.add(var.add_zone(zone))

    cg.add(var.set_supports_heat_mode(config[CONF_SUPPORTS_HEAT_MODE]))
    cg.add(var.set_supports_cool_mode(config[CONF_SUPPORTS_COOL_MODE]))
    cg.add(var.set_supports_current_humidity(config[CONF_SUPPORTS_CURRENT_HUMIDITY]))
    cg.add(var.set_supports_target_humidity(config[CONF_SUPPORTS_TARGET_HUMIDITY]))
    cg.add(var.set_automatic_fan_mode(config[CONF_AUTOMATIC_FAN_MODE]))

    for entry in config[CONF_FAN_MODES]:
        cg.add(
            var.add_fan_mode(
                entry[CONF_MINIMUM_TEMPERATURE_DELTA], entry[CONF_FAN_MODE]
            )
        )
