import esphome.codegen as cg
from esphome.components import climate
from esphome.components.econet import (
    CONF_ECONET_ID,
    CONF_REQUEST_MOD,
    CONF_REQUEST_ONCE,
    CONF_SRC_ADDRESS,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
    request_mod as econet_request_mod,
)
import esphome.config_validation as cv

DEPENDENCIES = ["econet"]

CONF_OPERATING_MODE_DATAPOINT = "operating_mode_datapoint"
CONF_MODE_DATAPOINT = "mode_datapoint"
CONF_MODES = "modes"
CONF_ZONES = "zones"
CONF_IS_PRIMARY = "is_primary"
CONF_AUTOMATIC_FAN_MODE = "automatic_fan_mode"
CONF_FAN_MODES = "fan_modes"
CONF_FAN_MODE = "fan_mode"
CONF_MINIMUM_TEMPERATURE_DELTA = "minimum_temperature_delta"
CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TARGET_TEMPERATURE_LOW_DATAPOINT = "target_temperature_low_datapoint"
CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT = "target_temperature_high_datapoint"
CONF_FAN_MODE_DATAPOINT = "fan_mode_datapoint"
CONF_FAN_MODE_NO_SCHEDULE_DATAPOINT = "fan_mode_no_schedule_datapoint"
CONF_CURRENT_HUMIDITY_DATAPOINT = "current_humidity_datapoint"


def ensure_climate_mode_map(value):
    cv.check_not_templatable(value)
    options_map_schema = cv.Schema({cv.uint8_t: climate.validate_climate_mode})
    value = options_map_schema(value)
    all_values = list(value.keys())
    if len(all_values) != len(set(all_values)):
        raise cv.Invalid("Mode mapping values must be unique.")
    return value


def validate_zones(value):
    primary_count = sum(1 for z in value if z.get(CONF_IS_PRIMARY, False))
    if primary_count != 1:
        raise cv.Invalid(
            f"Exactly one zone must have is_primary: true (found {primary_count})"
        )
    return value


FAN_MODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FAN_MODE): cv.uint8_t,
        cv.Required(CONF_MINIMUM_TEMPERATURE_DELTA): cv.All(
            cv.temperature_delta, cv.Range(min=0.0)
        ),
    }
)

ZONE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_REQUEST_MOD): econet_request_mod,
        cv.Required(CONF_SRC_ADDRESS): cv.uint32_t,
        cv.Optional(CONF_IS_PRIMARY, default=False): cv.boolean,
    }
)

econet_zone_control_ns = cg.esphome_ns.namespace("econet_zone_control")
EcoNetZoneControl = econet_zone_control_ns.class_(
    "EcoNetZoneControl", climate.Climate, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    climate.climate_schema(EcoNetZoneControl)
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(
        {
            cv.Required(CONF_OPERATING_MODE_DATAPOINT): cv.string,
            cv.Required(CONF_MODE_DATAPOINT): cv.string,
            cv.Required(CONF_MODES): ensure_climate_mode_map,
            cv.Required(CONF_ZONES): cv.All(
                cv.ensure_list(ZONE_SCHEMA),
                cv.Length(min=2),
                validate_zones,
            ),
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_FAN_MODE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_FAN_MODE_NO_SCHEDULE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_CURRENT_HUMIDITY_DATAPOINT, default=""): cv.string,
            cv.Required(CONF_AUTOMATIC_FAN_MODE): cv.uint8_t,
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

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_request_mod(config[CONF_REQUEST_MOD]))
    cg.add(var.set_request_once(config[CONF_REQUEST_ONCE]))
    cg.add(var.set_src_adr(config[CONF_SRC_ADDRESS]))

    cg.add(var.set_operating_mode_id(config[CONF_OPERATING_MODE_DATAPOINT]))
    cg.add(var.set_mode_id(config[CONF_MODE_DATAPOINT]))
    modes = config[CONF_MODES]
    cg.add(var.init_modes(len(modes)))
    for key, value in modes.items():
        cg.add(var.add_mode(key, value))

    for zone_conf in config[CONF_ZONES]:
        cg.add(
            var.add_zone(
                zone_conf[CONF_REQUEST_MOD],
                zone_conf[CONF_SRC_ADDRESS],
                zone_conf[CONF_IS_PRIMARY],
            )
        )

    cg.add(var.set_current_temperature_id(config[CONF_CURRENT_TEMPERATURE_DATAPOINT]))
    cg.add(var.set_target_temperature_low_id(config[CONF_TARGET_TEMPERATURE_LOW_DATAPOINT]))
    cg.add(var.set_target_temperature_high_id(config[CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT]))
    cg.add(var.set_fan_mode_id(config[CONF_FAN_MODE_DATAPOINT]))
    cg.add(var.set_fan_mode_no_schedule_id(config[CONF_FAN_MODE_NO_SCHEDULE_DATAPOINT]))
    cg.add(var.set_current_humidity_id(config[CONF_CURRENT_HUMIDITY_DATAPOINT]))

    cg.add(var.set_automatic_fan_mode(config[CONF_AUTOMATIC_FAN_MODE]))

    for entry in config[CONF_FAN_MODES]:
        cg.add(
            var.add_fan_mode(
                entry[CONF_MINIMUM_TEMPERATURE_DELTA],
                entry[CONF_FAN_MODE],
            )
        )
