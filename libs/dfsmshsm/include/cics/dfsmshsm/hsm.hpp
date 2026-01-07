#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::dfsmshsm {

enum class StorageLevel : UInt8 { ML1 = 1, ML2 = 2, TAPE = 3 };

struct MigrationInfo {
    String dataset_name; StorageLevel level; SystemTimePoint migrated_at;
    UInt64 original_size; UInt64 compressed_size; String target_volume;
};

class StorageManager {
    std::unordered_map<String, MigrationInfo> migrations_;
    mutable std::shared_mutex mutex_;
    UInt64 ml1_capacity_ = 10ULL * 1024 * 1024 * 1024;
    UInt64 ml2_capacity_ = 100ULL * 1024 * 1024 * 1024;
    UInt64 ml1_used_ = 0, ml2_used_ = 0;
public:
    static StorageManager& instance();
    
    Result<void> migrate(const String& dataset, StorageLevel target);
    Result<void> recall(const String& dataset);
    Result<MigrationInfo> get_migration_info(const String& dataset) const;
    [[nodiscard]] bool is_migrated(const String& dataset) const;
    [[nodiscard]] double ml1_utilization() const { return double(ml1_used_) / ml1_capacity_ * 100; }
    [[nodiscard]] double ml2_utilization() const { return double(ml2_used_) / ml2_capacity_ * 100; }
};

} // namespace cics::dfsmshsm
