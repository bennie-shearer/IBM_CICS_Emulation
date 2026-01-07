#include "cics/security/security_context.hpp"
#include <random>
#include <iomanip>
#include <sstream>

namespace cics::security {

String generate_session_id() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<UInt64> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << dist(gen);
    return oss.str();
}

SecurityContext::SecurityContext() : level_(SecurityLevel::PUBLIC) {}

Result<void> SecurityContext::authenticate(const String& user_id, [[maybe_unused]] const String& password) {
    if (user_id.empty()) return make_error<void>(ErrorCode::INVALID_ARGUMENT, "User ID required");
    
    // Basic authentication (in production, verify against security database)
    current_user_ = User{};
    current_user_->user_id = FixedString<8>(user_id);
    current_user_->authenticated = true;
    current_user_->last_access = SystemClock::now();
    
    session_id_ = generate_session_id();
    level_ = SecurityLevel::USER;
    
    return make_success();
}

Result<void> SecurityContext::authorize(const String& resource, [[maybe_unused]] AccessAction action) {
    if (!current_user_ || !current_user_->authenticated) {
        return make_error<void>(ErrorCode::AUTHENTICATION_FAILED, "Not authenticated");
    }
    
    // Check authorization (simplified - in production check against RACF-style rules)
    if (level_ >= SecurityLevel::USER) {
        return make_success();
    }
    
    return make_error<void>(ErrorCode::AUTHORIZATION_FAILED, "Access denied to " + resource);
}

void SecurityContext::logout() {
    current_user_.reset();
    session_id_.clear();
    level_ = SecurityLevel::PUBLIC;
}

bool SecurityContext::is_authenticated() const {
    return current_user_.has_value() && current_user_->authenticated;
}

bool SecurityContext::has_permission(const String& permission) const {
    if (!current_user_) return false;
    return std::find(current_user_->permissions.begin(), 
                     current_user_->permissions.end(), 
                     permission) != current_user_->permissions.end();
}

static thread_local Optional<SecurityContext> tl_context;

SecurityContext& SecurityContext::current() {
    if (!tl_context) tl_context.emplace();
    return *tl_context;
}

void SecurityContext::set_current(SecurityContext ctx) {
    tl_context = std::move(ctx);
}

} // namespace cics::security
