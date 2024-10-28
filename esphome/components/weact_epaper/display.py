import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

weact_epaper_ns = cg.esphome_ns.namespace("weact_epaper")
WeactEPaperBase = weact_epaper_ns.class_(
    "WeactEPaperBase", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
WeactEPaper = weact_epaper_ns.class_("WeactEPaper", WeactEPaperBase)
WeactEPaperTypeA = weact_epaper_ns.class_(
    "WeactEPaperTypeA", WeactEPaper
)
WeactEPaperTypeAModel = weact_epaper_ns.enum("WeactEPaperTypeAModel")

MODELS = {
    "1.54in": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_1_54_IN),
    "1.54inv2": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_1_54_IN_V2),
    "2.13in": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_2_13_IN),
    "2.13inv2": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_2_13_IN_V2),
    "2.90in": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_2_9_IN),
    "2.90inv2": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_2_9_IN_V2),
    "4.20in": ("a", WeactEPaperTypeAModel.WEACT_EPAPER_4_2_IN),
}

RESET_PIN_REQUIRED_MODELS = ("2.13inv2")


def validate_full_update_every_only_types_ac(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    if MODELS[value[CONF_MODEL]][0] == "b":
        full_models = []
        for key, val in sorted(MODELS.items()):
            if val[0] != "b":
                full_models.append(key)
        raise cv.Invalid(
            "The 'full_update_every' option is only available for models "
            + ", ".join(full_models)
        )
    return value


def validate_reset_pin_required(config):
    if config[CONF_MODEL] in RESET_PIN_REQUIRED_MODELS and CONF_RESET_PIN not in config:
        raise cv.Invalid(
            f"'{CONF_RESET_PIN}' is required for model {config[CONF_MODEL]}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeactEPaperBase),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FULL_UPDATE_EVERY): cv.int_range(min=1, max=4294967295),
            cv.Optional(CONF_RESET_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=500)),
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema()),
    validate_full_update_every_only_types_ac,
    validate_reset_pin_required,
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    model_type, model = MODELS[config[CONF_MODEL]]
    if model_type == "a":
        rhs = WeactEPaperTypeA.new(model)
        var = cg.Pvariable(config[CONF_ID], rhs, WeactEPaperTypeA)
    else:
        raise NotImplementedError()

    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
