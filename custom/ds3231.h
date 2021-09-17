#include "esphome.h"

// DS3232 Register Addresses
#define ADDR_RTC 0x00
#define ADDR_ALRM 0x07
#define ADDR_CTRL 0x0E
#define ADDR_STAT 0x0F

#define MASK_ALRM_TYPE_M1 0x01
#define MASK_ALRM_TYPE_M2 0x02
#define MASK_ALRM_TYPE_M3 0x04
#define MASK_ALRM_TYPE_M4 0x08
#define MASK_ALRM_TYPE_DAY_MODE 0x10
#define MASK_ALRM_TYPE_INT 0x40
#define MASK_ALRM_TYPE_NUM 0x80

enum DS3231AlarmType {
  Alarm1EverySecond = 0x0F,
  Alarm1EverySecondWithInterupt = 0x4F,
  Alarm1MatchSecond = 0x0E,
  Alarm1MatchSecondWithInterupt = 0x4E,
  Alarm1MatchMinuteSecond = 0x0C,
  Alarm1MatchMinuteSecondWithInterupt = 0x4C,
  Alarm1MatchHourMinuteSecond = 0x08,
  Alarm1MatchHourMinuteSecondWithInterupt = 0x48,
  Alarm1MatchDayOfMonthHourMinuteSecond = 0x00,
  Alarm1MatchDayOfMonthHourMinuteSecondWithInterupt = 0x40,
  Alarm1MatchDayOfWeekHourMinuteSecond = 0x10,
  Alarm1MatchDayOfWeekHourMinuteSecondWithInterupt = 0x50,
  Alarm2EveryMinute = 0x8E,
  Alarm2EveryMinuteWithInterupt = 0xCE,
  Alarm2MatchMinute = 0x8C,
  Alarm2MatchMinuteWithInterupt = 0xCC,
  Alarm2MatchHourMinute = 0x88,
  Alarm2MatchHourMinuteWithInterupt = 0xC8,
  Alarm2MatchDayOfMonthHourMinute = 0x80,
  Alarm2MatchDayOfMonthHourMinuteWithInterupt = 0xC0,
  Alarm2MatchDayOfWeekHourMinute = 0x90,
  Alarm2MatchDayOfWeekHourMinuteWithInterupt = 0xD0,
};

enum DS3231SquareWaveMode {
  SquareWave,
  Interupt,
};

enum DS3231SquareWaveFrequency {
  Frequency1Hz = 0x00,
  Frequency1024Hz = 0x01,
  Frequency4096Hz = 0x02,
  Frequency8192Hz = 0x03,
};

using namespace esphome;

class DS3231Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void read_time();
  void write_time();
  void set_alarm(DS3231AlarmType alarm_type, byte second, uint8_t minute, uint8_t hour, uint8_t day);
  void set_sqw_mode(DS3231SquareWaveMode mode, DS3231SquareWaveFrequency frequency);
  void reset_alarm(uint8_t alarm);

 protected:
  bool read_rtc_();
  bool write_rtc_();
  bool read_alrm_();
  bool write_alrm_();
  bool read_ctrl_();
  bool write_ctrl_();
  bool read_stat_();
  bool write_stat_();
  struct DS3231Reg {
    union DS3231RTC {
      struct {
        uint8_t second : 4;
        uint8_t second_10 : 3;
        uint8_t unused_1 : 1;

        uint8_t minute : 4;
        uint8_t minute_10 : 3;
        uint8_t unused_2 : 1;

        uint8_t hour : 4;
        uint8_t hour_10 : 2;
        uint8_t unused_3 : 2;

        uint8_t weekday : 3;
        uint8_t unused_4 : 5;

        uint8_t day : 4;
        uint8_t day_10 : 2;
        uint8_t unused_5 : 2;

        uint8_t month : 4;
        uint8_t month_10 : 1;
        uint8_t unused_6 : 2;
        uint8_t cent : 1;

        uint8_t year : 4;
        uint8_t year_10 : 4;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } rtc;
    union DS3231Alrm {
      struct {
        uint8_t a1_second : 4;
        uint8_t a1_second_10 : 3;
        bool a1_m1 : 1;

        uint8_t a1_minute : 4;
        uint8_t a1_minute_10 : 3;
        bool a1_m2 : 1;

        uint8_t a1_hour : 4;
        uint8_t a1_hour_10 : 2;
        uint8_t unused_1 : 1;
        bool a1_m3 : 1;

        uint8_t a1_day : 4;
        uint8_t a1_day_10 : 2;
        uint8_t a1_day_mode : 1;
        bool a1_m4 : 1;

        uint8_t a2_minute : 4;
        uint8_t a2_minute_10 : 3;
        bool a2_m2 : 1;

        uint8_t a2_hour : 4;
        uint8_t a2_hour_10 : 2;
        uint8_t unused_2 : 1;
        bool a2_m3 : 1;

        uint8_t a2_day : 4;
        uint8_t a2_day_10 : 2;
        uint8_t a2_day_mode : 1;
        bool a2_m4 : 1;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } alrm;
    union DS3231Ctrl {
      struct {
        bool alrm_1_int : 1;
        bool alrm_2_int : 1;
        bool int_ctrl : 1;
        uint8_t rs : 2;
        bool conv_tmp : 1;
        bool bat_sqw : 1;
        bool osc_dis : 1;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } ctrl;
    union DS3231Stat {
      struct {
        bool alrm_1_act : 1;
        bool alrm_2_act : 1;
        bool busy : 1;
        bool en32khz : 1;
        uint8_t unused : 3;
        bool osc_stop : 1;
      } reg;
    mutable uint8_t raw[sizeof(reg)];
    } stat;
  } ds3231_;
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};


// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS3231.pdf

static const char *TAG = "ds3231";

void DS3231Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3231...");
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
  if(!this->read_alrm_()) {
    this->mark_failed();
  }
  if(!this->read_ctrl_()) {
    this->mark_failed();
  }

  if(!this->read_stat_()) {
    this->mark_failed();
  }

  this->reset_alarm(1);
  this->set_alarm(DS3231AlarmType::Alarm1MatchSecondWithInterupt, 0, 0, 0, 0);
}

void DS3231Component::update() { this->read_time(); }

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS3231 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float DS3231Component::get_setup_priority() const { return setup_priority::DATA; }

void DS3231Component::read_time() {
  if (!this->read_stat_()) {
    return;
  }
  if (ds3231_.stat.reg.osc_stop) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  if (!this->read_rtc_()) {
    return;
  }
  time::ESPTime rtc_time{.second = uint8_t(ds3231_.rtc.reg.second + 10 * ds3231_.rtc.reg.second_10),
                         .minute = uint8_t(ds3231_.rtc.reg.minute + 10u * ds3231_.rtc.reg.minute_10),
                         .hour = uint8_t(ds3231_.rtc.reg.hour + 10u * ds3231_.rtc.reg.hour_10),
                         .day_of_week = uint8_t(ds3231_.rtc.reg.weekday),
                         .day_of_month = uint8_t(ds3231_.rtc.reg.day + 10u * ds3231_.rtc.reg.day_10),
                         .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
                         .month = uint8_t(ds3231_.rtc.reg.month + 10u * ds3231_.rtc.reg.month_10),
                         .year = uint16_t(ds3231_.rtc.reg.year + 10u * ds3231_.rtc.reg.year_10 + 2000)};
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void DS3231Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  this->read_stat_();
  if (ds3231_.stat.reg.osc_stop) {
    ds3231_.stat.reg.osc_stop = false;
    this->write_stat_();
  }
  ds3231_.rtc.reg.second = now.second % 10;
  ds3231_.rtc.reg.second_10 = now.second / 10;
  ds3231_.rtc.reg.minute = now.minute % 10;
  ds3231_.rtc.reg.minute_10 = now.minute / 10;
  ds3231_.rtc.reg.hour = now.hour % 10;
  ds3231_.rtc.reg.hour_10 = now.hour / 10;
  ds3231_.rtc.reg.weekday = now.day_of_week;
  ds3231_.rtc.reg.day = now.day_of_month % 10;
  ds3231_.rtc.reg.day_10 = now.day_of_month / 10;
  ds3231_.rtc.reg.month = now.month % 10;
  ds3231_.rtc.reg.month_10 = now.month / 10;
  ds3231_.rtc.reg.year = (now.year - 2000) % 10;
  ds3231_.rtc.reg.year_10 = (now.year - 2000) / 10 % 10;
  this->write_rtc_();
}

void DS3231Component::set_alarm(DS3231AlarmType alarm_type, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day) {
  bool need_ctrl_write = false;
  // Alarm 1
  if (!(alarm_type & MASK_ALRM_TYPE_NUM)) {
    ds3231_.alrm.reg.a1_second = second % 10;
    ds3231_.alrm.reg.a1_second_10 = second / 10;
    ds3231_.alrm.reg.a1_m1 = alarm_type & MASK_ALRM_TYPE_M1;
    ds3231_.alrm.reg.a1_minute = minute % 10;
    ds3231_.alrm.reg.a1_minute_10 = minute / 10;
    ds3231_.alrm.reg.a1_m2 = alarm_type & MASK_ALRM_TYPE_M2;
    ds3231_.alrm.reg.a1_hour = hour % 10;
    ds3231_.alrm.reg.a1_hour_10 = hour / 10;
    ds3231_.alrm.reg.a1_m3 = alarm_type & MASK_ALRM_TYPE_M3;
    ds3231_.alrm.reg.a1_day = day % 10;
    ds3231_.alrm.reg.a1_day_10 = day / 10;
    ds3231_.alrm.reg.a1_day_mode = alarm_type & MASK_ALRM_TYPE_DAY_MODE;
    ds3231_.alrm.reg.a1_m4 = alarm_type & MASK_ALRM_TYPE_M4;
    if (ds3231_.ctrl.reg.alrm_1_int != bool(alarm_type & MASK_ALRM_TYPE_INT)) {
      ds3231_.ctrl.reg.alrm_1_int = bool(alarm_type & MASK_ALRM_TYPE_INT);
      need_ctrl_write = true;
    }
  }
  // Alarm 2
  else {
    ds3231_.alrm.reg.a2_minute = minute % 10;
    ds3231_.alrm.reg.a2_minute_10 = minute / 10;
    ds3231_.alrm.reg.a2_m2 = alarm_type & MASK_ALRM_TYPE_M2;
    ds3231_.alrm.reg.a2_hour = hour % 10;
    ds3231_.alrm.reg.a2_hour_10 = hour / 10;
    ds3231_.alrm.reg.a2_m3 = alarm_type & MASK_ALRM_TYPE_M3;
    ds3231_.alrm.reg.a2_day = day % 10;
    ds3231_.alrm.reg.a2_day_10 = day / 10;
    ds3231_.alrm.reg.a2_day_mode = alarm_type & MASK_ALRM_TYPE_DAY_MODE;
    ds3231_.alrm.reg.a2_m4 = alarm_type & MASK_ALRM_TYPE_M4;
    if (ds3231_.ctrl.reg.alrm_2_int != bool(alarm_type & MASK_ALRM_TYPE_INT)) {
      ds3231_.ctrl.reg.alrm_2_int = bool(alarm_type & MASK_ALRM_TYPE_INT);
      need_ctrl_write = true;
    }
  }
  this->write_alrm_();
  if (need_ctrl_write) {
    this->write_ctrl_();
  }
}

void DS3231Component::set_sqw_mode(DS3231SquareWaveMode mode, DS3231SquareWaveFrequency frequency) {
  this->read_ctrl_();
  if (mode == DS3231SquareWaveMode::Interupt && !ds3231_.ctrl.reg.int_ctrl) {
    ds3231_.ctrl.reg.int_ctrl = true;
    this->write_ctrl_();
  }
  else if (mode == DS3231SquareWaveMode::SquareWave && (ds3231_.ctrl.reg.int_ctrl || ds3231_.ctrl.reg.rs != frequency)) {
    ds3231_.ctrl.reg.int_ctrl = false;
    ds3231_.ctrl.reg.rs = frequency;
    this->write_ctrl_();
  }
}

void DS3231Component::reset_alarm(uint8_t alarm) {
  read_stat_();
  if (alarm == 1 && ds3231_.stat.reg.alrm_1_act) {
    ds3231_.stat.reg.alrm_1_act = false;
    write_stat_();
  } else if (alarm == 2 && ds3231_.stat.reg.alrm_2_act) {
    ds3231_.stat.reg.alrm_2_act = false;
    write_stat_();
  }
}

bool DS3231Component::read_rtc_() {
  if (!this->read_bytes(ADDR_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           ds3231_.rtc.reg.hour_10, ds3231_.rtc.reg.hour,
           ds3231_.rtc.reg.minute_10, ds3231_.rtc.reg.minute,
           ds3231_.rtc.reg.second_10, ds3231_.rtc.reg.second,
           ds3231_.rtc.reg.year_10, ds3231_.rtc.reg.year,
           ds3231_.rtc.reg.month_10, ds3231_.rtc.reg.month,
           ds3231_.rtc.reg.day_10, ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::write_rtc_() {
  if (!this->write_bytes(ADDR_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           ds3231_.rtc.reg.hour_10, ds3231_.rtc.reg.hour,
           ds3231_.rtc.reg.minute_10, ds3231_.rtc.reg.minute,
           ds3231_.rtc.reg.second_10, ds3231_.rtc.reg.second,
           ds3231_.rtc.reg.year_10, ds3231_.rtc.reg.year,
           ds3231_.rtc.reg.month_10, ds3231_.rtc.reg.month,
           ds3231_.rtc.reg.day_10, ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::read_alrm_() {
  if (!this->read_bytes(ADDR_ALRM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a1_hour_10, ds3231_.alrm.reg.a1_hour,
           ds3231_.alrm.reg.a1_minute_10, ds3231_.alrm.reg.a1_minute,
           ds3231_.alrm.reg.a1_second_10, ds3231_.alrm.reg.a1_second,
           ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a1_day_10, ds3231_.alrm.reg.a1_day,
           ds3231_.alrm.reg.a1_m1, ds3231_.alrm.reg.a1_m2, ds3231_.alrm.reg.a1_m3, ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Read  Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a2_hour_10, ds3231_.alrm.reg.a2_hour,
           ds3231_.alrm.reg.a2_minute_10, ds3231_.alrm.reg.a2_minute,
           ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a2_day_10, ds3231_.alrm.reg.a2_day,
           ds3231_.alrm.reg.a2_m2, ds3231_.alrm.reg.a2_m3, ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::write_alrm_() {
  if (!this->write_bytes(ADDR_ALRM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a1_hour_10, ds3231_.alrm.reg.a1_hour,
           ds3231_.alrm.reg.a1_minute_10, ds3231_.alrm.reg.a1_minute,
           ds3231_.alrm.reg.a1_second_10, ds3231_.alrm.reg.a1_second,
           ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a1_day_10, ds3231_.alrm.reg.a1_day,
           ds3231_.alrm.reg.a1_m1, ds3231_.alrm.reg.a1_m2, ds3231_.alrm.reg.a1_m3, ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Write Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a2_hour_10, ds3231_.alrm.reg.a2_hour,
           ds3231_.alrm.reg.a2_minute_10, ds3231_.alrm.reg.a2_minute,
           ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a2_day_10, ds3231_.alrm.reg.a2_day,
           ds3231_.alrm.reg.a2_m2, ds3231_.alrm.reg.a2_m3, ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::read_ctrl_() {
  if (!this->read_bytes(ADDR_CTRL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(ds3231_.ctrl.reg.alrm_1_int),
           ONOFF(ds3231_.ctrl.reg.alrm_2_int),
           ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW",
           ds3231_.ctrl.reg.rs,
           ONOFF(ds3231_.ctrl.reg.conv_tmp),
           ONOFF(ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::write_ctrl_() {
  if (!this->write_bytes(ADDR_CTRL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(ds3231_.ctrl.reg.alrm_1_int),
           ONOFF(ds3231_.ctrl.reg.alrm_2_int),
           ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW",
           ds3231_.ctrl.reg.rs,
           ONOFF(ds3231_.ctrl.reg.conv_tmp),
           ONOFF(ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::read_stat_() {
  if (!this->read_bytes(ADDR_STAT, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  A1:%s A2:%s BSY:%s 32K:%s OSC:%s",
           ONOFF(ds3231_.stat.reg.alrm_1_act),
           ONOFF(ds3231_.stat.reg.alrm_2_act),
           YESNO(ds3231_.stat.reg.busy),
           ONOFF(ds3231_.stat.reg.en32khz),
           ONOFF(!ds3231_.stat.reg.osc_stop));
  return true;
}

bool DS3231Component::write_stat_() {
  if (!this->write_bytes(ADDR_STAT, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write A1:%s A2:%s BSY:%s 32K:%s OSC:%s",
           ONOFF(ds3231_.stat.reg.alrm_1_act),
           ONOFF(ds3231_.stat.reg.alrm_2_act),
           YESNO(ds3231_.stat.reg.busy),
           ONOFF(ds3231_.stat.reg.en32khz),
           ONOFF(!ds3231_.stat.reg.osc_stop));
  return true;
}

DS3231Component *DS3231;