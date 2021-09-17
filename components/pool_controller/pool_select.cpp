#include "esphome/core/log.h"
#include "pool_select.h"

namespace esphome {
namespace pool_controller {

static const char *const TAG = "pool_select";

void PoolSelect::setup() {
  std::string value;
  size_t index;
  this->pref_ = global_preferences.make_preference<size_t>(this->get_object_id_hash());
  if (!this->pref_.load(&index)) {
    value = "Off";
    ESP_LOGD(TAG, "State from initial (could not load): %s", value.c_str());
  } else {
    value = this->traits.get_options().at(index);
    ESP_LOGD(TAG, "State from restore: %s", value.c_str());
  }
  this->publish_state(value);
}

void PoolSelect::control(const std::string &value) {
  this->publish_state(value);

  auto options = this->traits.get_options();
  size_t index = std::find(options.begin(), options.end(), value) - options.begin();
  this->pref_.save(&index);
}

}  // namespace pool_controller
}  // namespace esphome
