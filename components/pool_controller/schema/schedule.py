from esphome.components.time import validate_cron_days_of_week
import esphome.config_validation as cv
from esphome.const import (
    CONF_DAYS_OF_WEEK,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_NAME,
    CONF_SECOND,
)

from ..const import CONF_END_TIME, CONF_MINUTES_PER_HOUR, CONF_RUNTIMES, CONF_START_TIME


def _time_to_minutes(time_val):
    """Convert a time_of_day dict to total minutes since midnight."""
    return time_val[CONF_HOUR] * 60 + time_val[CONF_MINUTE]


def _time_to_str(time_val):
    """Format a time_of_day dict for display."""
    return f"{time_val[CONF_HOUR]}:{time_val[CONF_MINUTE]:02d}"


def _validate_on_hour(time_val):
    """Validate that a time_of_day value falls on the hour."""
    if time_val[CONF_MINUTE] != 0:
        raise cv.Invalid(
            f"Minutes must be 00 (on the hour), got {time_val[CONF_MINUTE]:02d}"
        )
    return time_val


ON_HOUR_TIME = cv.All(cv.time_of_day, _validate_on_hour)

_MIDNIGHT_END = {CONF_HOUR: 24, CONF_MINUTE: 0, CONF_SECOND: 0}


def _validate_on_hour_end_time(value):
    """Like ON_HOUR_TIME but also accepts 24:00 to mean end of day."""
    value = cv.string(value)
    if value in ("24:00", "24:0"):
        return _MIDNIGHT_END
    return _validate_on_hour(cv.time_of_day(value))


ON_HOUR_END_TIME = _validate_on_hour_end_time

ALL_DAYS = set(range(1, 8))


def _days_to_mask(runtime):
    """Convert days_of_week list to a bitmask (bit0=Sun, bit1=Mon, ..., bit6=Sat)."""
    dow = runtime.get(CONF_DAYS_OF_WEEK)
    if dow is None:
        return 0x7F
    mask = 0
    for d in dow:
        mask |= 1 << (d - 1)
    return mask


def _runtime_days(runtime):
    """Return the set of days a runtime applies to (defaults to all days)."""
    dow = runtime.get(CONF_DAYS_OF_WEEK)
    return set(dow) if dow is not None else ALL_DAYS


def _validate_runtime(runtime):
    """Validate that start_time is strictly before end_time within a runtime."""
    start = _time_to_minutes(runtime[CONF_START_TIME])
    end = _time_to_minutes(runtime[CONF_END_TIME])
    if start >= end:
        raise cv.Invalid(
            f"start_time ({_time_to_str(runtime[CONF_START_TIME])}) must be before "
            f"end_time ({_time_to_str(runtime[CONF_END_TIME])})"
        )
    return runtime


def _validate_no_runtime_overlaps(schedule):
    """Validate that no two runtimes in the same schedule overlap on the same days."""
    runtimes = schedule[CONF_RUNTIMES]
    for i, a in enumerate(runtimes):
        for j in range(i + 1, len(runtimes)):
            b = runtimes[j]
            a_start = _time_to_minutes(a[CONF_START_TIME])
            a_end = _time_to_minutes(a[CONF_END_TIME])
            b_start = _time_to_minutes(b[CONF_START_TIME])
            b_end = _time_to_minutes(b[CONF_END_TIME])
            if (
                a_start < b_end
                and b_start < a_end
                and _runtime_days(a) & _runtime_days(b)
            ):
                raise cv.Invalid(
                    f"Runtime {i} ({_time_to_str(a[CONF_START_TIME])}-"
                    f"{_time_to_str(a[CONF_END_TIME])}) overlaps with "
                    f"runtime {j} ({_time_to_str(b[CONF_START_TIME])}-"
                    f"{_time_to_str(b[CONF_END_TIME])}) on shared days"
                )
    return schedule


RUNTIME_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_START_TIME): ON_HOUR_TIME,
            cv.Required(CONF_END_TIME): ON_HOUR_END_TIME,
            cv.Required(CONF_MINUTES_PER_HOUR): cv.All(
                cv.int_, cv.Range(min=1, max=60)
            ),
            cv.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
        }
    ),
    _validate_runtime,
)

SCHEDULE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.string,
            cv.Required(CONF_RUNTIMES): cv.All(
                cv.ensure_list(RUNTIME_SCHEMA),
                cv.Length(min=1, msg="Each schedule must define at least one runtime"),
            ),
        }
    ),
    _validate_no_runtime_overlaps,
)


def _validate_unique_schedule_names(schedules):
    """Validate that all schedule names are unique."""
    seen = {}
    for i, schedule in enumerate(schedules):
        name = schedule[CONF_NAME]
        if name in seen:
            raise cv.Invalid(
                f"Duplicate schedule name '{name}' at index {i} (first defined at index {seen[name]})"
            )
        seen[name] = i
    return schedules
