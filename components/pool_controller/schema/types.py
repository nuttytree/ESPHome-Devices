import esphome.codegen as cg
from esphome import automation
from esphome.components import binary_sensor, button, text_sensor, water_heater
from esphome.components import switch as esphome_switch
from esphome.components import select as esphome_select

pool_controller_ns = cg.esphome_ns.namespace("pool_controller")

PoolController = pool_controller_ns.class_("PoolController", cg.Component)
PoolHeater = pool_controller_ns.class_(
    "PoolHeater", water_heater.WaterHeater, cg.Component
)
ScheduleSelect = pool_controller_ns.class_(
    "ScheduleSelect", esphome_select.Select, cg.Component
)
PrimaryPumpSwitch = pool_controller_ns.class_(
    "PrimaryPumpSwitch", esphome_switch.Switch, cg.Component
)
AuxiliaryPumpSwitch = pool_controller_ns.class_(
    "AuxiliaryPumpSwitch", esphome_switch.Switch, cg.Component
)
PumpAnomalySwitch = pool_controller_ns.class_(
    "PumpAnomalySwitch", esphome_switch.Switch, cg.Component
)
PumpAnomalyStatusBinarySensor = pool_controller_ns.class_(
    "PumpAnomalyStatusBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PumpAnomalyResetButton = pool_controller_ns.class_(
    "PumpAnomalyResetButton", button.Button, cg.Component
)
PumpAnomalyReasonTextSensor = pool_controller_ns.class_(
    "PumpAnomalyReasonTextSensor", text_sensor.TextSensor, cg.Component
)
PumpNoCurrentBinarySensor = pool_controller_ns.class_(
    "PumpNoCurrentBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PumpCurrentStaleBinarySensor = pool_controller_ns.class_(
    "PumpCurrentStaleBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PumpFlowLossBinarySensor = pool_controller_ns.class_(
    "PumpFlowLossBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PumpUnexpectedFlowBinarySensor = pool_controller_ns.class_(
    "PumpUnexpectedFlowBinarySensor", binary_sensor.BinarySensor, cg.Component
)
PumpAnomalyTrigger = pool_controller_ns.class_(
    "PumpAnomalyTrigger", automation.Trigger.template(cg.std_string)
)
