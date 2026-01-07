#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::security {

// Simple XOR encryption (for demonstration - use OpenSSL in production)
ByteBuffer simple_encrypt(ConstByteSpan data, ConstByteSpan key) {
    ByteBuffer result(data.begin(), data.end());
    for (Size i = 0; i < result.size(); ++i) {
        result[i] ^= key[i % key.size()];
    }
    return result;
}

ByteBuffer simple_decrypt(ConstByteSpan data, ConstByteSpan key) {
    return simple_encrypt(data, key);  // XOR is symmetric
}

String hash_password(StringView password, StringView salt) {
    // Simple hash (use bcrypt/argon2 in production)
    String combined = String(password) + String(salt);
    UInt64 hash = fnv1a_hash(ConstByteSpan(
        reinterpret_cast<const Byte*>(combined.data()), combined.size()));
    return to_hex_string(ConstByteSpan(
        reinterpret_cast<const Byte*>(&hash), sizeof(hash)));
}

} // namespace cics::security
