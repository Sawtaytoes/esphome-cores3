import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_camera, uart
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32_camera", "uart"]
AUTO_LOAD = ["camera_link_protocol"]
ns = cg.esphome_ns.namespace("camera_link_sender")
CameraLinkSender = ns.class_("CameraLinkSender", cg.Component, uart.UARTDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CameraLinkSender),
    cv.Required("camera_id"): cv.use_id(esp32_camera.ESP32Camera),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_camera(await cg.get_variable(config["camera_id"])))
