#pragma once
// =============================================================================
// IBM CICS Emulation - DateTime Utilities
// Version: 3.4.6
// CICS-compatible date/time handling for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::datetime {

// =============================================================================
// CICS Date Formats
// =============================================================================

// ABSTIME - Milliseconds since January 1, 1900
using AbsTime = Int64;

// CICS packed date format YYYYDDD (Julian)
struct PackedDate {
    UInt32 value = 0;
    
    PackedDate() = default;
    explicit PackedDate(UInt32 v) : value(v) {}
    
    [[nodiscard]] UInt16 year() const { return static_cast<UInt16>(value / 1000); }
    [[nodiscard]] UInt16 day_of_year() const { return static_cast<UInt16>(value % 1000); }
    
    [[nodiscard]] String to_string() const;
};

// CICS packed time format HHMMSS
struct PackedTime {
    UInt32 value = 0;
    
    PackedTime() = default;
    explicit PackedTime(UInt32 v) : value(v) {}
    
    [[nodiscard]] UInt8 hours() const { return static_cast<UInt8>(value / 10000); }
    [[nodiscard]] UInt8 minutes() const { return static_cast<UInt8>((value / 100) % 100); }
    [[nodiscard]] UInt8 seconds() const { return static_cast<UInt8>(value % 100); }
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Date/Time Structure
// =============================================================================

struct DateTime {
    UInt16 year = 0;
    UInt8 month = 0;      // 1-12
    UInt8 day = 0;        // 1-31
    UInt8 hour = 0;       // 0-23
    UInt8 minute = 0;     // 0-59
    UInt8 second = 0;     // 0-59
    UInt16 millisecond = 0;  // 0-999
    Int16 tz_offset_minutes = 0;  // Timezone offset from UTC
    
    // Day of week (0=Sunday, 6=Saturday)
    [[nodiscard]] UInt8 day_of_week() const;
    
    // Day of year (1-366)
    [[nodiscard]] UInt16 day_of_year() const;
    
    // Week number (1-53)
    [[nodiscard]] UInt8 week_number() const;
    
    // Quarter (1-4)
    [[nodiscard]] UInt8 quarter() const { return static_cast<UInt8>(((month - 1) / 3) + 1); }
    
    // Is leap year
    [[nodiscard]] bool is_leap_year() const;
    
    // Validation
    [[nodiscard]] bool is_valid() const;
    
    // Formatting
    [[nodiscard]] String to_string() const;
    [[nodiscard]] String to_iso8601() const;
    [[nodiscard]] String format(StringView fmt) const;
    
    // CICS formats
    [[nodiscard]] PackedDate to_packed_date() const;
    [[nodiscard]] PackedTime to_packed_time() const;
    [[nodiscard]] AbsTime to_abstime() const;
    
    // System time point
    [[nodiscard]] SystemTimePoint to_system_time() const;
    
    // Comparison
    [[nodiscard]] auto operator<=>(const DateTime&) const = default;
};

// =============================================================================
// Duration
// =============================================================================

struct TimeDuration {
    Int32 days = 0;
    Int8 hours = 0;
    Int8 minutes = 0;
    Int8 seconds = 0;
    Int16 milliseconds = 0;
    
    [[nodiscard]] Int64 total_milliseconds() const;
    [[nodiscard]] Int64 total_seconds() const;
    [[nodiscard]] double total_hours() const;
    [[nodiscard]] double total_days() const;
    
    [[nodiscard]] String to_string() const;
    
    [[nodiscard]] bool is_negative() const;
    [[nodiscard]] TimeDuration abs() const;
    
    TimeDuration operator-() const;
    TimeDuration& operator+=(const TimeDuration& other);
    TimeDuration& operator-=(const TimeDuration& other);
    TimeDuration operator+(const TimeDuration& other) const;
    TimeDuration operator-(const TimeDuration& other) const;
};

// =============================================================================
// Factory Functions
// =============================================================================

// Get current date/time
[[nodiscard]] DateTime now();
[[nodiscard]] DateTime now_utc();
[[nodiscard]] DateTime today();

// Create from components
[[nodiscard]] DateTime make_datetime(UInt16 year, UInt8 month, UInt8 day,
                                     UInt8 hour = 0, UInt8 minute = 0, 
                                     UInt8 second = 0, UInt16 ms = 0);

// Create from CICS formats
[[nodiscard]] DateTime from_packed_date(PackedDate date);
[[nodiscard]] DateTime from_packed_time(PackedTime time);
[[nodiscard]] DateTime from_abstime(AbsTime abstime);
[[nodiscard]] DateTime from_system_time(SystemTimePoint tp);

// Parse from string
[[nodiscard]] Result<DateTime> parse(StringView str, StringView format = "");
[[nodiscard]] Result<DateTime> parse_iso8601(StringView str);

// =============================================================================
// Arithmetic Operations
// =============================================================================

[[nodiscard]] DateTime add_years(const DateTime& dt, Int32 years);
[[nodiscard]] DateTime add_months(const DateTime& dt, Int32 months);
[[nodiscard]] DateTime add_days(const DateTime& dt, Int32 days);
[[nodiscard]] DateTime add_hours(const DateTime& dt, Int32 hours);
[[nodiscard]] DateTime add_minutes(const DateTime& dt, Int32 minutes);
[[nodiscard]] DateTime add_seconds(const DateTime& dt, Int32 seconds);
[[nodiscard]] DateTime add_milliseconds(const DateTime& dt, Int64 ms);
[[nodiscard]] DateTime add_duration(const DateTime& dt, const TimeDuration& dur);

[[nodiscard]] TimeDuration difference(const DateTime& dt1, const DateTime& dt2);

// =============================================================================
// Timezone Operations
// =============================================================================

[[nodiscard]] DateTime to_utc(const DateTime& dt);
[[nodiscard]] DateTime to_local(const DateTime& dt);
[[nodiscard]] DateTime to_timezone(const DateTime& dt, Int16 offset_minutes);

[[nodiscard]] Int16 local_timezone_offset();
[[nodiscard]] String timezone_name();

// =============================================================================
// Date Calculations
// =============================================================================

[[nodiscard]] UInt8 days_in_month(UInt16 year, UInt8 month);
[[nodiscard]] UInt16 days_in_year(UInt16 year);
[[nodiscard]] bool is_leap_year(UInt16 year);

// First/last day of month
[[nodiscard]] DateTime first_day_of_month(const DateTime& dt);
[[nodiscard]] DateTime last_day_of_month(const DateTime& dt);

// First/last day of year
[[nodiscard]] DateTime first_day_of_year(const DateTime& dt);
[[nodiscard]] DateTime last_day_of_year(const DateTime& dt);

// Next/previous weekday
[[nodiscard]] DateTime next_weekday(const DateTime& dt, UInt8 weekday);
[[nodiscard]] DateTime previous_weekday(const DateTime& dt, UInt8 weekday);

// =============================================================================
// Formatting Constants
// =============================================================================

namespace format {
    constexpr StringView ISO8601 = "%Y-%m-%dT%H:%M:%S";
    constexpr StringView ISO8601_MS = "%Y-%m-%dT%H:%M:%S.%f";
    constexpr StringView DATE_ONLY = "%Y-%m-%d";
    constexpr StringView TIME_ONLY = "%H:%M:%S";
    constexpr StringView DATE_US = "%m/%d/%Y";
    constexpr StringView DATE_EU = "%d/%m/%Y";
    constexpr StringView CICS_DATE = "%Y%j";      // YYYYDDD
    constexpr StringView CICS_TIME = "%H%M%S";    // HHMMSS
    constexpr StringView FULL = "%Y-%m-%d %H:%M:%S";
}

// =============================================================================
// CICS-Specific Functions
// =============================================================================

// ASKTIME equivalent - returns current ABSTIME
[[nodiscard]] AbsTime asktime();

// FORMATTIME equivalent
[[nodiscard]] String formattime(AbsTime abstime, StringView format);

// Convert EIBDATE (0CYYDDD) to DateTime
[[nodiscard]] DateTime from_eibdate(UInt32 eibdate);

// Convert EIBTIME (0HHMMSS) to DateTime components
[[nodiscard]] PackedTime from_eibtime(UInt32 eibtime);

// Create EIBDATE format
[[nodiscard]] UInt32 to_eibdate(const DateTime& dt);

// Create EIBTIME format  
[[nodiscard]] UInt32 to_eibtime(const DateTime& dt);

// =============================================================================
// Timer Utilities
// =============================================================================

class StopWatch {
private:
    TimePoint start_;
    TimePoint stop_;
    bool running_ = false;
    
public:
    StopWatch();
    
    void start();
    void stop();
    void reset();
    void restart();
    
    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] Duration elapsed() const;
    [[nodiscard]] double elapsed_seconds() const;
    [[nodiscard]] Int64 elapsed_milliseconds() const;
    [[nodiscard]] Int64 elapsed_microseconds() const;
};

// =============================================================================
// Date Literals
// =============================================================================

namespace literals {
    // Duration literals
    [[nodiscard]] constexpr TimeDuration operator""_days(unsigned long long d) {
        return TimeDuration{static_cast<Int32>(d), 0, 0, 0, 0};
    }
    
    [[nodiscard]] constexpr TimeDuration operator""_hours(unsigned long long h) {
        return TimeDuration{0, static_cast<Int8>(h), 0, 0, 0};
    }
    
    [[nodiscard]] constexpr TimeDuration operator""_mins(unsigned long long m) {
        return TimeDuration{0, 0, static_cast<Int8>(m), 0, 0};
    }
    
    [[nodiscard]] constexpr TimeDuration operator""_secs(unsigned long long s) {
        return TimeDuration{0, 0, 0, static_cast<Int8>(s), 0};
    }
}

} // namespace cics::datetime
