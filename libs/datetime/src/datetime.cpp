// =============================================================================
// IBM CICS Emulation - DateTime Utilities Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/datetime/datetime.hpp>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace cics::datetime {

// =============================================================================
// Constants
// =============================================================================

// Days in each month (non-leap year)
static constexpr UInt8 DAYS_IN_MONTH[] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// Cumulative days before each month (non-leap year)
static constexpr UInt16 DAYS_BEFORE_MONTH[] = {
    0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

// ABSTIME epoch: January 1, 1900
static constexpr Int64 ABSTIME_EPOCH_OFFSET = 2208988800000LL;  // ms from 1900 to 1970

// =============================================================================
// PackedDate Implementation
// =============================================================================

String PackedDate::to_string() const {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year()
        << std::setw(3) << day_of_year();
    return oss.str();
}

// =============================================================================
// PackedTime Implementation
// =============================================================================

String PackedTime::to_string() const {
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(2) << static_cast<int>(hours()) << ":"
        << std::setw(2) << static_cast<int>(minutes()) << ":"
        << std::setw(2) << static_cast<int>(seconds());
    return oss.str();
}

// =============================================================================
// DateTime Implementation
// =============================================================================

bool DateTime::is_leap_year() const {
    return cics::datetime::is_leap_year(year);
}

UInt8 DateTime::day_of_week() const {
    // Zeller's congruence
    int y = year;
    int m = month;
    if (m < 3) {
        m += 12;
        y--;
    }
    int k = y % 100;
    int j = y / 100;
    int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return static_cast<UInt8>((h + 6) % 7);  // Convert to Sunday=0
}

UInt16 DateTime::day_of_year() const {
    UInt16 doy = DAYS_BEFORE_MONTH[month] + day;
    if (month > 2 && is_leap_year()) {
        doy++;
    }
    return doy;
}

UInt8 DateTime::week_number() const {
    // ISO 8601 week number
    DateTime jan1 = make_datetime(year, 1, 1);
    int jan1_dow = jan1.day_of_week();
    
    int doy = day_of_year();
    int week = (doy + jan1_dow - 1) / 7;
    
    // Adjust for weeks starting on Monday
    if (jan1_dow <= 4) {
        week++;
    }
    
    return static_cast<UInt8>(std::max(1, week));
}

bool DateTime::is_valid() const {
    if (year < 1 || year > 9999) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > days_in_month(year, month)) return false;
    if (hour > 23) return false;
    if (minute > 59) return false;
    if (second > 59) return false;
    if (millisecond > 999) return false;
    return true;
}

String DateTime::to_string() const {
    return format("%Y-%m-%d %H:%M:%S");
}

String DateTime::to_iso8601() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << year << "-"
        << std::setw(2) << static_cast<int>(month) << "-"
        << std::setw(2) << static_cast<int>(day) << "T"
        << std::setw(2) << static_cast<int>(hour) << ":"
        << std::setw(2) << static_cast<int>(minute) << ":"
        << std::setw(2) << static_cast<int>(second);
    
    if (millisecond > 0) {
        oss << "." << std::setw(3) << millisecond;
    }
    
    if (tz_offset_minutes == 0) {
        oss << "Z";
    } else {
        int offset_hours = std::abs(tz_offset_minutes) / 60;
        int offset_mins = std::abs(tz_offset_minutes) % 60;
        oss << (tz_offset_minutes >= 0 ? "+" : "-")
            << std::setw(2) << offset_hours << ":"
            << std::setw(2) << offset_mins;
    }
    
    return oss.str();
}

String DateTime::format(StringView fmt) const {
    std::ostringstream oss;
    
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.size()) {
            switch (fmt[++i]) {
                case 'Y': oss << std::setfill('0') << std::setw(4) << year; break;
                case 'y': oss << std::setfill('0') << std::setw(2) << (year % 100); break;
                case 'm': oss << std::setfill('0') << std::setw(2) << static_cast<int>(month); break;
                case 'd': oss << std::setfill('0') << std::setw(2) << static_cast<int>(day); break;
                case 'H': oss << std::setfill('0') << std::setw(2) << static_cast<int>(hour); break;
                case 'M': oss << std::setfill('0') << std::setw(2) << static_cast<int>(minute); break;
                case 'S': oss << std::setfill('0') << std::setw(2) << static_cast<int>(second); break;
                case 'f': oss << std::setfill('0') << std::setw(3) << millisecond; break;
                case 'j': oss << std::setfill('0') << std::setw(3) << day_of_year(); break;
                case 'w': oss << static_cast<int>(day_of_week()); break;
                case 'W': oss << std::setfill('0') << std::setw(2) << static_cast<int>(week_number()); break;
                case '%': oss << '%'; break;
                default: oss << '%' << fmt[i]; break;
            }
        } else {
            oss << fmt[i];
        }
    }
    
    return oss.str();
}

PackedDate DateTime::to_packed_date() const {
    return PackedDate(year * 1000 + day_of_year());
}

PackedTime DateTime::to_packed_time() const {
    return PackedTime(hour * 10000 + minute * 100 + second);
}

AbsTime DateTime::to_abstime() const {
    // Convert to milliseconds since 1900
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;
    
    std::time_t t = std::mktime(&tm);
    if (t == -1) return 0;
    
    return static_cast<Int64>(t) * 1000 + millisecond + ABSTIME_EPOCH_OFFSET;
}

SystemTimePoint DateTime::to_system_time() const {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;
    
    std::time_t t = std::mktime(&tm);
    return SystemClock::from_time_t(t) + Milliseconds(millisecond);
}

// =============================================================================
// TimeDuration Implementation
// =============================================================================

Int64 TimeDuration::total_milliseconds() const {
    return static_cast<Int64>(days) * 86400000LL +
           static_cast<Int64>(hours) * 3600000LL +
           static_cast<Int64>(minutes) * 60000LL +
           static_cast<Int64>(seconds) * 1000LL +
           milliseconds;
}

Int64 TimeDuration::total_seconds() const {
    return total_milliseconds() / 1000;
}

double TimeDuration::total_hours() const {
    return static_cast<double>(total_milliseconds()) / 3600000.0;
}

double TimeDuration::total_days() const {
    return static_cast<double>(total_milliseconds()) / 86400000.0;
}

String TimeDuration::to_string() const {
    std::ostringstream oss;
    if (days != 0) oss << days << "d ";
    oss << std::setfill('0')
        << std::setw(2) << static_cast<int>(std::abs(hours)) << ":"
        << std::setw(2) << static_cast<int>(std::abs(minutes)) << ":"
        << std::setw(2) << static_cast<int>(std::abs(seconds));
    if (milliseconds != 0) {
        oss << "." << std::setw(3) << std::abs(milliseconds);
    }
    return oss.str();
}

bool TimeDuration::is_negative() const {
    return total_milliseconds() < 0;
}

TimeDuration TimeDuration::abs() const {
    if (!is_negative()) return *this;
    return -(*this);
}

TimeDuration TimeDuration::operator-() const {
    return TimeDuration{-days, 
                        static_cast<Int8>(-hours), 
                        static_cast<Int8>(-minutes), 
                        static_cast<Int8>(-seconds), 
                        static_cast<Int16>(-milliseconds)};
}

TimeDuration& TimeDuration::operator+=(const TimeDuration& other) {
    Int64 total = total_milliseconds() + other.total_milliseconds();
    *this = TimeDuration{
        static_cast<Int32>(total / 86400000),
        static_cast<Int8>((total % 86400000) / 3600000),
        static_cast<Int8>((total % 3600000) / 60000),
        static_cast<Int8>((total % 60000) / 1000),
        static_cast<Int16>(total % 1000)
    };
    return *this;
}

TimeDuration& TimeDuration::operator-=(const TimeDuration& other) {
    return *this += (-other);
}

TimeDuration TimeDuration::operator+(const TimeDuration& other) const {
    TimeDuration result = *this;
    result += other;
    return result;
}

TimeDuration TimeDuration::operator-(const TimeDuration& other) const {
    TimeDuration result = *this;
    result -= other;
    return result;
}

// =============================================================================
// Factory Functions
// =============================================================================

DateTime now() {
    auto tp = SystemClock::now();
    return from_system_time(tp);
}

DateTime now_utc() {
    auto tp = SystemClock::now();
    std::time_t t = SystemClock::to_time_t(tp);
    std::tm* tm = std::gmtime(&t);
    
    DateTime dt;
    dt.year = static_cast<UInt16>(tm->tm_year + 1900);
    dt.month = static_cast<UInt8>(tm->tm_mon + 1);
    dt.day = static_cast<UInt8>(tm->tm_mday);
    dt.hour = static_cast<UInt8>(tm->tm_hour);
    dt.minute = static_cast<UInt8>(tm->tm_min);
    dt.second = static_cast<UInt8>(tm->tm_sec);
    
    auto ms = std::chrono::duration_cast<Milliseconds>(
        tp.time_since_epoch() % Seconds(1));
    dt.millisecond = static_cast<UInt16>(ms.count());
    dt.tz_offset_minutes = 0;
    
    return dt;
}

DateTime today() {
    DateTime dt = now();
    dt.hour = 0;
    dt.minute = 0;
    dt.second = 0;
    dt.millisecond = 0;
    return dt;
}

DateTime make_datetime(UInt16 year, UInt8 month, UInt8 day,
                       UInt8 hour, UInt8 minute, UInt8 second, UInt16 ms) {
    DateTime dt;
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    dt.millisecond = ms;
    dt.tz_offset_minutes = local_timezone_offset();
    return dt;
}

DateTime from_packed_date(PackedDate date) {
    UInt16 year = date.year();
    UInt16 doy = date.day_of_year();
    
    // Convert day of year to month/day
    UInt8 month = 1;
    UInt16 remaining = doy;
    
    while (month <= 12) {
        UInt8 dim = days_in_month(year, month);
        if (remaining <= dim) break;
        remaining -= dim;
        month++;
    }
    
    return make_datetime(year, month, static_cast<UInt8>(remaining));
}

DateTime from_packed_time(PackedTime time) {
    DateTime dt = today();
    dt.hour = time.hours();
    dt.minute = time.minutes();
    dt.second = time.seconds();
    return dt;
}

DateTime from_abstime(AbsTime abstime) {
    Int64 ms_since_1970 = abstime - ABSTIME_EPOCH_OFFSET;
    std::time_t t = static_cast<std::time_t>(ms_since_1970 / 1000);
    UInt16 ms = static_cast<UInt16>(ms_since_1970 % 1000);
    
    std::tm* tm = std::localtime(&t);
    
    DateTime dt;
    dt.year = static_cast<UInt16>(tm->tm_year + 1900);
    dt.month = static_cast<UInt8>(tm->tm_mon + 1);
    dt.day = static_cast<UInt8>(tm->tm_mday);
    dt.hour = static_cast<UInt8>(tm->tm_hour);
    dt.minute = static_cast<UInt8>(tm->tm_min);
    dt.second = static_cast<UInt8>(tm->tm_sec);
    dt.millisecond = ms;
    dt.tz_offset_minutes = local_timezone_offset();
    
    return dt;
}

DateTime from_system_time(SystemTimePoint tp) {
    std::time_t t = SystemClock::to_time_t(tp);
    std::tm* tm = std::localtime(&t);
    
    DateTime dt;
    dt.year = static_cast<UInt16>(tm->tm_year + 1900);
    dt.month = static_cast<UInt8>(tm->tm_mon + 1);
    dt.day = static_cast<UInt8>(tm->tm_mday);
    dt.hour = static_cast<UInt8>(tm->tm_hour);
    dt.minute = static_cast<UInt8>(tm->tm_min);
    dt.second = static_cast<UInt8>(tm->tm_sec);
    
    auto ms = std::chrono::duration_cast<Milliseconds>(
        tp.time_since_epoch() % Seconds(1));
    dt.millisecond = static_cast<UInt16>(ms.count());
    dt.tz_offset_minutes = local_timezone_offset();
    
    return dt;
}

Result<DateTime> parse(StringView str, StringView format) {
    // Simple parser for common formats
    if (str.empty()) {
        return make_error<DateTime>(ErrorCode::INVALID_ARGUMENT, "Empty date string");
    }
    
    // Try ISO 8601 first
    if (format.empty() || format == format::ISO8601) {
        return parse_iso8601(str);
    }
    
    return make_error<DateTime>(ErrorCode::INVALID_ARGUMENT, "Unsupported format");
}

Result<DateTime> parse_iso8601(StringView str) {
    DateTime dt{};
    
    // Minimum: YYYY-MM-DD
    if (str.size() < 10) {
        return make_error<DateTime>(ErrorCode::INVALID_ARGUMENT, "Invalid ISO 8601 format");
    }
    
    // Parse date
    try {
        dt.year = static_cast<UInt16>(std::stoi(String(str.substr(0, 4))));
        dt.month = static_cast<UInt8>(std::stoi(String(str.substr(5, 2))));
        dt.day = static_cast<UInt8>(std::stoi(String(str.substr(8, 2))));
        
        // Parse time if present
        if (str.size() > 11 && (str[10] == 'T' || str[10] == ' ')) {
            if (str.size() >= 19) {
                dt.hour = static_cast<UInt8>(std::stoi(String(str.substr(11, 2))));
                dt.minute = static_cast<UInt8>(std::stoi(String(str.substr(14, 2))));
                dt.second = static_cast<UInt8>(std::stoi(String(str.substr(17, 2))));
            }
            
            // Parse milliseconds
            if (str.size() > 20 && str[19] == '.') {
                size_t ms_end = 20;
                while (ms_end < str.size() && std::isdigit(str[ms_end])) ms_end++;
                String ms_str(str.substr(20, ms_end - 20));
                while (ms_str.size() < 3) ms_str += '0';
                dt.millisecond = static_cast<UInt16>(std::stoi(ms_str.substr(0, 3)));
            }
        }
    } catch (...) {
        return make_error<DateTime>(ErrorCode::INVALID_ARGUMENT, "Invalid date components");
    }
    
    if (!dt.is_valid()) {
        return make_error<DateTime>(ErrorCode::INVALID_ARGUMENT, "Invalid date values");
    }
    
    return dt;
}

// =============================================================================
// Arithmetic Operations
// =============================================================================

DateTime add_years(const DateTime& dt, Int32 years) {
    DateTime result = dt;
    result.year = static_cast<UInt16>(dt.year + years);
    
    // Handle Feb 29 in non-leap years
    if (result.month == 2 && result.day == 29 && !result.is_leap_year()) {
        result.day = 28;
    }
    
    return result;
}

DateTime add_months(const DateTime& dt, Int32 months) {
    DateTime result = dt;
    
    Int32 total_months = dt.year * 12 + dt.month - 1 + months;
    result.year = static_cast<UInt16>(total_months / 12);
    result.month = static_cast<UInt8>((total_months % 12) + 1);
    
    // Clamp day to valid range
    UInt8 max_day = days_in_month(result.year, result.month);
    if (result.day > max_day) {
        result.day = max_day;
    }
    
    return result;
}

DateTime add_days(const DateTime& dt, Int32 days) {
    return add_milliseconds(dt, static_cast<Int64>(days) * 86400000);
}

DateTime add_hours(const DateTime& dt, Int32 hours) {
    return add_milliseconds(dt, static_cast<Int64>(hours) * 3600000);
}

DateTime add_minutes(const DateTime& dt, Int32 minutes) {
    return add_milliseconds(dt, static_cast<Int64>(minutes) * 60000);
}

DateTime add_seconds(const DateTime& dt, Int32 seconds) {
    return add_milliseconds(dt, static_cast<Int64>(seconds) * 1000);
}

DateTime add_milliseconds(const DateTime& dt, Int64 ms) {
    AbsTime abstime = dt.to_abstime() + ms;
    return from_abstime(abstime);
}

DateTime add_duration(const DateTime& dt, const TimeDuration& dur) {
    return add_milliseconds(dt, dur.total_milliseconds());
}

TimeDuration difference(const DateTime& dt1, const DateTime& dt2) {
    Int64 ms = dt1.to_abstime() - dt2.to_abstime();
    
    return TimeDuration{
        static_cast<Int32>(ms / 86400000),
        static_cast<Int8>((ms % 86400000) / 3600000),
        static_cast<Int8>((ms % 3600000) / 60000),
        static_cast<Int8>((ms % 60000) / 1000),
        static_cast<Int16>(ms % 1000)
    };
}

// =============================================================================
// Timezone Operations
// =============================================================================

DateTime to_utc(const DateTime& dt) {
    return add_minutes(dt, -dt.tz_offset_minutes);
}

DateTime to_local(const DateTime& dt) {
    DateTime result = dt;
    result.tz_offset_minutes = local_timezone_offset();
    return add_minutes(dt, result.tz_offset_minutes - dt.tz_offset_minutes);
}

DateTime to_timezone(const DateTime& dt, Int16 offset_minutes) {
    DateTime result = add_minutes(dt, offset_minutes - dt.tz_offset_minutes);
    result.tz_offset_minutes = offset_minutes;
    return result;
}

Int16 local_timezone_offset() {
    std::time_t t = std::time(nullptr);
    std::tm local_tm = *std::localtime(&t);
    std::tm utc_tm = *std::gmtime(&t);
    
    Int32 diff = (local_tm.tm_hour - utc_tm.tm_hour) * 60 +
                 (local_tm.tm_min - utc_tm.tm_min);
    
    // Handle day boundary
    if (local_tm.tm_mday != utc_tm.tm_mday) {
        diff += (local_tm.tm_mday > utc_tm.tm_mday) ? 1440 : -1440;
    }
    
    return static_cast<Int16>(diff);
}

String timezone_name() {
#ifdef _WIN32
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    std::wstring ws(tzi.StandardName);
    return String(ws.begin(), ws.end());
#else
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    return tm->tm_zone ? tm->tm_zone : "UTC";
#endif
}

// =============================================================================
// Date Calculations
// =============================================================================

UInt8 days_in_month(UInt16 year, UInt8 month) {
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return DAYS_IN_MONTH[month];
}

UInt16 days_in_year(UInt16 year) {
    return is_leap_year(year) ? 366 : 365;
}

bool is_leap_year(UInt16 year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

DateTime first_day_of_month(const DateTime& dt) {
    DateTime result = dt;
    result.day = 1;
    result.hour = 0;
    result.minute = 0;
    result.second = 0;
    result.millisecond = 0;
    return result;
}

DateTime last_day_of_month(const DateTime& dt) {
    DateTime result = dt;
    result.day = days_in_month(dt.year, dt.month);
    result.hour = 23;
    result.minute = 59;
    result.second = 59;
    result.millisecond = 999;
    return result;
}

DateTime first_day_of_year(const DateTime& dt) {
    return make_datetime(dt.year, 1, 1);
}

DateTime last_day_of_year(const DateTime& dt) {
    return make_datetime(dt.year, 12, 31, 23, 59, 59, 999);
}

DateTime next_weekday(const DateTime& dt, UInt8 weekday) {
    UInt8 current = dt.day_of_week();
    Int32 days_ahead = weekday - current;
    if (days_ahead <= 0) days_ahead += 7;
    return add_days(dt, days_ahead);
}

DateTime previous_weekday(const DateTime& dt, UInt8 weekday) {
    UInt8 current = dt.day_of_week();
    Int32 days_back = current - weekday;
    if (days_back <= 0) days_back += 7;
    return add_days(dt, -days_back);
}

// =============================================================================
// CICS-Specific Functions
// =============================================================================

AbsTime asktime() {
    return now().to_abstime();
}

String formattime(AbsTime abstime, StringView format) {
    DateTime dt = from_abstime(abstime);
    return dt.format(format);
}

DateTime from_eibdate(UInt32 eibdate) {
    // EIBDATE format: 0CYYDDD where C=century (0=1900s, 1=2000s)
    UInt8 century = static_cast<UInt8>((eibdate / 100000) % 10);
    UInt8 yy = static_cast<UInt8>((eibdate / 1000) % 100);
    UInt16 doy = static_cast<UInt16>(eibdate % 1000);
    
    UInt16 year = static_cast<UInt16>((century == 0 ? 1900 : 2000) + yy);
    
    PackedDate pd(year * 1000 + doy);
    return from_packed_date(pd);
}

PackedTime from_eibtime(UInt32 eibtime) {
    // EIBTIME format: 0HHMMSS
    return PackedTime(eibtime % 1000000);
}

UInt32 to_eibdate(const DateTime& dt) {
    UInt8 century = (dt.year >= 2000) ? 1 : 0;
    UInt8 yy = static_cast<UInt8>(dt.year % 100);
    return century * 100000 + yy * 1000 + dt.day_of_year();
}

UInt32 to_eibtime(const DateTime& dt) {
    return dt.hour * 10000 + dt.minute * 100 + dt.second;
}

// =============================================================================
// StopWatch Implementation
// =============================================================================

StopWatch::StopWatch() : start_(Clock::now()), stop_(start_) {}

void StopWatch::start() {
    if (!running_) {
        start_ = Clock::now();
        running_ = true;
    }
}

void StopWatch::stop() {
    if (running_) {
        stop_ = Clock::now();
        running_ = false;
    }
}

void StopWatch::reset() {
    start_ = Clock::now();
    stop_ = start_;
    running_ = false;
}

void StopWatch::restart() {
    reset();
    start();
}

Duration StopWatch::elapsed() const {
    if (running_) {
        return Clock::now() - start_;
    }
    return stop_ - start_;
}

double StopWatch::elapsed_seconds() const {
    return std::chrono::duration<double>(elapsed()).count();
}

Int64 StopWatch::elapsed_milliseconds() const {
    return std::chrono::duration_cast<Milliseconds>(elapsed()).count();
}

Int64 StopWatch::elapsed_microseconds() const {
    return std::chrono::duration_cast<Microseconds>(elapsed()).count();
}

} // namespace cics::datetime
