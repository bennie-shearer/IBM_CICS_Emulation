#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::security {

enum class SecurityLevel : UInt8 {
    PUBLIC = 0,
    USER = 1,
    OPERATOR = 2,
    ADMIN = 3,
    SYSTEM = 4
};

enum class AccessAction : UInt8 {
    READ = 1,
    WRITE = 2,
    UPDATE = 3,
    DELETE = 4,
    EXECUTE = 5,
    ADMIN = 6
};

struct User {
    FixedString<8> user_id;
    FixedString<8> default_group;
    std::vector<String> groups;
    std::vector<String> roles;
    std::vector<String> permissions;
    bool authenticated = false;
    SystemTimePoint last_access;
    UInt32 failed_attempts = 0;
};

class SecurityContext {
private:
    SecurityLevel level_;
    Optional<User> current_user_;
    String session_id_;
    
public:
    SecurityContext();
    
    Result<void> authenticate(const String& user_id, const String& password);
    Result<void> authorize(const String& resource, AccessAction action);
    void logout();
    
    [[nodiscard]] bool is_authenticated() const;
    [[nodiscard]] bool has_permission(const String& permission) const;
    [[nodiscard]] SecurityLevel level() const { return level_; }
    [[nodiscard]] const Optional<User>& user() const { return current_user_; }
    [[nodiscard]] const String& session_id() const { return session_id_; }
    
    static SecurityContext& current();
    static void set_current(SecurityContext ctx);
};

String generate_session_id();

} // namespace cics::security
