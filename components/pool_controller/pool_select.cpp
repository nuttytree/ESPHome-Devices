#include "pool_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pool_controller {

static const char *const SELECT_TAG = "pool.select";

void PoolSelect::setup() {
  std::string value;
  ESP_LOGCONFIG(SELECT_TAG, "Setting up Pool Select");
  size_t index;
  this->pref_ = global_preferences->make_preference<size_t>(this->get_object_id_hash());
  if (!this->pref_.load(&index)) {
    value = this->initial_option_;
    ESP_LOGCONFIG(SELECT_TAG, "State from initial (could not load stored index): %s", value.c_str());
  } else if (!this->has_index(index)) {
    value = this->initial_option_;
    ESP_LOGCONFIG(SELECT_TAG, "State from initial (restored index %d out of bounds): %s", index, value.c_str());
  } else {
    value = this->at(index).value();
    ESP_LOGCONFIG(SELECT_TAG, "State from restore: %s", value.c_str());
  }

  this->publish_state(value);
}

void PoolSelect::control(const std::string &value) {
  ESP_LOGD(SELECT_TAG, "%s changed to option %s", this->get_name().c_str(), value.c_str());
  this->publish_state(value);

    auto index = this->index_of(value);
    this->pref_.save(&index.value());
}

}  // namespace pool_controller
}  // namespace esphome
