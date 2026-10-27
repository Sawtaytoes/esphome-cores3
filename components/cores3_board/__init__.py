from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INPUT, CONF_INVERTED, CONF_NUMBER, CONF_OUTPUT

DEPENDENCIES = ["i2c"]
ns = cg.esphome_ns.namespace("cores3_board")
CoreS3Board = ns.class_("CoreS3Board", cg.PollingComponent, i2c.I2CDevice)
ResetPin = ns.class_("ResetPin", cg.GPIOPin)
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(CoreS3Board)}).extend(
    cv.polling_component_schema("30s")
).extend(i2c.i2c_device_schema(0x34))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(config):
    if config[CONF_INPUT] or not config[CONF_OUTPUT]:
        raise cv.Invalid("CoreS3 reset pins support output mode only")
    return config


PIN_SCHEMA = pins.gpio_base_schema(
    ResetPin, cv.one_of(0, 9, int=True), modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=validate_mode,
).extend({cv.Required("cores3_board"): cv.use_id(CoreS3Board)})


@pins.PIN_SCHEMA_REGISTRY.register("cores3_board", PIN_SCHEMA)
async def reset_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config["cores3_board"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
