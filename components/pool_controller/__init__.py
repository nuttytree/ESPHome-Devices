import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, sensor, output, time, water_heater
from esphome.components import switch as esphome_switch
from esphome.components import select as esphome_select
from esphome.components.time import validate_cron_days_of_week
from esphome.const import (
    CONF_DAYS_OF_WEEK,
    CONF_ID,
    CONF_NAME,
    CONF_OUTPUT,
    CONF_TIME_ID,
    CONF_TRIGGER_ID,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_SECOND,
)
from .const import (
    CONF_PRIMARY_PUMP,
    CONF_AUXILIARY_PUMPS,
    CONF_SCHEDULES,
    CONF_RUNTIMES,
    CONF_START_TIME,
    CONF_END_TIME,
    CONF_MINUTES_PER_HOUR,
    CONF_SCHEDULE_SELECT,
    CONF_SEQUENCE_DELAY,
    CONF_DISABLE_PUMPS_SENSOR,
    CONF_CURRENT_SENSOR,
    CONF_ENABLE_ANOMALY_DETECTION,
    CONF_ANOMALY_THRESHOLD_PCT,
    CONF_LEARNING_SAMPLES,
    CONF_ON_ANOMALY,
    CONF_USE_CURRENT_FOR_STATE,
    CONF_CURRENT_ON_THRESHOLD,
    CONF_FLOW_SENSOR,
    CONF_FLOW_TIMEOUT,
)

AUTO_LOAD = ["binary_sensor", "select", "switch", "water_heater"]
DEPENDENCIES = ["time"]

pool_controller_ns = cg.esphome_ns.namespace("pool_controller")
PoolController = pool_controller_ns.class_("PoolController", cg.Component)
PoolHeater = pool_controller_ns.class_("PoolHeater", water_heater.WaterHeater, cg.Component)
ScheduleSelect = pool_controller_ns.class_("ScheduleSelect", esphome_select.Select, cg.Component)
PrimaryPumpSwitch = pool_controller_ns.class_("PrimaryPumpSwitch", esphome_switch.Switch, cg.Component)
AuxiliaryPumpSwitch = pool_controller_ns.class_("AuxiliaryPumpSwitch", esphome_switch.Switch, cg.Component)
PumpAnomalyTrigger = pool_controller_ns.class_(
    "PumpAnomalyTrigger", automation.Trigger.template(cg.std_string)
)

# ── Time / schedule helpers ────────────────────────────────────────────────────

def _time_to_minutes(time_val):
    """Convert a time_of_day dict to total minutes since midnight."""
    return time_val[CONF_HOUR] * 60 + time_val[CONF_MINUTE]


def _time_to_str(time_val):
    """Format a time_of_day dict for display."""
    return f"{time_val[CONF_HOUR]}:{time_val[CONF_MINUTE]:02d}"


def _validate_half_hour(time_val):
    """Validate that a time_of_day value falls on the hour or half-hour."""
    if time_val[CONF_MINUTE] not in (0, 30):
        raise cv.Invalid(
            f"Minutes must be 00 or 30 (on the hour or half-hour), got {time_val[CONF_MINUTE]:02d}"
        )
    return time_val


HALF_HOUR_TIME = cv.All(cv.time_of_day, _validate_half_hour)

_MIDNIGHT_END = {CONF_HOUR: 24, CONF_MINUTE: 0, CONF_SECOND: 0}


def _validate_half_hour_end_time(value):
    """Like HALF_HOUR_TIME but also accepts 24:00 to mean end of day."""
    value = cv.string(value)
    if value in ("24:00", "24:0"):
        return _MIDNIGHT_END
    return _validate_half_hour(cv.time_of_day(value))


HALF_HOUR_END_TIME = _validate_half_hour_end_time

ALL_DAYS = set(range(1, 8))


def _days_to_mask(runtime):
    """Convert days_of_week list to a bitmask (bit0=Sun, bit1=Mon, ..., bit6=Sat)."""
    dow = runtime.get(CONF_DAYS_OF_WEEK)
    if dow is None:
        return 0x7F
    mask = 0
    for d in dow:
        mask |= 1 << (d - 1)
    return mask


def _runtime_days(runtime):
    """Return the set of days a runtime applies to (defaults to all days)."""
    dow = runtime.get(CONF_DAYS_OF_WEEK)
    return set(dow) if dow is not None else ALL_DAYS


def _validate_runtime(runtime):
    """Validate that start_time is strictly before end_time within a runtime."""
    start = _time_to_minutes(runtime[CONF_START_TIME])
    end = _time_to_minutes(runtime[CONF_END_TIME])
    if start >= end:
        raise cv.Invalid(
            f"start_time ({_time_to_str(runtime[CONF_START_TIME])}) must be before "
            f"end_time ({_time_to_str(runtime[CONF_END_TIME])})"
        )
    return runtime


def _validate_no_runtime_overlaps(schedule):
    """Validate that no two runtimes in the same schedule overlap on the same days."""
    runtimes = schedule[CONF_RUNTIMES]
    for i in range(len(runtimes)):
        for j in range(i + 1, len(runtimes)):
            a, b = runtimes[i], runtimes[j]
            a_start = _time_to_minutes(a[CONF_START_TIME])
            a_end = _time_to_minutes(a[CONF_END_TIME])
            b_start = _time_to_minutes(b[CONF_START_TIME])
            b_end = _time_to_minutes(b[CONF_END_TIME])
            if a_start < b_end and b_start < a_end:
                if _runtime_days(a) & _runtime_days(b):
                    raise cv.Invalid(
                        f"Runtime {i} ({_time_to_str(a[CONF_START_TIME])}-"
                        f"{_time_to_str(a[CONF_END_TIME])}) overlaps with "
                        f"runtime {j} ({_time_to_str(b[CONF_START_TIME])}-"
                        f"{_time_to_str(b[CONF_END_TIME])}) on shared days"
                    )
    return schedule


RUNTIME_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_START_TIME): HALF_HOUR_TIME,
            cv.Required(CONF_END_TIME): HALF_HOUR_END_TIME,
            cv.Required(CONF_MINUTES_PER_HOUR): cv.All(
                cv.int_, cv.Range(min=1, max=60)
            ),
            cv.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
        }
    ),
    _validate_runtime,
)

SCHEDULE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.string,
            cv.Required(CONF_RUNTIMES): cv.All(
                cv.ensure_list(RUNTIME_SCHEMA),
                cv.Length(min=1, msg="Each schedule must define at least one runtime"),
            ),
        }
    ),
    _validate_no_runtime_overlaps,
)


def _validate_unique_schedule_names(schedules):
    """Validate that all schedule names are unique."""
    seen = {}
    for i, schedule in enumerate(schedules):
        name = schedule[CONF_NAME]
        if name in seen:
            raise cv.Invalid(
                f"Duplicate schedule name '{name}' at index {i} (first defined at index {seen[name]})"
            )
        seen[name] = i
    return schedules


def _validate_sensor_config(config):
    """Require current_sensor when any feature that reads current is enabled."""
    needs_sensor = (
        config.get(CONF_USE_CURRENT_FOR_STATE, False)
        or config.get(CONF_ENABLE_ANOMALY_DETECTION, False)
    )
    if needs_sensor and CONF_CURRENT_SENSOR not in config:
        raise cv.Invalid(
            "current_sensor is required when use_current_for_state: true "
            "or enable_anomaly_detection: true",
            path=[CONF_CURRENT_SENSOR],
        )
    return config


# ── Per-pump schema (shared by primary and auxiliary) ──────────────────────────

PUMP_SCHEMA = {
    cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
    cv.Optional(CONF_SCHEDULES, default=[]): cv.All(
        cv.ensure_list(SCHEDULE_SCHEMA),
        _validate_unique_schedule_names,
    ),
    cv.Required(CONF_SCHEDULE_SELECT): esphome_select.select_schema(ScheduleSelect),
    cv.Optional(CONF_CURRENT_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_USE_CURRENT_FOR_STATE, default=False): cv.boolean,
    cv.Optional(CONF_CURRENT_ON_THRESHOLD, default=0.5): cv.positive_float,
    cv.Optional(CONF_ENABLE_ANOMALY_DETECTION, default=False): cv.boolean,
    cv.Optional(CONF_ANOMALY_THRESHOLD_PCT, default=10): cv.int_range(min=1, max=100),
    cv.Optional(CONF_LEARNING_SAMPLES, default=200): cv.int_range(min=10, max=10000),
    cv.Optional(CONF_ON_ANOMALY): automation.validate_automation(
        {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PumpAnomalyTrigger)}
    ),
    cv.Optional(CONF_FLOW_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_FLOW_TIMEOUT, default="2s"): cv.positive_time_period_milliseconds,
}

PRIMARY_PUMP_SCHEMA = cv.All(
    esphome_switch.switch_schema(PrimaryPumpSwitch)
    .extend({cv.GenerateID(): cv.declare_id(PrimaryPumpSwitch)})
    .extend(PUMP_SCHEMA),
    _validate_sensor_config,
)

AUX_PUMP_SCHEMA = cv.All(
    esphome_switch.switch_schema(AuxiliaryPumpSwitch)
    .extend({cv.GenerateID(): cv.declare_id(AuxiliaryPumpSwitch)})
    .extend(PUMP_SCHEMA),
    _validate_sensor_config,
)

# ── Pool heater schema ─────────────────────────────────────────────────────────
# Constants are defined inline here because they are only used in this file.
_CONF_POOL_HEATER = "pool_heater"
_CONF_TEMPERATURE_SENSOR = "temperature_sensor"
_CONF_DEADBAND = "deadband"
_CONF_OVERRUN = "overrun"

# Deadband / overrun default: 0.5 °F expressed in °C.
_HEATER_DELTA_C = 0.5 * 5.0 / 9.0

# Temperature range and step defaults live in the C++ field initializers
# (60 °F / 15.56 °C min, 90 °F / 32.22 °C max, 1 °F / 0.556 °C step).
# They can be overridden via the standard `visual:` block in the YAML.
POOL_HEATER_SCHEMA = (
    water_heater.water_heater_schema(PoolHeater)
    .extend(
        {
            cv.Required(_CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(_CONF_DEADBAND, default=_HEATER_DELTA_C): cv.temperature_delta,
            cv.Optional(_CONF_OVERRUN, default=_HEATER_DELTA_C): cv.temperature_delta,
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

# ── Top-level component schema ───────────────────────────────────────────────

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PoolController),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONF_PRIMARY_PUMP): PRIMARY_PUMP_SCHEMA,
            cv.Optional(CONF_AUXILIARY_PUMPS, default=[]): cv.ensure_list(AUX_PUMP_SCHEMA),
            cv.Optional(CONF_SEQUENCE_DELAY, default="2s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DISABLE_PUMPS_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(_CONF_POOL_HEATER): POOL_HEATER_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

# ── Codegen helpers ────────────────────────────────────────────────────────────

async def _pump_to_code(var, pump_config, delay_ms, disable_sensor):
    """Register a pump switch and emit all its configuration calls."""
    await esphome_switch.register_switch(var, pump_config)
    await cg.register_component(var, pump_config)

    out = await cg.get_variable(pump_config[CONF_OUTPUT])
    cg.add(var.set_output(out))
    cg.add(var.set_sequence_delay(delay_ms))

    if disable_sensor is not None:
        cg.add(var.set_disable_pumps_sensor(disable_sensor))

    for schedule in pump_config[CONF_SCHEDULES]:
        cg.add(var.add_schedule(schedule[CONF_NAME]))
        for runtime in schedule[CONF_RUNTIMES]:
            cg.add(var.add_runtime_to_last_schedule(
                _time_to_minutes(runtime[CONF_START_TIME]),
                _time_to_minutes(runtime[CONF_END_TIME]),
                runtime[CONF_MINUTES_PER_HOUR],
                _days_to_mask(runtime),
            ))

    if CONF_CURRENT_SENSOR in pump_config:
        current_sens = await cg.get_variable(pump_config[CONF_CURRENT_SENSOR])
        cg.add(var.set_current_sensor(current_sens))

    if pump_config.get(CONF_USE_CURRENT_FOR_STATE, False):
        cg.add(var.set_use_current_for_state(True))
        cg.add(var.set_current_on_threshold(pump_config[CONF_CURRENT_ON_THRESHOLD]))

    if pump_config.get(CONF_ENABLE_ANOMALY_DETECTION, False):
        cg.add(var.set_enable_anomaly_detection(True))
        cg.add(var.set_anomaly_threshold_pct(pump_config[CONF_ANOMALY_THRESHOLD_PCT]))
        cg.add(var.set_learning_samples(pump_config[CONF_LEARNING_SAMPLES]))
        for conf in pump_config.get(CONF_ON_ANOMALY, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_FLOW_SENSOR in pump_config:
        flow_sens = await cg.get_variable(pump_config[CONF_FLOW_SENSOR])
        cg.add(var.set_flow_sensor(flow_sens))
        cg.add(var.set_flow_timeout_ms(pump_config[CONF_FLOW_TIMEOUT]))


async def _schedule_select_to_code(pump_var, pump_config, builtin_last_option):
    """Create and register the ScheduleSelect entity for a pump."""
    sel_conf = pump_config[CONF_SCHEDULE_SELECT]
    user_names = [s[CONF_NAME] for s in pump_config[CONF_SCHEDULES]]
    options = ["Off"] + user_names + [builtin_last_option]
    sel = cg.new_Pvariable(sel_conf[CONF_ID])
    await esphome_select.register_select(sel, sel_conf, options=options)
    await cg.register_component(sel, sel_conf)
    cg.add(sel.set_pump_switch(pump_var))


# ── Top-level to_code ──────────────────────────────────────────────────────────

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    rtc = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_rtc(rtc))

    delay_ms = config[CONF_SEQUENCE_DELAY]
    cg.add(var.set_sequence_delay(delay_ms))

    disable_sensor = None
    if CONF_DISABLE_PUMPS_SENSOR in config:
        disable_sensor = await cg.get_variable(config[CONF_DISABLE_PUMPS_SENSOR])
        cg.add(var.set_disable_pumps_sensor(disable_sensor))

    # Primary pump
    primary_config = config[CONF_PRIMARY_PUMP]
    primary = cg.new_Pvariable(primary_config[CONF_ID])
    await _pump_to_code(primary, primary_config, delay_ms, disable_sensor)
    cg.add(var.set_primary_pump(primary))
    await _schedule_select_to_code(primary, primary_config, "Always")

    # Auxiliary pumps
    primary_name = primary_config.get(CONF_NAME, "Primary Pump")
    aux_pumps = []
    for aux_config in config.get(CONF_AUXILIARY_PUMPS, []):
        aux = cg.new_Pvariable(aux_config[CONF_ID])
        await _pump_to_code(aux, aux_config, delay_ms, disable_sensor)
        cg.add(aux.set_primary_pump(primary))
        await _schedule_select_to_code(aux, aux_config, f"When {primary_name} is Running")
        aux_pumps.append(aux)

    if aux_pumps:
        cg.add(var.set_auxiliary_pumps(aux_pumps))

    # Pool heater (optional)
    if _CONF_POOL_HEATER in config:
        heater_config = config[_CONF_POOL_HEATER]
        heater = await water_heater.new_water_heater(heater_config)
        await cg.register_component(heater, heater_config)

        temp_sens = await cg.get_variable(heater_config[_CONF_TEMPERATURE_SENSOR])
        cg.add(heater.set_temperature_sensor(temp_sens))

        cg.add(heater.set_deadband(heater_config[_CONF_DEADBAND]))
        cg.add(heater.set_overrun(heater_config[_CONF_OVERRUN]))

        heater_out = await cg.get_variable(heater_config[CONF_OUTPUT])
        cg.add(heater.set_heater_output(heater_out))

        # Wire heater to primary pump and to both shutdown coordinators.
        cg.add(heater.set_primary_pump(primary))
        cg.add(primary.set_pool_heater(heater))
        cg.add(var.set_pool_heater(heater))
