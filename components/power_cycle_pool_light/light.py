import esphome.codegen as cg
from esphome.components import light, output
from esphome.components.light.effects import (
    BINARY_EFFECTS,
    register_binary_effect,
    validate_effects,
)
from esphome.components.light.types import LightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLORS,
    CONF_DURATION,
    CONF_EFFECTS,
    CONF_NAME,
    CONF_OUTPUT,
    CONF_OUTPUT_ID,
    CONF_STATE,
)

CONF_COLOR_CHANGE_TIME = "color_change_time"
CONF_MIN_OFF_TIME = "min_off_time"
CONF_RESET_SEQUENCE = "reset_sequence"

power_cycle_pool_light_ns = cg.esphome_ns.namespace("power_cycle_pool_light")
PowerCyclePoolLightOutput = power_cycle_pool_light_ns.class_(
    "PowerCyclePoolLightOutput", light.LightOutput, cg.Component
)
PowerCyclePoolLightEffect = power_cycle_pool_light_ns.class_(
    "PowerCyclePoolLightEffect", LightEffect
)


@register_binary_effect(
    "power_cycle_pool_light_color", PowerCyclePoolLightEffect, "Pool Light Color", {}
)
async def color_effect_to_code(config, effect_id):
    return cg.new_Pvariable(effect_id, config[CONF_NAME])


def _inject_color_effects(config):
    # Expose the configured colors as the light's (fixed) list of effects. Effects
    # are 1-indexed by the light component, which is how the current color is
    # tracked in C++: effect 1 is the first configured color, the one the light
    # shows after a reset.
    effects = [
        {"power_cycle_pool_light_color": {"name": color}}
        for color in config[CONF_COLORS]
    ]
    config = config.copy()
    config[CONF_EFFECTS] = validate_effects(BINARY_EFFECTS)(effects)
    return config


def _validate_unique_colors(value):
    seen = set()
    for color in value:
        if color.lower() in seen:
            raise cv.Invalid(
                f"Duplicate color name '{color}', color names must be unique"
            )
        seen.add(color.lower())
    return value


RESET_STEP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATE): cv.boolean,
        cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PowerCyclePoolLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_COLORS): cv.All(
            cv.ensure_list(cv.string_strict),
            cv.Length(min=2, max=255),
            _validate_unique_colors,
        ),
        cv.Required(CONF_RESET_SEQUENCE): cv.All(
            cv.ensure_list(RESET_STEP_SCHEMA), cv.Length(min=1)
        ),
        cv.Required(CONF_MIN_OFF_TIME): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_COLOR_CHANGE_TIME, default="200ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_EFFECTS): cv.invalid(
            "The effects list is generated from 'colors' and cannot be customized. "
            "Remove the 'effects' key from your configuration."
        ),
    }
).add_extra(_inject_color_effects)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
    cg.add(var.set_color_count(len(config[CONF_COLORS])))
    cg.add(var.set_min_off_time(config[CONF_MIN_OFF_TIME]))
    cg.add(var.set_color_change_time(config[CONF_COLOR_CHANGE_TIME]))
    cg.add(var.reserve_reset_steps(len(config[CONF_RESET_SEQUENCE])))
    for step in config[CONF_RESET_SEQUENCE]:
        cg.add(var.add_reset_step(step[CONF_STATE], step[CONF_DURATION]))
