#include "schedule_select.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_controller.select";

void ScheduleSelect::setup() {
  this->pref_ = this->make_entity_preference<size_t>();

  size_t index = 0;
  if (this->pref_.load(&index) && this->has_index(index)) {
    ESP_LOGD(TAG, "Restored schedule index %zu from flash", index);
  } else {
    index = 0;
    ESP_LOGD(TAG, "No valid stored schedule; defaulting to index 0 (Off)");
  }

  if (this->pump_ != nullptr)
    this->pump_->set_active_schedule_index(index);
  this->publish_state(index);
}

void ScheduleSelect::dump_config() { LOG_SELECT("", "Pool Controller Schedule Select", this); }

void ScheduleSelect::control(size_t index) {
  this->pref_.save(&index);
  if (this->pump_ != nullptr)
    this->pump_->set_active_schedule_index(index);
  this->publish_state(index);
}

}  // namespace pool_controller
}  // namespace esphome
