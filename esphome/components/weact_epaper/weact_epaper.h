#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace weact_epaper {

class WeactEPaperBase : public display::DisplayBuffer,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                  spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }

  void command(uint8_t value);
  void data(uint8_t value);
  void cmd_data(const uint8_t *data, size_t length);

  virtual void display() = 0;
  virtual void initialize() = 0;
  virtual void deep_sleep() = 0;

  void update() override;

  void setup() override {
    this->setup_pins_();
    this->initialize();
  }

  void on_safe_shutdown() override;

 protected:
  bool wait_until_idle_();

  void setup_pins_();

  void reset_() {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(reset_duration_);  // NOLINT
      this->reset_pin_->digital_write(true);
      delay(20);
    }
  }

  virtual int get_width_controller() { return this->get_width_internal(); };

  virtual uint32_t get_buffer_length_() = 0;  // NOLINT(readability-identifier-naming)
  uint32_t reset_duration_{200};

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_{nullptr};
  virtual uint32_t idle_timeout_() { return 1000u; }  // NOLINT(readability-identifier-naming)
};

class WeactEPaper : public WeactEPaperBase {
 public:
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length_() override;
};

enum WeactEPaperTypeAModel {
  WEACT_EPAPER_1_54_IN = 0,
  WEACT_EPAPER_1_54_IN_V2,
  WEACT_EPAPER_2_13_IN,
  WEACT_EPAPER_2_13_IN_V2,
  WEACT_EPAPER_2_9_IN,
  WEACT_EPAPER_2_9_IN_V2,
  WEACT_EPAPER_4_2_IN,
};

class WeactEPaperTypeA : public WeactEPaper {
 public:
  WeactEPaperTypeA(WeactEPaperTypeAModel model);

  void initialize() override;

  void dump_config() override;

  void display() override;

  void deep_sleep() override {
    switch (this->model_) {
      // Models with specific deep sleep command and data
      case WEACT_EPAPER_1_54_IN:
      case WEACT_EPAPER_1_54_IN_V2:
      case WEACT_EPAPER_2_9_IN_V2:
      case WEACT_EPAPER_2_13_IN_V2:
        // COMMAND DEEP SLEEP MODE
        this->command(0x10);
        this->data(0x01);
        break;
      // Other models default to simple deep sleep command
      default:
        // COMMAND DEEP SLEEP
        this->command(0x10);
        break;
    }
    if (this->model_ != WEACT_EPAPER_2_13_IN_V2) {
      // From panel specification:
      // "After this command initiated, the chip will enter Deep Sleep Mode, BUSY pad will keep output high."
      this->wait_until_idle_();
    }
  }

  void set_full_update_every(uint32_t full_update_every);

 protected:
  void write_lut_(const uint8_t *lut, uint8_t size);

  void init_display_();

  int get_width_internal() override;

  int get_height_internal() override;

  int get_width_controller() override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  WeactEPaperTypeAModel model_;
  uint32_t idle_timeout_() override;

  bool deep_sleep_between_updates_{false};
};

}  // namespace weact_epaper
}  // namespace esphome
