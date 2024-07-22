import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time, sensor, switch
from esphome.const import (
    CONF_ID,
    CONF_TIME_ID,
    CONF_MAX_CURRENT,
)

DEPENDENCIES = ["time"]
AUTO_LOAD = ["select"]

pool_controller_ns = cg.esphome_ns.namespace("pool_controller")
PoolController = pool_controller_ns.class_("PoolController", cg.PollingComponent)

CONF_PUMP = "pump"
CONF_CLEANER = "cleaner"
CONF_PUMP_SELECT = "pump_select"
CONF_SWITCH_ID = "switch_id"
CONF_CURRENT_ID = "current_id"
CONF_MIN_CURRENT = "min_current"
CONF_MAX_OUT_OF_RANGE_DURATION = "max_out_of_range_duration"

PUMP_SWITCH_CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_SWITCH_ID): cv.use_id(switch.Switch),
            cv.Required(CONF_CURRENT_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_MIN_CURRENT): cv.positive_float,
            cv.Required(CONF_MAX_CURRENT): cv.positive_float,
            cv.Required(CONF_MAX_OUT_OF_RANGE_DURATION): cv.positive_time_period_milliseconds,
        }
    )
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(PoolController),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONF_PUMP): PUMP_SWITCH_CONFIG_SCHEMA,
            cv.Required(CONF_CLEANER): PUMP_SWITCH_CONFIG_SCHEMA,
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    pump_config = config[CONF_PUMP]
    
    pump_switch = await cg.get_variable(pump_config[CONF_SWITCH_ID])
    cg.add(var.set_pump_switch(pump_switch))
    
    pump_current = await cg.get_variable(pump_config[CONF_CURRENT_ID])
    cg.add(var.set_pump_current_monitoring(pump_current, pump_config[CONF_MIN_CURRENT], pump_config[CONF_MAX_CURRENT], pump_config[CONF_MAX_OUT_OF_RANGE_DURATION]))

    cleaner_config = config[CONF_CLEANER]
    
    cleaner_switch = await cg.get_variable(cleaner_config[CONF_SWITCH_ID])
    cg.add(var.set_cleaner_switch(cleaner_switch))
    
    cleaner_current = await cg.get_variable(cleaner_config[CONF_CURRENT_ID])
    cg.add(var.set_cleaner_current_monitoring(cleaner_current, cleaner_config[CONF_MIN_CURRENT], cleaner_config[CONF_MAX_CURRENT], cleaner_config[CONF_MAX_OUT_OF_RANGE_DURATION]))
