import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import runtime_image, text_sensor, uart
from esphome.components.lvgl.types import lv_obj_t
from esphome.components.lvgl.defines import add_lv_use
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

DEPENDENCIES = ["uart", "lvgl"]
AUTO_LOAD = ["camera_link_protocol", "runtime_image", "text_sensor"]
ns = cg.esphome_ns.namespace("camera_link_viewer")
CameraLinkViewer = ns.class_("CameraLinkViewer", cg.Component, uart.UARTDevice, runtime_image.RuntimeImage)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CameraLinkViewer),
    cv.Required("parent_id"): cv.use_id(lv_obj_t),
    cv.Required("status_id"): cv.use_id(text_sensor.TextSensor),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


@coroutine_with_priority(10.0)
async def to_code(config):
    add_lv_use("IMAGE")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_parent_widget(await cg.get_variable(config["parent_id"])))
    cg.add(var.set_status(await cg.get_variable(config["status_id"])))
    runtime_image.enable_format("JPEG")
