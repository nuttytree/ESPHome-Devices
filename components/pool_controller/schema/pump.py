import uuid

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, button, output, sensor
from esphome.components import switch as esphome_switch
from esphome.components import select as esphome_select
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_OUTPUT,
    DEVICE_CLASS_PROBLEM,
    CONF_TRIGGER_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from ..const import (
    CONF_SCHEDULES,
    CONF_RUNTIMES,
    CONF_START_TIME,
    CONF_END_TIME,
    CONF_MINUTES_PER_HOUR,
    CONF_SCHEDULE_SELECT,
    CONF_CURRENT_SENSOR,
    CONF_ANOMALY_DETECTION_SWITCH,
    CONF_ANOMALY_BASELINE_RESET_BUTTON,
    CONF_ANOMALY_STATUS_BINARY_SENSOR,
    CONF_ANOMALY_THRESHOLD_PCT,
    CONF_LEARNING_SAMPLES,
    CONF_ON_ANOMALY,
    CONF_CURRENT_ON_THRESHOLD,
    CONF_NO_CURRENT_BINARY_SENSOR,
    CONF_FLOW_SENSOR,
    CONF_FLOW_TIMEOUT,
    CONF_FLOW_LOSS_BINARY_SENSOR,
    CONF_UNEXPECTED_FLOW_BINARY_SENSOR,
)
from .types import (
    ScheduleSelect,
    PrimaryPumpSwitch,
    AuxiliaryPumpSwitch,
    PumpAnomalySwitch,
    PumpAnomalyStatusBinarySensor,
    PumpAnomalyResetButton,
    PumpNoCurrentBinarySensor,
    PumpFlowLossBinarySensor,
    PumpUnexpectedFlowBinarySensor,
    PumpAnomalyTrigger,
)
from .schedule import (
    SCHEDULE_SCHEMA,
    _validate_unique_schedule_names,
    _time_to_minutes,
    _days_to_mask,
)

DEFAULT_ANOMALY_DETECTION_SWITCH_PREFIX = (
    "__pool_controller_anomaly_detection_switch_default_name__"
)
DEFAULT_ANOMALY_STATUS_BINARY_SENSOR_PREFIX = (
    "__pool_controller_anomaly_status_binary_sensor_default_name__"
)
DEFAULT_ANOMALY_BASELINE_RESET_BUTTON_PREFIX = (
    "__pool_controller_anomaly_baseline_reset_button_default_name__"
)
DEFAULT_NO_CURRENT_BINARY_SENSOR_PREFIX = (
    "__pool_controller_no_current_binary_sensor_default_name__"
)
DEFAULT_FLOW_LOSS_BINARY_SENSOR_PREFIX = (
    "__pool_controller_flow_loss_binary_sensor_default_name__"
)
DEFAULT_UNEXPECTED_FLOW_BINARY_SENSOR_PREFIX = (
    "__pool_controller_unexpected_flow_binary_sensor_default_name__"
)


def _validate_sensor_config(config):
    """Require current_sensor when any feature that reads current is enabled."""
    needs_sensor = bool(config.get(CONF_ON_ANOMALY))
    if needs_sensor and CONF_CURRENT_SENSOR not in config:
        raise cv.Invalid(
            "current_sensor is required when on_anomaly is configured",
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
    cv.Optional(
        CONF_ANOMALY_DETECTION_SWITCH,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_ANOMALY_DETECTION_SWITCH_PREFIX}{uuid.uuid4()}"
        },
    ): esphome_switch.switch_schema(
        PumpAnomalySwitch,
        default_restore_mode="RESTORE_DEFAULT_ON",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:alert-circle-outline",
    ),
    cv.Optional(
        CONF_ANOMALY_BASELINE_RESET_BUTTON,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_ANOMALY_BASELINE_RESET_BUTTON_PREFIX}{uuid.uuid4()}"
        },
    ): button.button_schema(
        PumpAnomalyResetButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:restart-alert",
    ),
    cv.Optional(
        CONF_ANOMALY_STATUS_BINARY_SENSOR,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_ANOMALY_STATUS_BINARY_SENSOR_PREFIX}{uuid.uuid4()}"
        },
    ): binary_sensor.binary_sensor_schema(
        PumpAnomalyStatusBinarySensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:alert-octagon",
    ),
    cv.Optional(CONF_CURRENT_ON_THRESHOLD, default=0.5): cv.positive_float,
    cv.Optional(
        CONF_NO_CURRENT_BINARY_SENSOR,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_NO_CURRENT_BINARY_SENSOR_PREFIX}{uuid.uuid4()}"
        },
    ): binary_sensor.binary_sensor_schema(
        PumpNoCurrentBinarySensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:flash-off",
    ),
    cv.Optional(CONF_ANOMALY_THRESHOLD_PCT, default=10): cv.int_range(min=1, max=100),
    cv.Optional(CONF_LEARNING_SAMPLES, default=200): cv.int_range(min=10, max=10000),
    cv.Optional(CONF_ON_ANOMALY): automation.validate_automation(
        {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PumpAnomalyTrigger)}
    ),
    cv.Optional(CONF_FLOW_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_FLOW_TIMEOUT, default="2s"): cv.positive_time_period_milliseconds,
    cv.Optional(
        CONF_FLOW_LOSS_BINARY_SENSOR,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_FLOW_LOSS_BINARY_SENSOR_PREFIX}{uuid.uuid4()}"
        },
    ): binary_sensor.binary_sensor_schema(
        PumpFlowLossBinarySensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:water-alert",
    ),
    cv.Optional(
        CONF_UNEXPECTED_FLOW_BINARY_SENSOR,
        default=lambda: {
            CONF_NAME: f"{DEFAULT_UNEXPECTED_FLOW_BINARY_SENSOR_PREFIX}{uuid.uuid4()}"
        },
    ): binary_sensor.binary_sensor_schema(
        PumpUnexpectedFlowBinarySensor,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:water-alert-outline",
    ),
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


# ── Codegen helpers ────────────────────────────────────────────────────────────


async def pump_to_code(var, pump_config, delay_ms, disable_sensor):
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
            cg.add(
                var.add_runtime_to_last_schedule(
                    _time_to_minutes(runtime[CONF_START_TIME]),
                    _time_to_minutes(runtime[CONF_END_TIME]),
                    runtime[CONF_MINUTES_PER_HOUR],
                    _days_to_mask(runtime),
                )
            )

    if CONF_CURRENT_SENSOR in pump_config:
        current_sens = await cg.get_variable(pump_config[CONF_CURRENT_SENSOR])
        cg.add(var.set_current_sensor(current_sens))
        cg.add(var.set_current_on_threshold(pump_config[CONF_CURRENT_ON_THRESHOLD]))

        diag_conf = pump_config[CONF_ANOMALY_DETECTION_SWITCH]
        if diag_conf.get(CONF_NAME, "").startswith(
            DEFAULT_ANOMALY_DETECTION_SWITCH_PREFIX
        ):
            diag_conf[CONF_NAME] = f"{pump_config[CONF_NAME]} Anomaly Detection"
        diag = cg.new_Pvariable(diag_conf[CONF_ID])
        await esphome_switch.register_switch(diag, diag_conf)
        await cg.register_component(diag, diag_conf)
        cg.add(diag.set_pump(var))

        if CONF_ANOMALY_BASELINE_RESET_BUTTON in pump_config:
            reset_conf = pump_config[CONF_ANOMALY_BASELINE_RESET_BUTTON]
            if reset_conf.get(CONF_NAME, "").startswith(
                DEFAULT_ANOMALY_BASELINE_RESET_BUTTON_PREFIX
            ):
                reset_conf[CONF_NAME] = (
                    f"{pump_config[CONF_NAME]} Reset Anomaly Baseline"
                )
            reset_button = cg.new_Pvariable(reset_conf[CONF_ID])
            await button.register_button(reset_button, reset_conf)
            await cg.register_component(reset_button, reset_conf)
            cg.add(reset_button.set_pump(var))

        status_conf = pump_config[CONF_ANOMALY_STATUS_BINARY_SENSOR]
        if status_conf.get(CONF_NAME, "").startswith(
            DEFAULT_ANOMALY_STATUS_BINARY_SENSOR_PREFIX
        ):
            status_conf[CONF_NAME] = f"{pump_config[CONF_NAME]} Anomaly Status"
        status_sensor = cg.new_Pvariable(status_conf[CONF_ID])
        await binary_sensor.register_binary_sensor(status_sensor, status_conf)
        await cg.register_component(status_sensor, status_conf)
        cg.add(var.set_anomaly_status_sensor(status_sensor))

        cg.add(var.set_anomaly_threshold_pct(pump_config[CONF_ANOMALY_THRESHOLD_PCT]))
        cg.add(var.set_learning_samples(pump_config[CONF_LEARNING_SAMPLES]))
        for conf in pump_config.get(CONF_ON_ANOMALY, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

        no_current_conf = pump_config[CONF_NO_CURRENT_BINARY_SENSOR]
        if no_current_conf.get(CONF_NAME, "").startswith(
            DEFAULT_NO_CURRENT_BINARY_SENSOR_PREFIX
        ):
            no_current_conf[CONF_NAME] = f"{pump_config[CONF_NAME]} No Current"
        no_current_sensor = cg.new_Pvariable(no_current_conf[CONF_ID])
        await binary_sensor.register_binary_sensor(no_current_sensor, no_current_conf)
        await cg.register_component(no_current_sensor, no_current_conf)
        cg.add(var.set_no_current_sensor(no_current_sensor))

    if CONF_FLOW_SENSOR in pump_config:
        flow_sens = await cg.get_variable(pump_config[CONF_FLOW_SENSOR])
        cg.add(var.set_flow_sensor(flow_sens))
        cg.add(var.set_flow_timeout_ms(pump_config[CONF_FLOW_TIMEOUT]))

        unexpected_flow_conf = pump_config[CONF_UNEXPECTED_FLOW_BINARY_SENSOR]
        if unexpected_flow_conf.get(CONF_NAME, "").startswith(
            DEFAULT_UNEXPECTED_FLOW_BINARY_SENSOR_PREFIX
        ):
            unexpected_flow_conf[CONF_NAME] = (
                f"{pump_config[CONF_NAME]} Unexpected Flow"
            )
        unexpected_flow_sensor = cg.new_Pvariable(unexpected_flow_conf[CONF_ID])
        await binary_sensor.register_binary_sensor(
            unexpected_flow_sensor, unexpected_flow_conf
        )
        await cg.register_component(unexpected_flow_sensor, unexpected_flow_conf)
        cg.add(var.set_unexpected_flow_sensor(unexpected_flow_sensor))

        if CONF_CURRENT_SENSOR in pump_config:
            flow_loss_conf = pump_config[CONF_FLOW_LOSS_BINARY_SENSOR]
            if flow_loss_conf.get(CONF_NAME, "").startswith(
                DEFAULT_FLOW_LOSS_BINARY_SENSOR_PREFIX
            ):
                flow_loss_conf[CONF_NAME] = f"{pump_config[CONF_NAME]} Flow Loss"
            flow_loss_sensor = cg.new_Pvariable(flow_loss_conf[CONF_ID])
            await binary_sensor.register_binary_sensor(flow_loss_sensor, flow_loss_conf)
            await cg.register_component(flow_loss_sensor, flow_loss_conf)
            cg.add(var.set_flow_loss_sensor(flow_loss_sensor))


async def schedule_select_to_code(pump_var, pump_config, builtin_last_option):
    """Create and register the ScheduleSelect entity for a pump."""
    sel_conf = pump_config[CONF_SCHEDULE_SELECT]
    user_names = [s[CONF_NAME] for s in pump_config[CONF_SCHEDULES]]
    options = ["Off"] + user_names + [builtin_last_option]
    sel = cg.new_Pvariable(sel_conf[CONF_ID])
    await esphome_select.register_select(sel, sel_conf, options=options)
    await cg.register_component(sel, sel_conf)
    cg.add(sel.set_pump_switch(pump_var))
