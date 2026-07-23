#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/switch/switch.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace pool_controller {

/// A single time window within a schedule.
struct ScheduleRuntime {
  uint16_t start_minute;     ///< Minutes since midnight (0-1410)
  uint16_t end_minute;       ///< Minutes since midnight (30-1440, 1440 = 24:00)
  uint8_t minutes_per_hour;  ///< Minutes to run per hour within this window
  uint8_t days_of_week;      ///< Bitmask: bit0=Sun, bit1=Mon, ..., bit6=Sat; 0x7F = every day
};

/// A named collection of runtime windows.
struct Schedule {
  StringRef name;
  std::vector<ScheduleRuntime> runtimes;
};

/// Persisted baseline data for current-draw anomaly detection.
struct AnomalyBaseline {
  float steady_state{0.0f};  ///< EMA baseline of steady-state run current (amps).
  float drift_ema{0.0f};     ///< Slow EMA for long-term bearing-wear detection.
  float startup_peak{0.0f};  ///< Average inrush peak measured during the startup window.
  uint32_t sample_count{0};  ///< Steady-state samples used to build steady_state.
  uint8_t startup_runs{0};   ///< Pump-start cycles contributing to startup_peak.
};

class AuxiliaryPumpSwitch;
class PoolController;
class PoolHeater;
class PumpAnomalySwitch;

/// Base class for all pump switch types. Holds shared output and schedule state.
class PumpSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_output(output::BinaryOutput *output) { output_ = output; }

  void add_schedule(const char *name) { schedules_.push_back({StringRef(name), {}}); }
  void add_runtime_to_last_schedule(uint16_t start_minute, uint16_t end_minute, uint8_t minutes_per_hour,
                                    uint8_t days_of_week) {
    schedules_.back().runtimes.push_back({start_minute, end_minute, minutes_per_hour, days_of_week});
  }

  /// Returns total pump runtime in seconds since the last half-hour reset.
  uint32_t get_runtime_seconds() const {
    if (this->runtime_start_ms_ != 0) {
      return this->runtime_seconds_ + (millis_64() - this->runtime_start_ms_) / 1000;
    }
    return this->runtime_seconds_;
  }

  /// Returns the index of the currently active schedule.
  /// Index convention:
  ///   0       = Off (built-in, always first)
  ///   1..N    = user-defined schedules (schedules_ vector, 1-based)
  ///   N+1     = built-in last schedule ("Always" for primary, "When X is Running" for auxiliary)
  size_t get_active_schedule_index() const { return this->active_schedule_idx_; }

  /// Sets the active schedule by index. Called by ScheduleSelect.
  void set_active_schedule_index(size_t index) { this->active_schedule_idx_ = index; }

  /// Sets the sequencing delay (ms) between primary and auxiliary pump state changes.
  void set_sequence_delay(uint32_t delay_ms) { this->sequence_delay_ms_ = delay_ms; }

  /// Sets an optional binary sensor whose active (on) state prevents pump turn-ons.
  void set_disable_pumps_sensor(binary_sensor::BinarySensor *sensor) { this->disable_pumps_sensor_ = sensor; }

  // ── Anomaly detection ──────────────────────────────────────────────────────
  void set_enable_anomaly_detection(bool enable) { this->enable_anomaly_detection_ = enable; }
#ifdef USE_SENSOR
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
#endif
  /// Threshold (as a percentage of baseline) beyond which an anomaly is fired. Default: 20 %.
  void set_anomaly_threshold_pct(float pct) { this->anomaly_threshold_pct_ = pct; }
  /// Number of steady-state samples to collect before locking the baseline. Default: 200.
  void set_learning_samples(uint32_t n) { this->learning_samples_ = n; }
  /// Called by PumpAnomalyTrigger to register the automation callback.
  void set_anomaly_trigger(Trigger<std::string> *trigger) { this->anomaly_trigger_ = trigger; }
  void set_anomaly_status_sensor(binary_sensor::BinarySensor *sensor) { this->anomaly_status_sensor_ = sensor; }
  /// Reset the stored anomaly baseline and force new sample learning.
  void reset_anomaly_baseline();

  // ── Current-based state ────────────────────────────────────────────────────
  /// Current threshold (amps) above which the pump is considered running. Default: 0.1 A.
  void set_current_on_threshold(float threshold) { this->current_on_threshold_ = threshold; }
  /// Problem sensor: true while the pump is commanded on but no current is detected.
  void set_no_current_sensor(binary_sensor::BinarySensor *sensor) { this->no_current_sensor_ = sensor; }

  // ── Flow sensor ────────────────────────────────────────────────────────────
  /// Optional binary sensor that detects water flow.
  void set_flow_sensor(binary_sensor::BinarySensor *sensor) { this->flow_sensor_ = sensor; }
  /// How long (ms) flow must be absent before the pump is shut down. Default: 2000 ms.
  void set_flow_timeout_ms(uint32_t ms) { this->flow_timeout_ms_ = ms; }
  /// Problem sensor: latches true when the pump is shut down due to lost flow; only clears once
  /// the pump is on, current confirms the motor is running, and flow is detected again.
  void set_flow_loss_sensor(binary_sensor::BinarySensor *sensor) { this->flow_loss_sensor_ = sensor; }
  /// Problem sensor: true when flow is detected while the pump is commanded off, debounced by
  /// flow_timeout to allow for residual flow immediately after shutdown.
  void set_unexpected_flow_sensor(binary_sensor::BinarySensor *sensor) { this->unexpected_flow_sensor_ = sensor; }

  /// Returns true when the Off schedule (index 0) is active.
  bool is_off_schedule() const { return this->active_schedule_idx_ == 0; }

  /// Returns true when the built-in last schedule is active
  /// ("Always" for PrimaryPumpSwitch, "When X is Running" for AuxiliaryPumpSwitch).
  bool is_builtin_last_schedule() const { return this->active_schedule_idx_ == this->schedules_.size() + 1; }

  /// Returns true when the pump has been off for at least 5 minutes (minimum off-time before restart).
  bool can_turn_on() const { return (millis_64() - this->last_off_ms_) >= (5u * 60u * 1000u); }

  /// Returns true when the disable-pumps sensor is configured and currently active.
  bool is_disabled() const { return this->disable_pumps_sensor_ != nullptr && this->disable_pumps_sensor_->state; }

  /// Returns the millis_64() timestamp when the pump last physically turned on.
  /// Used by PoolHeater to decide when 15 s of pump-on time has elapsed.
  uint64_t get_turned_on_ms() const { return this->turned_on_ms_; }

 protected:
  friend class PoolController;

  /// Resets accumulated runtime to zero. Called by PoolController at :00 and :30.
  void reset_runtime();

  /// Call this in write_state() before setting the output so runtime is tracked correctly.
  void track_runtime(bool new_state);
  void set_anomaly_detected_(bool detected);

  /// Re-evaluates the no-current problem sensor from current `state`/`motor_running_`.
  void update_no_current_();
  /// Sets (or clears) the latched flow-loss problem sensor.
  void set_flow_loss_(bool latched);
  /// Sets (or clears) the unexpected-flow-while-off problem sensor.
  void set_unexpected_flow_(bool detected);

  /// Returns the ScheduleRuntime from the active user-defined schedule that covers
  /// [slot_start_minute, slot_start_minute+30), for the given day_of_week (1=Sun..7=Sat).
  /// Returns nullptr if no matching runtime exists or the active schedule is not a user schedule.
  const ScheduleRuntime *find_active_runtime(uint16_t slot_start_minute, uint8_t day_of_week) const;

  output::BinaryOutput *output_ = nullptr;
  std::vector<Schedule> schedules_;

  size_t active_schedule_idx_{0};  ///< Index into schedules_ for the currently active schedule.
  uint32_t runtime_seconds_ = 0;   ///< Accumulated runtime (seconds) since last half-hour reset.
  uint64_t runtime_start_ms_ = 0;  ///< millis_64() when pump last turned on; 0 when off.
  uint64_t last_off_ms_ = 0;       ///< millis_64() when pump last turned off; used for 5-min cooldown.
  uint64_t turned_on_ms_ = 0;      ///< millis_64() when pump last physically turned on; used for turn-on sequencing.
  uint32_t sequence_delay_ms_ = 2000;  ///< Delay (ms) between primary and auxiliary pump state changes.
  binary_sensor::BinarySensor *disable_pumps_sensor_{
      nullptr};                       ///< Optional sensor that turns off pumps and blocks turn-ons when active.
  ESPPreferenceObject runtime_pref_;  ///< Persists runtime_seconds_ across reboots.

  // ── Anomaly detection state ────────────────────────────────────────────────
  bool enable_anomaly_detection_{false};
#ifdef USE_SENSOR
  sensor::Sensor *current_sensor_{nullptr};
#endif
  float anomaly_threshold_pct_{20.0f};  ///< % deviation from baseline that triggers an anomaly alert.
  uint32_t learning_samples_{200};      ///< Samples to collect before the baseline is locked.
  Trigger<std::string> *anomaly_trigger_{nullptr};

  AnomalyBaseline anomaly_baseline_{};
  bool baseline_locked_{false};       ///< True once learning_samples_ steady-state samples have been collected.
  uint32_t sample_count_{0};          ///< Steady-state samples collected so far.
  uint64_t last_sample_ms_{0};        ///< millis_64() of the last sensor sample; shared rate-limiter for loop features.
  uint64_t last_anomaly_ms_{0};       ///< millis_64() of the last anomaly fired; used for 5-min debounce.
  float startup_peak_current_{0.0f};  ///< Highest current seen during the current run's startup window.
  bool startup_processed_{false};     ///< True once the startup peak has been evaluated for this run.
  bool anomaly_detected_{false};      ///< True while the current run is flagged as anomalous.
  ESPPreferenceObject anomaly_pref_;  ///< Persists AnomalyBaseline across reboots.
  binary_sensor::BinarySensor *anomaly_status_sensor_{nullptr};

  // ── Current-based state fields ─────────────────────────────────────────────
  /// When a current sensor is configured, the published state still reflects the output
  /// command, but runtime is only accumulated while current confirms the motor is running.
  float current_on_threshold_{0.1f};  ///< Amps above which motor is considered running. Default: 0.1 A.
  bool motor_running_{false};         ///< True while current confirms the motor is actually drawing current.
  binary_sensor::BinarySensor *no_current_sensor_{nullptr};  ///< Problem sensor: on but no current detected.
  bool no_current_detected_{false};                          ///< Last published state of no_current_sensor_.

  /// Returns true when a current sensor is configured for this pump.
#ifdef USE_SENSOR
  bool has_current_sensor() const { return this->current_sensor_ != nullptr; }
#else
  bool has_current_sensor() const { return false; }
#endif

  // ── Flow sensor state ──────────────────────────────────────────────────────
  /// When a flow sensor is configured but no current sensor is, the published state still
  /// reflects the output command, but runtime is only accumulated while flow is detected.
  binary_sensor::BinarySensor *flow_sensor_{nullptr};  ///< Optional water flow sensor.
  uint32_t flow_timeout_ms_{2000};                     ///< Ms of absent flow before shutdown. Default: 2 s.
  uint64_t flow_check_start_ms_{0};  ///< millis_64() when no-flow condition first detected; 0 if clear.
  bool flow_running_{false};         ///< True while flow confirms the pump is running (used when no current sensor).
  binary_sensor::BinarySensor *flow_loss_sensor_{nullptr};        ///< Problem sensor: latched flow-loss shutdown.
  bool flow_loss_latched_{false};                                 ///< Last published state of flow_loss_sensor_.
  binary_sensor::BinarySensor *unexpected_flow_sensor_{nullptr};  ///< Problem sensor: flow detected while off.
  bool unexpected_flow_detected_{false};                          ///< Last published state of unexpected_flow_sensor_.
  uint64_t unexpected_flow_check_start_ms_{0};  ///< millis_64() when unexpected flow first detected; 0 if clear.

  /// Returns true when a flow sensor is configured for this pump.
  bool has_flow_sensor() const { return this->flow_sensor_ != nullptr; }

#ifdef USE_SENSOR
  void tick_anomaly_();                           ///< Called at 1 Hz while the pump is running.
  void process_startup_peak_();                   ///< Evaluates the inrush peak captured during the startup window.
  void fire_anomaly_(const std::string &reason);  ///< Logs + triggers the anomaly automation (5-min debounce).
#endif
};

class PumpAnomalySwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void write_state(bool state) override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

class PumpAnomalyStatusBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

class PumpAnomalyResetButton : public button::Button, public Component {
 public:
  void press_action() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Problem sensor: true while the pump is commanded on but no current is detected.
class PumpNoCurrentBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Problem sensor: latches true when the pump is shut down due to lost flow; only clears once
/// the pump is on, current confirms the motor is running, and flow is detected again.
class PumpFlowLossBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

/// Problem sensor: true when flow is detected while the pump is commanded off, debounced by
/// flow_timeout to allow for residual flow immediately after shutdown.
class PumpUnexpectedFlowBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void set_pump(PumpSwitch *pump) { this->pump_ = pump; }

 protected:
  PumpSwitch *pump_{nullptr};
};

class PrimaryPumpSwitch : public PumpSwitch {
 public:
  /// Registers the pool heater so it participates in the sequenced shutdown.
  void set_pool_heater(PoolHeater *heater) { this->pool_heater_ = heater; }

 protected:
  friend class AuxiliaryPumpSwitch;
  void add_auxiliary_pump(AuxiliaryPumpSwitch *aux_switch);
  void write_state(bool state) override;

  std::vector<AuxiliaryPumpSwitch *> auxiliary_pumps_;
  PoolHeater *pool_heater_{nullptr};
};

class AuxiliaryPumpSwitch : public PumpSwitch {
 public:
  void set_primary_pump(PrimaryPumpSwitch *primary_pump);

 protected:
  void write_state(bool state) override;

  PrimaryPumpSwitch *primary_pump_ = nullptr;
};

/// Automation trigger fired when a current-draw anomaly is detected.
/// The trigger argument `x` is one of:
///   "CURRENT_HIGH"     – steady-state current exceeds baseline by threshold_pct
///   "CURRENT_LOW"      – steady-state current is below baseline by threshold_pct
///   "NO_STARTUP_SPIKE" – inrush peak absent on start (possible capacitor fault)
///   "BASELINE_DRIFT"   – slow upward drift detected, suggesting bearing wear
class PumpAnomalyTrigger : public Trigger<std::string> {
 public:
  explicit PumpAnomalyTrigger(PumpSwitch *parent) { parent->set_anomaly_trigger(this); }
};

}  // namespace pool_controller
}  // namespace esphome
