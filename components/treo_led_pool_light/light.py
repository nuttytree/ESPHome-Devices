import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT, CONF_OUTPUT_ID


treo_light_ns = cg.esphome_ns.namespace("treo_light")
TreoPoolLightOutput = treo_light_ns.class_("TreoPoolLightOutput", light.LightOutput, cg.Component)


CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TreoPoolLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
