// =============================================================================
// CICS Emulation - Interval Control Module
// Version: 3.4.6
// =============================================================================
// Implements CICS Interval Control services:
// - ASKTIME: Get current time and date
// - DELAY: Suspend task for specified interval
// - POST: Post an event control area
// - WAIT: Wait for posted events
// - START: Start a transaction asynchronously
// - RETRIEVE: Retrieve data passed by START
// - CANCEL: Cancel a previously started transaction
// =============================================================================

#ifndef CICS_INTERVAL_CONTROL_HPP
#define CICS_INTERVAL_CONTROL_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace cics {
namespace interval {

// =============================================================================
// Time and Date Structures
// =============================================================================

struct AbsTime {
    UInt64 value = 0;  // Internal time value (microseconds since epoch)
    
    [[nodiscard]] static AbsTime now();
    [[nodiscard]] static AbsTime from_hhmmss(UInt32 hhmmss);
    [[nodiscard]] static AbsTime from_packed(UInt64 packed);
    
    [[nodiscard]] UInt32 to_hhmmss() const;
    [[nodiscard]] UInt32 to_yyyyddd() const;  // Julian date
    [[nodiscard]] UInt32 to_yyyymmdd() const;
    [[nodiscard]] String to_string() const;
    
    AbsTime operator+(const AbsTime& other) const { return {value + other.value}; }
    AbsTime operator-(const AbsTime& other) const { return {value - other.value}; }
    bool operator<(const AbsTime& other) const { return value < other.value; }
    bool operator<=(const AbsTime& other) const { return value <= other.value; }
    bool operator>(const AbsTime& other) const { return value > other.value; }
    bool operator>=(const AbsTime& other) const { return value >= other.value; }
    bool operator==(const AbsTime& other) const { return value == other.value; }
};

struct TimeInfo {
    AbsTime abstime;
    UInt32 date;       // YYYYMMDD format
    UInt32 dateform;   // Date format indicator
    UInt32 time;       // HHMMSS format
    UInt32 year;       // 4-digit year
    UInt32 month;      // Month (1-12)
    UInt32 dayofmonth; // Day of month (1-31)
    UInt32 dayofweek;  // Day of week (0=Sunday)
    UInt32 daycount;   // Days since Jan 1, 1900
    UInt32 milliseconds;
};

// =============================================================================
// Interval Specifications
// =============================================================================

enum class IntervalType {
    INTERVAL,  // Relative time (HHMMSS)
    TIME,      // Absolute time of day
    AFTER,     // After specified hours/minutes/seconds
    AT         // At specified time
};

struct IntervalSpec {
    IntervalType type = IntervalType::INTERVAL;
    UInt32 hours = 0;
    UInt32 minutes = 0;
    UInt32 seconds = 0;
    AbsTime abstime;
    
    [[nodiscard]] static IntervalSpec interval(UInt32 hours, UInt32 minutes, UInt32 seconds);
    [[nodiscard]] static IntervalSpec time(UInt32 hours, UInt32 minutes, UInt32 seconds);
    [[nodiscard]] static IntervalSpec after(UInt32 hours, UInt32 minutes, UInt32 seconds);
    [[nodiscard]] static IntervalSpec at(const AbsTime& time);
    
    [[nodiscard]] std::chrono::milliseconds to_duration() const;
    [[nodiscard]] AbsTime to_abstime() const;
};

// =============================================================================
// Event Control Area (ECA)
// =============================================================================

enum class EventStatus : UInt8 {
    NOT_POSTED = 0x00,
    POSTED = 0x40,
    EXPIRED = 0x80
};

struct EventControlArea {
    EventStatus status = EventStatus::NOT_POSTED;
    UInt32 event_id = 0;
    AbsTime post_time;
    ByteBuffer data;
    
    [[nodiscard]] bool is_posted() const { return status == EventStatus::POSTED; }
    [[nodiscard]] bool is_expired() const { return status == EventStatus::EXPIRED; }
    void reset() { status = EventStatus::NOT_POSTED; data.clear(); }
};

// =============================================================================
// START Transaction Request
// =============================================================================

struct StartRequest {
    FixedString<4> transaction_id;
    FixedString<4> terminal_id;
    ByteBuffer data;           // Data to pass via RETRIEVE
    IntervalSpec interval;
    UInt32 request_id = 0;
    AbsTime scheduled_time;
    bool cancelled = false;
    FixedString<8> user_id;
    String queue_name;         // For RETRIEVE with QUEUE
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// Interval Control Manager
// =============================================================================

using TransactionCallback = std::function<void(const StartRequest&)>;

class IntervalControlManager {
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::thread scheduler_thread_;
    
    // Event management
    std::unordered_map<UInt32, EventControlArea> events_;
    UInt32 next_event_id_ = 1;
    
    // START transaction queue
    std::priority_queue<StartRequest, std::vector<StartRequest>,
        std::function<bool(const StartRequest&, const StartRequest&)>> start_queue_;
    std::unordered_map<UInt32, StartRequest> pending_starts_;
    UInt32 next_request_id_ = 1;
    
    // Retrieve data storage (keyed by transaction+terminal)
    std::unordered_map<String, ByteBuffer> retrieve_data_;
    
    // Callbacks
    TransactionCallback transaction_callback_;
    
    // Statistics
    struct Statistics {
        UInt64 asktime_count = 0;
        UInt64 delay_count = 0;
        UInt64 post_count = 0;
        UInt64 wait_count = 0;
        UInt64 start_count = 0;
        UInt64 retrieve_count = 0;
        UInt64 cancel_count = 0;
    } stats_;
    
    void scheduler_loop();
    void process_scheduled_transactions();
    
public:
    IntervalControlManager();
    ~IntervalControlManager();
    
    // Singleton access
    static IntervalControlManager& instance();
    
    // Lifecycle
    Result<void> initialize();
    void shutdown();
    [[nodiscard]] bool is_running() const { return running_; }
    
    // ASKTIME - Get current time
    Result<TimeInfo> asktime();
    Result<AbsTime> asktime_abstime();
    
    // DELAY - Suspend task
    Result<void> delay(const IntervalSpec& interval);
    Result<void> delay_interval(UInt32 hours, UInt32 minutes, UInt32 seconds);
    Result<void> delay_for(std::chrono::milliseconds duration);
    
    // POST - Post event
    Result<void> post(UInt32 event_id);
    Result<void> post(EventControlArea& eca);
    Result<void> post_with_data(UInt32 event_id, const ByteBuffer& data);
    
    // WAIT - Wait for events
    Result<UInt32> wait_event(UInt32 event_id, const IntervalSpec& timeout);
    Result<UInt32> wait_event(EventControlArea& eca, const IntervalSpec& timeout);
    Result<std::vector<UInt32>> wait_any(const std::vector<UInt32>& event_ids, 
                                          const IntervalSpec& timeout);
    
    // START - Start transaction
    Result<UInt32> start(const FixedString<4>& transid, const IntervalSpec& interval);
    Result<UInt32> start(const FixedString<4>& transid, const IntervalSpec& interval,
                         const ByteBuffer& data);
    Result<UInt32> start(const FixedString<4>& transid, const FixedString<4>& termid,
                         const IntervalSpec& interval, const ByteBuffer& data);
    
    // RETRIEVE - Retrieve passed data
    Result<ByteBuffer> retrieve();
    Result<ByteBuffer> retrieve(const String& queue_name);
    Result<UInt32> retrieve_length();
    
    // CANCEL - Cancel started transaction
    Result<void> cancel(UInt32 request_id);
    Result<void> cancel(const FixedString<4>& transid);
    
    // Event management
    Result<UInt32> create_event();
    Result<void> delete_event(UInt32 event_id);
    Result<EventControlArea*> get_event(UInt32 event_id);
    
    // Set callbacks
    void set_transaction_callback(TransactionCallback callback);
    
    // Internal: Store data for RETRIEVE
    void store_retrieve_data(const String& key, const ByteBuffer& data);
    
    // Statistics
    [[nodiscard]] String get_statistics() const;
    void reset_statistics();
};

// =============================================================================
// EXEC CICS Style Interface
// =============================================================================

Result<TimeInfo> exec_cics_asktime();
Result<void> exec_cics_delay_interval(UInt32 hhmmss);
Result<void> exec_cics_delay_time(UInt32 hhmmss);
Result<void> exec_cics_delay_for(UInt32 hours, UInt32 minutes, UInt32 seconds);
Result<void> exec_cics_post(EventControlArea& eca);
Result<void> exec_cics_wait_event(EventControlArea& eca, UInt32 timeout_hhmmss);
Result<UInt32> exec_cics_start(StringView transid, UInt32 interval_hhmmss);
Result<UInt32> exec_cics_start(StringView transid, UInt32 interval_hhmmss, 
                                ConstByteSpan data);
Result<ByteBuffer> exec_cics_retrieve();
Result<void> exec_cics_cancel(UInt32 request_id);

} // namespace interval
} // namespace cics

#endif // CICS_INTERVAL_CONTROL_HPP
