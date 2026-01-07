#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::dfsmshsm {

enum class StorageLevel : UInt8 { ML1 = 1, ML2 = 2, TAPE = 3 };
enum class MigrationStatus : UInt8 { RESIDENT = 0, MIGRATED_ML1 = 1, MIGRATED_ML2 = 2, MIGRATED_TAPE = 3 };

struct HsmDataset {
    String name;
    StorageLevel level = StorageLevel::ML1;
    MigrationStatus status = MigrationStatus::RESIDENT;
    UInt64 size_bytes = 0;
    SystemTimePoint last_access;
    SystemTimePoint migrated_date;
    UInt32 days_since_reference = 0;
    String volume;
    bool recall_pending = false;
};

struct HsmStatistics {
    AtomicCounter<> total_datasets;
    AtomicCounter<> migrated_ml1;
    AtomicCounter<> migrated_ml2;
    AtomicCounter<> migrated_tape;
    AtomicCounter<UInt64> bytes_migrated;
    AtomicCounter<UInt64> bytes_recalled;
    AtomicCounter<> migrations;
    AtomicCounter<> recalls;
    
    [[nodiscard]] String to_string() const {
        return std::format("Datasets: {}, ML1: {}, ML2: {}, Tape: {}", 
            total_datasets.get(), migrated_ml1.get(), migrated_ml2.get(), migrated_tape.get());
    }
};

[[nodiscard]] constexpr StringView to_string(StorageLevel level) {
    switch (level) {
        case StorageLevel::ML1: return "ML1";
        case StorageLevel::ML2: return "ML2";
        case StorageLevel::TAPE: return "TAPE";
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr StringView to_string(MigrationStatus status) {
    switch (status) {
        case MigrationStatus::RESIDENT: return "RESIDENT";
        case MigrationStatus::MIGRATED_ML1: return "ML1";
        case MigrationStatus::MIGRATED_ML2: return "ML2";
        case MigrationStatus::MIGRATED_TAPE: return "TAPE";
    }
    return "UNKNOWN";
}

class StorageManager {
private:
    std::unordered_map<String, HsmDataset> datasets_;
    mutable std::shared_mutex mutex_;
    HsmStatistics stats_;
    
public:
    Result<void> migrate(const String& dataset_name, StorageLevel target);
    Result<void> recall(const String& dataset_name);
    Result<HsmDataset> get_status(const String& dataset_name);
    std::vector<HsmDataset> list_migrated();
    [[nodiscard]] const HsmStatistics& statistics() const { return stats_; }
};

} // namespace cics::dfsmshsm
