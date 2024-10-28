#include "weact_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <cinttypes>

namespace esphome {
namespace weact_epaper {

static const char *const TAG = "weact_epaper";

static const uint8_t LUT_SIZE_WEACT = 30;

static const uint8_t FULL_UPDATE_LUT[LUT_SIZE_WEACT] = {0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
                                                            0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
                                                            0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT[LUT_SIZE_WEACT] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


void WeactEPaperBase::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
  this->spi_setup();

  this->reset_();
}
float WeactEPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }
void WeactEPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}
void WeactEPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by one or more bytes of data.
// The command is the first byte, length is the total including cmd.
void WeactEPaperBase::cmd_data(const uint8_t *c_data, size_t length) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(c_data[0]);
  this->dc_pin_->digital_write(true);
  this->write_array(c_data + 1, length - 1);
  this->disable();
}

bool WeactEPaperBase::wait_until_idle_() {
  if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(1);
  }
  return true;
}
void WeactEPaperBase::update() {
  this->do_update_();
  this->display();
}
void WeactEPaper::fill(Color color) {
  // flip logic
  const uint8_t fill = color.is_on() ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void HOT WeactEPaper::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_controller()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (!color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }
}

uint32_t WeactEPaper::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 8u;
}  // just a black buffer

void WeactEPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void WeactEPaperBase::end_command_() { this->disable(); }
void WeactEPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void WeactEPaperBase::end_data_() { this->disable(); }
void WeactEPaperBase::on_safe_shutdown() { this->deep_sleep(); }

// ========================================================
//                          Type A
// ========================================================

void WeactEPaperTypeA::initialize() {
  // Achieve display intialization
  this->init_display_();
  // If a reset pin is configured, eligible displays can be set to deep sleep
  // between updates, as recommended by the hardware provider
  if (this->reset_pin_ != nullptr) {
    switch (this->model_) {
      // More models can be added here to enable deep sleep if eligible
      case WEACT_EPAPER_1_54_IN:
      case WEACT_EPAPER_1_54_IN_V2:
      case WEACT_EPAPER_4_2_IN:
        this->deep_sleep_between_updates_ = true;
        ESP_LOGI(TAG, "Set the display to deep sleep");
        this->deep_sleep();
        break;
      default:
        break;
    }
  }
}
void WeactEPaperTypeA::init_display_() {
  if (this->model_ == WEACT_EPAPER_2_13_IN_V2) {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(10);
      this->reset_pin_->digital_write(true);
      delay(10);
      this->wait_until_idle_();
    }

    this->command(0x12);  // SWRESET
    this->wait_until_idle_();
  }

  // COMMAND DRIVER OUTPUT CONTROL
  this->command(0x01);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0

  // COMMAND BOOSTER SOFT START CONTROL
  this->command(0x0C);
  this->data(0xD7);
  this->data(0xD6);
  this->data(0x9D);

  // COMMAND WRITE VCOM REGISTER
  this->command(0x2C);
  this->data(0xA8);

  // COMMAND SET DUMMY LINE PERIOD
  this->command(0x3A);
  this->data(0x1A);

  // COMMAND SET GATE TIME
  this->command(0x3B);
  this->data(0x08);  // 2µs per row

  // COMMAND DATA ENTRY MODE SETTING
  this->command(0x11);
  switch (this->model_) {
    case WEACT_EPAPER_2_9_IN_V2:
      this->data(0x03);  // from top left to bottom right
      // RAM content option for Display Update
      this->command(0x21);
      this->data(0x00);
      this->data(0x80);
      break;
    default:
      this->data(0x03);  // from top left to bottom right
  }
}
void WeactEPaperTypeA::dump_config() {
  LOG_DISPLAY("", "Weact E-Paper", this);
  switch (this->model_) {
    case WEACT_EPAPER_1_54_IN:
      ESP_LOGCONFIG(TAG, "  Model: 1.54in");
      break;
    case WEACT_EPAPER_1_54_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 1.54inV2");
      break;
    case WEACT_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in");
      break;
    case WEACT_EPAPER_2_13_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.13inV2");
      break;
    case WEACT_EPAPER_2_9_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.9in");
      break;
    case WEACT_EPAPER_2_9_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.9inV2");
      break;
    case WEACT_EPAPER_4_2_IN:
      ESP_LOGCONFIG(TAG, "  Model: 4.2in");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void HOT WeactEPaperTypeA::display() {
  bool full_update = this->at_update_ == 0;
  bool prev_full_update = this->at_update_ == 1;

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Wake up the display");
    this->reset_();
    this->wait_until_idle_();
    this->init_display_();
  }

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  if (this->full_update_every_ >= 1) {
    if (full_update != prev_full_update) {
      this->write_lut_(full_update ? FULL_UPDATE_LUT : PARTIAL_UPDATE_LUT, LUT_SIZE_WEACT);
    }
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  }

  if (this->model_ == WEACT_EPAPER_2_13_IN_V2) {
    // Set VCOM for full or partial update
    this->command(0x2C);
    this->data(full_update ? 0x55 : 0x26);

    if (!full_update) {
      // Enable "ping-pong"
      this->command(0x37);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x40);
      this->data(0x00);
      this->data(0x00);
      this->command(0x22);
      this->data(0xc0);
      this->command(0x20);
    }
  }

  // Border waveform
  switch (this->model_) {
    case WEACT_EPAPER_2_13_IN_V2:
      this->command(0x3C);
      this->data(full_update ? 0x03 : 0x01);
      break;
    default:
      break;
  }

  // COMMAND SET RAM X ADDRESS START END POSITION
  this->command(0x44);
  this->data(0x00);
  this->data((this->get_width_internal() - 1) >> 3);
  // COMMAND SET RAM Y ADDRESS START END POSITION
  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);

  // COMMAND SET RAM X ADDRESS COUNTER
  this->command(0x4E);
  this->data(0x00);
  // COMMAND SET RAM Y ADDRESS COUNTER
  this->command(0x4F);
  this->data(0x00);
  this->data(0x00);

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  // COMMAND WRITE RAM
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  if (this->model_ == WEACT_EPAPER_2_13_IN_V2 && full_update) {
    // Write base image again on full refresh
    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  // COMMAND DISPLAY UPDATE CONTROL 2
  this->command(0x22);
  switch (this->model_) {
    case WEACT_EPAPER_2_9_IN_V2:
    case WEACT_EPAPER_1_54_IN_V2:
    case WEACT_EPAPER_4_2_IN:
      this->data(full_update ? 0xF7 : 0xFF);
      break;
    case WEACT_EPAPER_2_13_IN_V2:
      this->data(full_update ? 0xC7 : 0x0C);
      break;
    default:
      this->data(0xC4);
      break;
  }

  // COMMAND MASTER ACTIVATION
  this->command(0x20);
  // COMMAND TERMINATE FRAME READ WRITE
  this->command(0xFF);

  this->status_clear_warning();

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Set the display back to deep sleep");
    this->deep_sleep();
  }
}
int WeactEPaperTypeA::get_width_internal() {
  switch (this->model_) {
    case WEACT_EPAPER_1_54_IN:
    case WEACT_EPAPER_1_54_IN_V2:
      return 200;
    case WEACT_EPAPER_2_13_IN:
    case WEACT_EPAPER_2_13_IN_V2:
      return 122;
    case WEACT_EPAPER_2_9_IN:
    case WEACT_EPAPER_2_9_IN_V2:
      return 128;
    case WEACT_EPAPER_4_2_IN:
      return 400;
  }
  return 0;
}
// The controller of the 2.13" displays has a buffer larger than screen size
int WeactEPaperTypeA::get_width_controller() {
  switch (this->model_) {
    case WEACT_EPAPER_2_13_IN:
    case WEACT_EPAPER_2_13_IN_V2:
      return 128;
    default:
      return this->get_width_internal();
  }
}
int WeactEPaperTypeA::get_height_internal() {
  switch (this->model_) {
    case WEACT_EPAPER_1_54_IN:
    case WEACT_EPAPER_1_54_IN_V2:
      return 200;
    case WEACT_EPAPER_2_13_IN:
    case WEACT_EPAPER_2_13_IN_V2:
      return 250;
    case WEACT_EPAPER_2_9_IN:
    case WEACT_EPAPER_2_9_IN_V2:
      return 296;
    case WEACT_EPAPER_4_2_IN:
      return 300;
  }
  return 0;
}
void WeactEPaperTypeA::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->command(0x32);
  for (uint8_t i = 0; i < size; i++)
    this->data(lut[i]);
}
WeactEPaperTypeA::WeactEPaperTypeA(WeactEPaperTypeAModel model) : model_(model) {}
void WeactEPaperTypeA::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

uint32_t WeactEPaperTypeA::idle_timeout_() {
  switch (this->model_) {
    case WEACT_EPAPER_1_54_IN:
    case WEACT_EPAPER_1_54_IN_V2:
    case WEACT_EPAPER_2_13_IN_V2:
    case WEACT_EPAPER_4_2_IN:
      return 2500;
    default:
      return WeactEPaperBase::idle_timeout_();
  }
}

}  // namespace weact_epaper
}  // namespace esphome
