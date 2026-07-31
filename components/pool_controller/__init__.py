import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, time
from esphome.const import CONF_ID, CONF_NAME, CONF_TIME_ID

from .const import (
    CONF_PRIMARY_PUMP,
    CONF_AUXILIARY_PUMPS,
    CONF_SEQUENCE_DELAY,
    CONF_DISABLE_PUMPS_SENSOR,
    CONF_STATE_SAVE_INTERVAL,
    CONF_MAX_RESUME_AGE,
)
from .schema.types import PoolController
from .schema.pump import (
    PRIMARY_PUMP_SCHEMA,
    AUX_PUMP_SCHEMA,
    pump_to_code,
    schedule_select_to_code,
)
from .schema.heater import CONF_POOL_HEATER, POOL_HEATER_SCHEMA, heater_to_code

AUTO_LOAD = [
    "binary_sensor",
    "button",
    "select",
    "switch",
    "text_sensor",
    "water_heater",
]
DEPENDENCIES = ["time"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PoolController),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_PRIMARY_PUMP): PRIMARY_PUMP_SCHEMA,
        cv.Optional(CONF_AUXILIARY_PUMPS, default=[]): cv.ensure_list(AUX_PUMP_SCHEMA),
        cv.Optional(
            CONF_SEQUENCE_DELAY, default="2s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DISABLE_PUMPS_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(
            CONF_STATE_SAVE_INTERVAL, default="5min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RESUME_AGE, default="10min"): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(min=cv.TimePeriod(seconds=1)),
        ),
        cv.Optional(CONF_POOL_HEATER): POOL_HEATER_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    rtc = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_rtc(rtc))

    delay_ms = config[CONF_SEQUENCE_DELAY]
    cg.add(var.set_sequence_delay(delay_ms))
    cg.add(var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL]))
    cg.add(var.set_max_resume_age(config[CONF_MAX_RESUME_AGE]))

    disable_sensor = None
    if CONF_DISABLE_PUMPS_SENSOR in config:
        disable_sensor = await cg.get_variable(config[CONF_DISABLE_PUMPS_SENSOR])
        cg.add(var.set_disable_pumps_sensor(disable_sensor))

    # Primary pump
    primary_config = config[CONF_PRIMARY_PUMP]
    primary = cg.new_Pvariable(primary_config[CONF_ID])
    await pump_to_code(primary, primary_config, delay_ms, disable_sensor, rtc)
    cg.add(var.set_primary_pump(primary))
    await schedule_select_to_code(primary, primary_config, "Always")

    # Auxiliary pumps
    primary_name = primary_config.get(CONF_NAME, "Primary Pump")
    aux_pumps = []
    for aux_config in config.get(CONF_AUXILIARY_PUMPS, []):
        aux = cg.new_Pvariable(aux_config[CONF_ID])
        await pump_to_code(aux, aux_config, delay_ms, disable_sensor, rtc)
        cg.add(aux.set_primary_pump(primary))
        await schedule_select_to_code(
            aux, aux_config, f"When {primary_name} is Running"
        )
        aux_pumps.append(aux)

    if aux_pumps:
        cg.add(var.set_auxiliary_pumps(aux_pumps))

    # Pool heater (optional)
    if CONF_POOL_HEATER in config:
        await heater_to_code(var, config[CONF_POOL_HEATER], primary)
