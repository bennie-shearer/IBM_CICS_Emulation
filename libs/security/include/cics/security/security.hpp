#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <unordered_set>

namespace cics::security {

enum class Permission : UInt32 {
    READ = 0x0001, WRITE = 0x0002, EXECUTE = 0x0004, DELETE_ = 0x0008,
    ADMIN = 0x0010, CONTROL = 0x0020, ALTER = 0x0040, UPDATE = 0x0080
};

class SecurityContext {
    UserId user_id_; GroupId group_id_; String session_id_;
    UInt32 permissions_ = 0; SystemTimePoint created_, expires_;
    std::unordered_set<String> roles_;
    bool active_ = false;
public:
    SecurityContext() = default;
    SecurityContext(UserId user, GroupId group);
    [[nodiscard]] const UserId& user_id() const { return user_id_; }
    [[nodiscard]] const GroupId& group_id() const { return group_id_; }
    [[nodiscard]] const String& session_id() const { return session_id_; }
    [[nodiscard]] bool is_active() const { return active_; }
    [[nodiscard]] bool is_expired() const;
    [[nodiscard]] bool has_permission(Permission perm) const;
    [[nodiscard]] bool has_role(StringView role) const;
    void grant_permission(Permission perm);
    void revoke_permission(Permission perm);
    void add_role(String role);
    void remove_role(const String& role);
    void activate();
    void deactivate();
    void extend_session(Duration duration);
};

class Authenticator {
public:
    virtual ~Authenticator() = default;
    virtual Result<SecurityContext> authenticate(StringView user, StringView password) = 0;
    virtual Result<void> validate_session(const SecurityContext& ctx) = 0;
    virtual void logout(SecurityContext& ctx) = 0;
};

class SimpleAuthenticator : public Authenticator {
    std::unordered_map<String, String> credentials_;
public:
    void add_user(String user, String password);
    Result<SecurityContext> authenticate(StringView user, StringView password) override;
    Result<void> validate_session(const SecurityContext& ctx) override;
    void logout(SecurityContext& ctx) override;
};

class SecurityManager {
    UniquePtr<Authenticator> authenticator_;
    std::unordered_map<String, SecurityContext> sessions_;
    mutable std::shared_mutex mutex_;
    SecurityManager();
public:
    static SecurityManager& instance();
    void set_authenticator(UniquePtr<Authenticator> auth);
    Result<String> login(StringView user, StringView password);
    Result<SecurityContext*> get_session(const String& session_id);
    Result<void> logout(const String& session_id);
    Result<void> check_permission(const String& session_id, Permission perm);
};

} // namespace cics::security
