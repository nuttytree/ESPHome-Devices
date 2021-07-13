from esphome.components import fan
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID, CONF_SWITCH_DATAPOINT
from esphome.components.tuya import CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_MAX_VALUE = "dimmer_max_value"

tuya_ns = cg.esphome_ns.namespace("tuya")
TuyaFan = tuya_ns.class_("TuyaDimmerAsFan", cg.Component)

CONFIG_SCHEMA = cv.All(
    fan.FAN_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaFan),
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_DIMMER_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TUYA_ID])
    state = await fan.create_fan_state(config)

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID], parent, state
    )
    await cg.register_component(var, config)

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    cg.add(var.set_dimmer_id(config[CONF_DIMMER_DATAPOINT]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_dimmer_max_value(config[CONF_MAX_VALUE]))
