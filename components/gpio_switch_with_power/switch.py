import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import switch, sensor
from esphome.const import (
    CONF_ID,
    CONF_INTERLOCK,
    CONF_PIN,
    CONF_RESTORE_MODE,
    CONF_POWER,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    ICON_POWER,
    CONF_UPDATE_INTERVAL,
)

gpio_power_ns = cg.esphome_ns.namespace("gpio_power")
GPIOSwitchWithPower = gpio_power_ns.class_("GPIOSwitchWithPower", switch.Switch, cg.PollingComponent)
GPIOSwitchRestoreMode = gpio_power_ns.enum("GPIOSwitchRestoreMode")

RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_OFF,
    "ALWAYS_ON": GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
}

CONF_INTERLOCK_WAIT_TIME = "interlock_wait_time"
CONF_DEVICE_WATTAGE = "device_wattage"

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GPIOSwitchWithPower),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
        cv.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_id(switch.Switch)),
        cv.Optional(
            CONF_INTERLOCK_WAIT_TIME, default="0ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement_=UNIT_WATT,
            accuracy_decimals_=1,
            device_class_=DEVICE_CLASS_POWER,
            state_class_=STATE_CLASS_MEASUREMENT,
            icon_=ICON_POWER,
        ).extend(
            {
                cv.Optional(CONF_DEVICE_WATTAGE): cv.positive_float,
                cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_INTERLOCK in config:
        interlock = []
        for it in config[CONF_INTERLOCK]:
            lock = await cg.get_variable(it)
            interlock.append(lock)
        cg.add(var.set_interlock(interlock))
        cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))

    if CONF_POWER in config:
        power_config = config[CONF_POWER]
        power_sensor = await sensor.new_sensor(power_config)
        cg.add(var.set_device_wattage(power_config[CONF_DEVICE_WATTAGE]))
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_update_interval(power_config[CONF_UPDATE_INTERVAL]))
    else:
        cg.add(var.set_update_interval(4294967295)) # uint32_t max
