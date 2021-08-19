import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.components import light, output, sensor
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_OUTPUT,
    CONF_POWER,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    ICON_POWER,
    CONF_UPDATE_INTERVAL,
)

CONF_LIGHT_WATTAGE = "light_wattage"

binary_power_ns = cg.esphome_ns.namespace("binary_power")
LightWithPower = binary_power_ns.class_("BinaryLightWithPower", light.LightOutput, cg.PollingComponent)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LightWithPower),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_POWER,
        ).extend(
            {
                cv.Optional(CONF_LIGHT_WATTAGE): cv.positive_float,
                cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
    if CONF_POWER in config:
        power_config = config[CONF_POWER]
        power_sensor = await sensor.new_sensor(power_config)
        cg.add(var.set_light_wattage(power_config[CONF_LIGHT_WATTAGE]))
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_update_interval(power_config[CONF_UPDATE_INTERVAL]))
    else:
        cg.add(var.set_update_interval(4294967295)) # uint32_t max
