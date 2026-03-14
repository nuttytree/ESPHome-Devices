import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import output
from esphome.components import sensor
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_OUTPUT, DEVICE_CLASS_OCCUPANCY, ENTITY_CATEGORY_DIAGNOSTIC

CODEOWNERS = ["@nuttytree"]

adc_ns = cg.esphome_ns.namespace("adc")
ADCSensor = adc_ns.class_("ADCSensor")

bed_sensor_ns = cg.esphome_ns.namespace("bed_sensor")
BedSensor = bed_sensor_ns.class_("BedSensor", cg.PollingComponent)

CONF_ADC_SENSOR = "adc_sensor"
CONF_SIDE_ONE = "side_one"
CONF_SIDE_TWO = "side_two"
CONF_STATUS_NAME = "status_name"
CONF_VALUE_SENSOR = "value_sensor"
CONF_SOMEONE = "someone_sensor"
CONF_COUNT = "count_sensor"
CONF_STATUS = "status_sensor"

ICON_BED = "mdi:bed"
ICON_NUMERIC = "mdi:numeric"
ICON_COUNTER = "mdi:counter"

BED_SIDE_CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_OCCUPANCY,
    icon=ICON_BED,
).extend(
    {
        cv.Required(CONF_STATUS_NAME): cv.string,
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_VALUE_SENSOR): sensor.sensor_schema(
            icon=ICON_NUMERIC,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

SOMEONE_CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_OCCUPANCY,
    icon=ICON_BED,
).extend(
    {
        cv.Required(CONF_STATUS_NAME): cv.string,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BedSensor),
            cv.Required(CONF_ADC_SENSOR): cv.use_id(ADCSensor),
            cv.Required(CONF_SIDE_ONE): BED_SIDE_CONFIG_SCHEMA,
            cv.Required(CONF_SIDE_TWO): BED_SIDE_CONFIG_SCHEMA,
            cv.Required(CONF_SOMEONE): SOMEONE_CONFIG_SCHEMA,
            cv.Required(CONF_COUNT): sensor.sensor_schema(
                icon=ICON_COUNTER,
                accuracy_decimals=0,
            ),
            cv.Required(CONF_STATUS): text_sensor.text_sensor_schema(
                icon=ICON_BED,
            ),
        }
    )
    .extend(cv.polling_component_schema("2s"))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    adc_sensor = await cg.get_variable(config[CONF_ADC_SENSOR])
    cg.add(var.set_adc_sensor(adc_sensor))
    
    side_one_config = config[CONF_SIDE_ONE]
    cg.add(var.set_side_one_name(side_one_config[CONF_STATUS_NAME]))
    side_one_output = await cg.get_variable(side_one_config[CONF_OUTPUT])
    cg.add(var.set_side_one_output(side_one_output))
    side_one_value_sensor_config = side_one_config[CONF_VALUE_SENSOR]
    side_one_value_sensor = await sensor.new_sensor(side_one_value_sensor_config)
    cg.add(var.set_side_one_value_sensor(side_one_value_sensor))
    side_one_sensor = await binary_sensor.new_binary_sensor(side_one_config)
    cg.add(var.set_side_one_sensor(side_one_sensor))

    side_two_config = config[CONF_SIDE_TWO]
    cg.add(var.set_side_two_name(side_two_config[CONF_STATUS_NAME]))
    side_two_output = await cg.get_variable(side_two_config[CONF_OUTPUT])
    cg.add(var.set_side_two_output(side_two_output))
    side_two_value_sensor_config = side_two_config[CONF_VALUE_SENSOR]
    side_two_value_sensor = await sensor.new_sensor(side_two_value_sensor_config)
    cg.add(var.set_side_two_value_sensor(side_two_value_sensor))
    side_two_sensor = await binary_sensor.new_binary_sensor(side_two_config)
    cg.add(var.set_side_two_sensor(side_two_sensor))

    someone_config = config[CONF_SOMEONE]
    cg.add(var.set_someone_name(someone_config[CONF_STATUS_NAME]))
    someone_sensor = await binary_sensor.new_binary_sensor(someone_config)
    cg.add(var.set_someone_sensor(someone_sensor))

    count_config = config[CONF_COUNT]
    count_sensor = await sensor.new_sensor(count_config)
    cg.add(var.set_count_sensor(count_sensor))

    status_config = config[CONF_STATUS]
    status_sensor = await text_sensor.new_text_sensor(status_config)
    cg.add(var.set_status_sensor(status_sensor))
