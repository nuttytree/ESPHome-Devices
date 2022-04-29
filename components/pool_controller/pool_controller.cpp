#include "pool_controller.h"
#include "esphome/components/time/automation.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller";

static const uint32_t RUNTIME_30_MINUTES_PER_HALF_HOUR = UINT32_MAX;
static const uint32_t RUNTIME_15_MINUTES_PER_HALF_HOUR = 900000;
static const uint32_t RUNTIME_10_MINUTES_PER_HALF_HOUR = 600000;

// Minimum time the pump needs to be off before the automation will turn it back on (300000 = 5 minutes)
static const uint32_t MINIMUM_AUTOMATION_PUMP_OFF_TIME = 300000;

// Minimum time the pump needs to be on before the automation will turn it back off (300000 = 5 minutes)
static const uint32_t MINIMUM_AUTOMATION_PUMP_ON_TIME = 300000;

PoolController::PoolController() {
  ESP_LOGD(TAG, "Creating pump mode select component");
  this->pump_select_ = new PoolSelect();
  App.register_component(this->pump_select_);
  App.register_select(this->pump_select_);
  this->pump_select_->set_name("Pool Pump Mode");
  this->pump_select_->set_icon("mdi:pump");
  this->pump_select_->traits.set_options({"Off", "Normal", "Always Except Peak", "Always"});
  this->pump_select_->add_on_state_callback([this](std::string value) -> void {
    auto options = this->pump_select_->traits.get_options();
    size_t index = std::find(options.begin(), options.end(), value) - options.begin();
    this->pump_mode_ = static_cast<PumpMode>(index);
  });

  ESP_LOGD(TAG, "Creating cleaner mode select component");
  this->cleaner_select_ = new PoolSelect();
  App.register_component(this->cleaner_select_);
  App.register_select(this->cleaner_select_);
  this->cleaner_select_->set_name("Pool Cleaner Mode");
  this->cleaner_select_->set_icon("mdi:robot-vacuum");
  this->cleaner_select_->traits.set_options({"Off", "Normal", "When Pump Is On"});
  this->cleaner_select_->add_on_state_callback([this](std::string value) -> void {
    auto options = this->cleaner_select_->traits.get_options();
    size_t index = std::find(options.begin(), options.end(), value) - options.begin();
    this->cleaner_mode_ = static_cast<CleanerMode>(index);
  });

  // this->heat_select_ = new PoolSelect();
  // App.register_component(this->heat_select_);
  // App.register_select(this->heat_select_);
  // this->heat_select_->set_name("Pool Heat Mode");
  // this->heat_select_->traits.set_icon("mdi:thermometer");
  // this->heat_select_->traits.set_options({"Off", "Normal", "Always"});
}

void PoolController::set_time(time::RealTimeClock *time) { 
  this->time_ = time;
  
  ESP_LOGD(TAG, "Adding cron trigger");
  auto cron_trigger = new time::CronTrigger(time);
  cron_trigger->add_second(0);
  cron_trigger->add_minutes({0, 30});
  cron_trigger->add_hours({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23});
  cron_trigger->add_days_of_month({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31});
  cron_trigger->add_months({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  cron_trigger->add_days_of_week({1, 2, 3, 4, 5, 6, 7});
  App.register_component(cron_trigger);
  auto lambda_action = new LambdaAction<>([=]() -> void {
    this->pump_switch_->reset_runtime();
    this->cleaner_switch_->reset_runtime();
  });
  auto automation = new Automation<>(cron_trigger);
  automation->add_actions({lambda_action});
}

void PoolController::set_pump_switch(switch_::Switch *pump_switch) {
  this->pump_switch_ = new PumpSwitch(pump_switch);
  App.register_component(this->pump_switch_);
  App.register_switch(this->pump_switch_);
  this->pump_switch_->add_turn_off_check([this]() -> bool {
    ESP_LOGD(TAG, "Pump switch turn off check is checking the state of the pool cleaner");
    if (this->cleaner_switch_->state) {
      this->cleaner_switch_->turn_off();
    }
    return this->cleaner_switch_->get_current_off_time() > 5000;
  });
}

void PoolController::set_cleaner_switch(switch_::Switch *cleaner_switch) {
  this->cleaner_switch_ = new PumpSwitch(cleaner_switch);
  App.register_component(this->cleaner_switch_);
  App.register_switch(this->cleaner_switch_);
  this->cleaner_switch_->add_turn_on_check([this]() {
    ESP_LOGD(TAG, "Cleaner switch turn on check is checking the state of the pool pump");
    if (!this->pump_switch_->state) {
      this->pump_switch_->turn_on();
    }
    return this->pump_switch_->get_current_on_time() > 5000;
  });
}

void PoolController::setup() {
}

void PoolController::loop() {
  manage_pump_();

  manage_cleaner_();
}

void PoolController::manage_pump_() {
  uint32_t desired_runtime = 0;
  time::ESPTime now = this->time_->now();
  uint8_t hour = now.hour;
  uint8_t day_of_week = now.day_of_week;
  switch (this->pump_mode_) {
    case PumpMode::PUMP_MODE_OFF:
      if (this->pump_switch_->state) {
        this->pump_switch_->turn_off();
      }
      break;
    
    case PumpMode::PUMP_MODE_NORMAL:
      if (hour >= 4 && hour < 6) {
        desired_runtime = RUNTIME_30_MINUTES_PER_HALF_HOUR; // normal cleaner run time
      } else if (day_of_week > 1 && day_of_week < 7 && hour >= 15 && hour < 20 ) {
        desired_runtime = RUNTIME_10_MINUTES_PER_HALF_HOUR; // peak electric rate
      } else if (hour >= 6 && hour < 20) {
        desired_runtime = RUNTIME_15_MINUTES_PER_HALF_HOUR;
      }
      break;
    
    case PumpMode::PUMP_MODE_ALWAYS_EXCEPT_PEAK:
      if (day_of_week == 1 || day_of_week == 7 || hour < 15 || hour >= 20) {
        desired_runtime = RUNTIME_30_MINUTES_PER_HALF_HOUR;
      }
      break;
    
    case PumpMode::PUMP_MODE_ALWAYS:
      desired_runtime = RUNTIME_30_MINUTES_PER_HALF_HOUR;
      break;
  }

  if (!this->pump_switch_->state && desired_runtime > this->pump_switch_->get_runtime() && this->pump_switch_->get_current_off_time() > MINIMUM_AUTOMATION_PUMP_OFF_TIME) {
    this->pump_switch_->turn_on();
  } else if (this->pump_switch_->state && desired_runtime < this->pump_switch_->get_runtime() && this->pump_switch_->get_current_on_time() > MINIMUM_AUTOMATION_PUMP_ON_TIME) {
    this->pump_switch_->turn_off();
  }
}

void PoolController::manage_cleaner_() {
  bool desired_state = false;
  uint8_t hour = this->time_->now().hour;
  switch (this->cleaner_mode_) {
    case CleanerMode::CLEANER_MODE_OFF:
      if (this->cleaner_switch_->state) {
        this->cleaner_switch_->turn_off();
      }
      break;
    
    case CleanerMode::CLEANER_MODE_NORMAL:
      if (hour >= 4 && hour < 6) {
        desired_state = true;
      }
      break;
    
    case CleanerMode::CLEANER_MODE_WHEN_PUMP_IS_ON:
      if (this->pump_switch_->state) {
        desired_state = true;
      }
      break;
  }

  if (!this->cleaner_switch_->state && desired_state && this->cleaner_switch_->get_current_off_time() > MINIMUM_AUTOMATION_PUMP_OFF_TIME) {
    this->cleaner_switch_->turn_on();
  } else if (this->cleaner_switch_->state && !desired_state && this->cleaner_switch_->get_current_on_time() > MINIMUM_AUTOMATION_PUMP_ON_TIME) {
    this->cleaner_switch_->turn_off();
  }
}

}  // namespace pool_controller
}  // namespace esphome
