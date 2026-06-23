import esphome.codegen as cg
from esphome.components import light, output
from esphome.components.light.effects import BINARY_EFFECTS, register_binary_effect, validate_effects
from esphome.components.light.types import LightEffect
import esphome.config_validation as cv
from esphome.const import CONF_EFFECTS, CONF_NAME, CONF_OUTPUT, CONF_OUTPUT_ID


treo_light_ns = cg.esphome_ns.namespace("treo_light")
TreoPoolLightOutput = treo_light_ns.class_("TreoPoolLightOutput", light.LightOutput, cg.Component)
TreoPoolLightEffect = treo_light_ns.class_("TreoPoolLightEffect", LightEffect)

_TREO_EFFECT_NAMES = [
    "Slow Change",
    "White",
    "Blue",
    "Green",
    "Red",
    "Amber",
    "Magenta",
    "Fast Change",
]


@register_binary_effect("treo_color", TreoPoolLightEffect, "Treo Color", {})
async def treo_color_effect_to_code(config, effect_id):
    return cg.new_Pvariable(effect_id, config[CONF_NAME])


def _inject_treo_effects(config):
    default = [{"treo_color": {"name": name}} for name in _TREO_EFFECT_NAMES]
    config = config.copy()
    config[CONF_EFFECTS] = validate_effects(BINARY_EFFECTS)(default)
    return config


CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TreoPoolLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_EFFECTS): cv.invalid(
            "Treo LED pool light effects are fixed and cannot be customized. "
            "Remove the 'effects' key from your configuration."
        ),
    }
).add_extra(_inject_treo_effects)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
