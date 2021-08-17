from esphome.components import fan, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components.tuya import CONF_TUYA_ID, Tuya
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_SWITCH_DATAPOINT,
    CONF_POWER,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    ICON_POWER,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["tuya"]

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_MAX_VALUE = "dimmer_max_value"
CONF_FAN_WATTAGE = "fan_wattage"

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
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement_=UNIT_WATT,
                accuracy_decimals_=1,
                device_class_=DEVICE_CLASS_POWER,
                state_class_=STATE_CLASS_MEASUREMENT,
                icon_=ICON_POWER,
            ).extend(
                {
                    cv.Optional(CONF_FAN_WATTAGE): cv.positive_float,
                    cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
                }
            ),
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
    if CONF_POWER in config:
        power_config = config[CONF_POWER]
        power_sensor = await sensor.new_sensor(power_config)
        cg.add(var.set_fan_wattage(power_config[CONF_FAN_WATTAGE]))
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_update_interval(power_config[CONF_UPDATE_INTERVAL]))
    else:
        cg.add(var.set_update_interval(4294967295)) # uint32_t max
