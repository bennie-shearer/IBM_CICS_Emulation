// =============================================================================
// CICS Emulation - Interval Control Module Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/interval/interval_control.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace cics {
namespace interval {

// =============================================================================
// AbsTime Implementation
// =============================================================================

AbsTime AbsTime::now() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return {static_cast<UInt64>(
        std::chrono::duration_cast<std::chrono::microseconds>(duration).count())};
}

AbsTime AbsTime::from_hhmmss(UInt32 hhmmss) {
    UInt32 hours = hhmmss / 10000;
    UInt32 minutes = (hhmmss / 100) % 100;
    UInt32 seconds = hhmmss % 100;
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_t_now);
    
    tm_now->tm_hour = static_cast<int>(hours);
    tm_now->tm_min = static_cast<int>(minutes);
    tm_now->tm_sec = static_cast<int>(seconds);
    
    auto target_time = std::chrono::system_clock::from_time_t(std::mktime(tm_now));
    return {static_cast<UInt64>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            target_time.time_since_epoch()).count())};
}

AbsTime AbsTime::from_packed(UInt64 packed) {
    return {packed};
}

UInt32 AbsTime::to_hhmmss() const {
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::microseconds(value));
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm_val = std::localtime(&time_t_val);
    
    return static_cast<UInt32>(tm_val->tm_hour * 10000 + 
                                tm_val->tm_min * 100 + 
                                tm_val->tm_sec);
}

UInt32 AbsTime::to_yyyyddd() const {
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::microseconds(value));
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm_val = std::localtime(&time_t_val);
    
    return static_cast<UInt32>((tm_val->tm_year + 1900) * 1000 + 
                                tm_val->tm_yday + 1);
}

UInt32 AbsTime::to_yyyymmdd() const {
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::microseconds(value));
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm_val = std::localtime(&time_t_val);
    
    return static_cast<UInt32>((tm_val->tm_year + 1900) * 10000 + 
                                (tm_val->tm_mon + 1) * 100 + 
                                tm_val->tm_mday);
}

String AbsTime::to_string() const {
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::microseconds(value));
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm_val = std::localtime(&time_t_val);
    
    std::ostringstream oss;
    oss << std::put_time(tm_val, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// =============================================================================
// IntervalSpec Implementation
// =============================================================================

IntervalSpec IntervalSpec::interval(UInt32 h, UInt32 m, UInt32 s) {
    IntervalSpec spec;
    spec.type = IntervalType::INTERVAL;
    spec.hours = h;
    spec.minutes = m;
    spec.seconds = s;
    return spec;
}

IntervalSpec IntervalSpec::time(UInt32 h, UInt32 m, UInt32 s) {
    IntervalSpec spec;
    spec.type = IntervalType::TIME;
    spec.hours = h;
    spec.minutes = m;
    spec.seconds = s;
    return spec;
}

IntervalSpec IntervalSpec::after(UInt32 h, UInt32 m, UInt32 s) {
    IntervalSpec spec;
    spec.type = IntervalType::AFTER;
    spec.hours = h;
    spec.minutes = m;
    spec.seconds = s;
    return spec;
}

IntervalSpec IntervalSpec::at(const AbsTime& t) {
    IntervalSpec spec;
    spec.type = IntervalType::AT;
    spec.abstime = t;
    return spec;
}

std::chrono::milliseconds IntervalSpec::to_duration() const {
    switch (type) {
        case IntervalType::INTERVAL:
        case IntervalType::AFTER:
            return std::chrono::hours(hours) + 
                   std::chrono::minutes(minutes) + 
                   std::chrono::seconds(seconds);
        case IntervalType::TIME: {
            auto now = AbsTime::now();
            auto target = AbsTime::from_hhmmss(
                static_cast<UInt32>(hours * 10000 + minutes * 100 + seconds));
            if (target.value <= now.value) {
                // Target time has passed, schedule for tomorrow
                target.value += 24ULL * 60 * 60 * 1000000; // Add 24 hours
            }
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(target.value - now.value));
        }
        case IntervalType::AT: {
            auto now = AbsTime::now();
            if (abstime.value <= now.value) {
                return std::chrono::milliseconds(0);
            }
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(abstime.value - now.value));
        }
    }
    return std::chrono::milliseconds(0);
}

AbsTime IntervalSpec::to_abstime() const {
    auto duration = to_duration();
    auto now = AbsTime::now();
    return {now.value + static_cast<UInt64>(
        std::chrono::duration_cast<std::chrono::microseconds>(duration).count())};
}

// =============================================================================
// StartRequest Implementation
// =============================================================================

String StartRequest::to_string() const {
    std::ostringstream oss;
    oss << "StartRequest{id=" << request_id
        << ", trans=" << String(transaction_id.data(), transaction_id.size())
        << ", scheduled=" << scheduled_time.to_string()
        << ", cancelled=" << (cancelled ? "yes" : "no")
        << "}";
    return oss.str();
}

// =============================================================================
// IntervalControlManager Implementation
// =============================================================================

IntervalControlManager::IntervalControlManager()
    : start_queue_([](const StartRequest& a, const StartRequest& b) {
        return a.scheduled_time > b.scheduled_time; // Min-heap by time
    }) {
}

IntervalControlManager::~IntervalControlManager() {
    shutdown();
}

IntervalControlManager& IntervalControlManager::instance() {
    static IntervalControlManager instance;
    return instance;
}

Result<void> IntervalControlManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return make_success();
    }
    
    running_ = true;
    scheduler_thread_ = std::thread(&IntervalControlManager::scheduler_loop, this);
    
    return make_success();
}

void IntervalControlManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void IntervalControlManager::scheduler_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (start_queue_.empty()) {
            cv_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }
        
        auto now = AbsTime::now();
        const auto& next = start_queue_.top();
        
        if (next.scheduled_time <= now) {
            StartRequest request = start_queue_.top();
            start_queue_.pop();
            pending_starts_.erase(request.request_id);
            
            if (!request.cancelled) {
                // Store data for RETRIEVE
                if (!request.data.empty()) {
                    String key = String(request.transaction_id.data(), 4) + "_" +
                                String(request.terminal_id.data(), 4);
                    retrieve_data_[key] = request.data;
                }
                
                // Call transaction callback
                if (transaction_callback_) {
                    lock.unlock();
                    transaction_callback_(request);
                    lock.lock();
                }
            }
        } else {
            auto wait_time = std::chrono::microseconds(
                next.scheduled_time.value - now.value);
            cv_.wait_for(lock, wait_time);
        }
    }
}

Result<TimeInfo> IntervalControlManager::asktime() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.asktime_count;
    
    TimeInfo info;
    info.abstime = AbsTime::now();
    
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::microseconds(info.abstime.value));
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm_val = std::localtime(&time_t_val);
    
    info.date = static_cast<UInt32>((tm_val->tm_year + 1900) * 10000 + 
                                     (tm_val->tm_mon + 1) * 100 + 
                                     tm_val->tm_mday);
    info.dateform = 0; // YYYYMMDD format
    info.time = static_cast<UInt32>(tm_val->tm_hour * 10000 + 
                                     tm_val->tm_min * 100 + 
                                     tm_val->tm_sec);
    info.year = static_cast<UInt32>(tm_val->tm_year + 1900);
    info.month = static_cast<UInt32>(tm_val->tm_mon + 1);
    info.dayofmonth = static_cast<UInt32>(tm_val->tm_mday);
    info.dayofweek = static_cast<UInt32>(tm_val->tm_wday);
    
    // Calculate days since Jan 1, 1900
    std::tm epoch_tm = {};
    epoch_tm.tm_year = 0; // 1900
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_mday = 1;
    auto epoch_time = std::mktime(&epoch_tm);
    info.daycount = static_cast<UInt32>(
        std::difftime(time_t_val, epoch_time) / (24 * 60 * 60));
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()) % 1000;
    info.milliseconds = static_cast<UInt32>(ms.count());
    
    return make_success(info);
}

Result<AbsTime> IntervalControlManager::asktime_abstime() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.asktime_count;
    return make_success(AbsTime::now());
}

Result<void> IntervalControlManager::delay(const IntervalSpec& interval) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.delay_count;
    }
    
    auto duration = interval.to_duration();
    std::this_thread::sleep_for(duration);
    return make_success();
}

Result<void> IntervalControlManager::delay_interval(UInt32 hours, UInt32 minutes, UInt32 seconds) {
    return delay(IntervalSpec::interval(hours, minutes, seconds));
}

Result<void> IntervalControlManager::delay_for(std::chrono::milliseconds duration) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.delay_count;
    }
    std::this_thread::sleep_for(duration);
    return make_success();
}

Result<void> IntervalControlManager::post(UInt32 event_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.post_count;
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND, 
            "Event not found: " + std::to_string(event_id));
    }
    
    it->second.status = EventStatus::POSTED;
    it->second.post_time = AbsTime::now();
    cv_.notify_all();
    
    return make_success();
}

Result<void> IntervalControlManager::post(EventControlArea& eca) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.post_count;
    
    eca.status = EventStatus::POSTED;
    eca.post_time = AbsTime::now();
    cv_.notify_all();
    
    return make_success();
}

Result<void> IntervalControlManager::post_with_data(UInt32 event_id, const ByteBuffer& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.post_count;
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Event not found: " + std::to_string(event_id));
    }
    
    it->second.status = EventStatus::POSTED;
    it->second.post_time = AbsTime::now();
    it->second.data = data;
    cv_.notify_all();
    
    return make_success();
}

Result<UInt32> IntervalControlManager::wait_event(UInt32 event_id, const IntervalSpec& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++stats_.wait_count;
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return make_error<UInt32>(ErrorCode::RECORD_NOT_FOUND,
            "Event not found: " + std::to_string(event_id));
    }
    
    auto deadline = timeout.to_abstime();
    
    while (it->second.status != EventStatus::POSTED) {
        auto now = AbsTime::now();
        if (now >= deadline) {
            it->second.status = EventStatus::EXPIRED;
            return make_error<UInt32>(ErrorCode::TIMEOUT, "Wait timed out");
        }
        
        auto wait_time = std::chrono::microseconds(deadline.value - now.value);
        cv_.wait_for(lock, wait_time);
    }
    
    return make_success(event_id);
}

Result<UInt32> IntervalControlManager::wait_event(EventControlArea& eca, const IntervalSpec& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++stats_.wait_count;
    
    auto deadline = timeout.to_abstime();
    
    while (eca.status != EventStatus::POSTED) {
        auto now = AbsTime::now();
        if (now >= deadline) {
            eca.status = EventStatus::EXPIRED;
            return make_error<UInt32>(ErrorCode::TIMEOUT, "Wait timed out");
        }
        
        auto wait_time = std::chrono::microseconds(deadline.value - now.value);
        cv_.wait_for(lock, wait_time);
    }
    
    return make_success(eca.event_id);
}

Result<std::vector<UInt32>> IntervalControlManager::wait_any(
    const std::vector<UInt32>& event_ids, const IntervalSpec& timeout) {
    
    std::unique_lock<std::mutex> lock(mutex_);
    ++stats_.wait_count;
    
    auto deadline = timeout.to_abstime();
    std::vector<UInt32> posted;
    
    while (posted.empty()) {
        for (UInt32 id : event_ids) {
            auto it = events_.find(id);
            if (it != events_.end() && it->second.status == EventStatus::POSTED) {
                posted.push_back(id);
            }
        }
        
        if (!posted.empty()) break;
        
        auto now = AbsTime::now();
        if (now >= deadline) {
            return make_error<std::vector<UInt32>>(ErrorCode::TIMEOUT, "Wait timed out");
        }
        
        auto wait_time = std::chrono::microseconds(deadline.value - now.value);
        cv_.wait_for(lock, wait_time);
    }
    
    return make_success(posted);
}

Result<UInt32> IntervalControlManager::start(const FixedString<4>& transid, 
                                              const IntervalSpec& interval) {
    return start(transid, interval, ByteBuffer{});
}

Result<UInt32> IntervalControlManager::start(const FixedString<4>& transid,
                                              const IntervalSpec& interval,
                                              const ByteBuffer& data) {
    FixedString<4> empty_term;
    return start(transid, empty_term, interval, data);
}

Result<UInt32> IntervalControlManager::start(const FixedString<4>& transid,
                                              const FixedString<4>& termid,
                                              const IntervalSpec& interval,
                                              const ByteBuffer& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.start_count;
    
    StartRequest request;
    request.transaction_id = transid;
    request.terminal_id = termid;
    request.data = data;
    request.interval = interval;
    request.request_id = next_request_id_++;
    request.scheduled_time = interval.to_abstime();
    request.cancelled = false;
    
    pending_starts_[request.request_id] = request;
    start_queue_.push(request);
    cv_.notify_one();
    
    return make_success(request.request_id);
}

Result<ByteBuffer> IntervalControlManager::retrieve() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.retrieve_count;
    
    // For simplicity, return first available data
    if (retrieve_data_.empty()) {
        return make_error<ByteBuffer>(ErrorCode::RECORD_NOT_FOUND,
            "No data to retrieve");
    }
    
    auto it = retrieve_data_.begin();
    ByteBuffer data = std::move(it->second);
    retrieve_data_.erase(it);
    
    return make_success(std::move(data));
}

Result<ByteBuffer> IntervalControlManager::retrieve(const String& queue_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.retrieve_count;
    
    auto it = retrieve_data_.find(queue_name);
    if (it == retrieve_data_.end()) {
        return make_error<ByteBuffer>(ErrorCode::RECORD_NOT_FOUND,
            "No data for queue: " + queue_name);
    }
    
    ByteBuffer data = std::move(it->second);
    retrieve_data_.erase(it);
    
    return make_success(std::move(data));
}

Result<UInt32> IntervalControlManager::retrieve_length() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (retrieve_data_.empty()) {
        return make_success(static_cast<UInt32>(0));
    }
    
    return make_success(
        static_cast<UInt32>(retrieve_data_.begin()->second.size()));
}

Result<void> IntervalControlManager::cancel(UInt32 request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.cancel_count;
    
    auto it = pending_starts_.find(request_id);
    if (it == pending_starts_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Request not found: " + std::to_string(request_id));
    }
    
    it->second.cancelled = true;
    return make_success();
}

Result<void> IntervalControlManager::cancel(const FixedString<4>& transid) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.cancel_count;
    
    bool found = false;
    for (auto& [id, request] : pending_starts_) {
        if (request.transaction_id == transid && !request.cancelled) {
            request.cancelled = true;
            found = true;
        }
    }
    
    if (!found) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "No pending requests for transaction");
    }
    
    return make_success();
}

Result<UInt32> IntervalControlManager::create_event() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    UInt32 event_id = next_event_id_++;
    events_[event_id] = EventControlArea{};
    events_[event_id].event_id = event_id;
    
    return make_success(event_id);
}

Result<void> IntervalControlManager::delete_event(UInt32 event_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return make_error<void>(ErrorCode::RECORD_NOT_FOUND,
            "Event not found: " + std::to_string(event_id));
    }
    
    events_.erase(it);
    return make_success();
}

Result<EventControlArea*> IntervalControlManager::get_event(UInt32 event_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return make_error<EventControlArea*>(ErrorCode::RECORD_NOT_FOUND,
            "Event not found: " + std::to_string(event_id));
    }
    
    return make_success(&it->second);
}

void IntervalControlManager::set_transaction_callback(TransactionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    transaction_callback_ = std::move(callback);
}

void IntervalControlManager::store_retrieve_data(const String& key, const ByteBuffer& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    retrieve_data_[key] = data;
}

String IntervalControlManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "Interval Control Statistics:\n"
        << "  ASKTIME calls:   " << stats_.asktime_count << "\n"
        << "  DELAY calls:     " << stats_.delay_count << "\n"
        << "  POST calls:      " << stats_.post_count << "\n"
        << "  WAIT calls:      " << stats_.wait_count << "\n"
        << "  START calls:     " << stats_.start_count << "\n"
        << "  RETRIEVE calls:  " << stats_.retrieve_count << "\n"
        << "  CANCEL calls:    " << stats_.cancel_count << "\n"
        << "  Active events:   " << events_.size() << "\n"
        << "  Pending starts:  " << pending_starts_.size() << "\n";
    
    return oss.str();
}

void IntervalControlManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Statistics{};
}

// =============================================================================
// EXEC CICS Interface Implementation
// =============================================================================

Result<TimeInfo> exec_cics_asktime() {
    return IntervalControlManager::instance().asktime();
}

Result<void> exec_cics_delay_interval(UInt32 hhmmss) {
    UInt32 hours = hhmmss / 10000;
    UInt32 minutes = (hhmmss / 100) % 100;
    UInt32 seconds = hhmmss % 100;
    return IntervalControlManager::instance().delay_interval(hours, minutes, seconds);
}

Result<void> exec_cics_delay_time(UInt32 hhmmss) {
    UInt32 hours = hhmmss / 10000;
    UInt32 minutes = (hhmmss / 100) % 100;
    UInt32 seconds = hhmmss % 100;
    return IntervalControlManager::instance().delay(
        IntervalSpec::time(hours, minutes, seconds));
}

Result<void> exec_cics_delay_for(UInt32 hours, UInt32 minutes, UInt32 seconds) {
    return IntervalControlManager::instance().delay_interval(hours, minutes, seconds);
}

Result<void> exec_cics_post(EventControlArea& eca) {
    return IntervalControlManager::instance().post(eca);
}

Result<void> exec_cics_wait_event(EventControlArea& eca, UInt32 timeout_hhmmss) {
    UInt32 hours = timeout_hhmmss / 10000;
    UInt32 minutes = (timeout_hhmmss / 100) % 100;
    UInt32 seconds = timeout_hhmmss % 100;
    
    auto result = IntervalControlManager::instance().wait_event(
        eca, IntervalSpec::interval(hours, minutes, seconds));
    
    if (result.is_success()) {
        return make_success();
    }
    return make_error<void>(result.error().code, result.error().message);
}

Result<UInt32> exec_cics_start(StringView transid, UInt32 interval_hhmmss) {
    FixedString<4> trans;
    trans = transid;
    
    UInt32 hours = interval_hhmmss / 10000;
    UInt32 minutes = (interval_hhmmss / 100) % 100;
    UInt32 seconds = interval_hhmmss % 100;
    
    return IntervalControlManager::instance().start(
        trans, IntervalSpec::interval(hours, minutes, seconds));
}

Result<UInt32> exec_cics_start(StringView transid, UInt32 interval_hhmmss, 
                                ConstByteSpan data) {
    FixedString<4> trans;
    trans = transid;
    
    UInt32 hours = interval_hhmmss / 10000;
    UInt32 minutes = (interval_hhmmss / 100) % 100;
    UInt32 seconds = interval_hhmmss % 100;
    
    ByteBuffer data_vec(data.begin(), data.end());
    
    return IntervalControlManager::instance().start(
        trans, IntervalSpec::interval(hours, minutes, seconds), data_vec);
}

Result<ByteBuffer> exec_cics_retrieve() {
    return IntervalControlManager::instance().retrieve();
}

Result<void> exec_cics_cancel(UInt32 request_id) {
    return IntervalControlManager::instance().cancel(request_id);
}

} // namespace interval
} // namespace cics
