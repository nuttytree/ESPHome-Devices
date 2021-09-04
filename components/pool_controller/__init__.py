import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time, select, switch
from esphome.const import (
    CONF_ID,
    CONF_TIME_ID,
    CONF_NAME,
    CONF_ICON,
    ICON_THERMOMETER,
    CONF_DISABLED_BY_DEFAULT,
)

DEPENDENCIES = ["time"]
AUTO_LOAD = ["select"]

pool_controller_ns = cg.esphome_ns.namespace("pool_controller")
PoolController = pool_controller_ns.class_("PoolController", cg.PollingComponent)

CONFIG_PUMP_SWITCH_ID = "pump_switch_id"
CONFIG_CLEANER_SWITCH_ID = "cleaner_switch_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(PoolController),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONFIG_PUMP_SWITCH_ID): cv.use_id(switch.Switch),
            cv.Required(CONFIG_CLEANER_SWITCH_ID): cv.use_id(switch.Switch),
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    pump_switch = await cg.get_variable(config[CONFIG_PUMP_SWITCH_ID])
    cg.add(var.set_pump_switch(pump_switch))

    cleaner_switch = await cg.get_variable(config[CONFIG_CLEANER_SWITCH_ID])
    cg.add(var.set_cleaner_switch(cleaner_switch))
