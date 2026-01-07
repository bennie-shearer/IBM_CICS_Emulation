#include "cics/dfsmshsm/dfsmshsm_types.hpp"

namespace cics::dfsmshsm {

Result<void> StorageManager::migrate(const String& dataset_name, StorageLevel target) {
    std::unique_lock lock(mutex_);
    
    auto& ds = datasets_[dataset_name];
    ds.name = dataset_name;
    
    MigrationStatus new_status;
    switch (target) {
        case StorageLevel::ML1: new_status = MigrationStatus::MIGRATED_ML1; break;
        case StorageLevel::ML2: new_status = MigrationStatus::MIGRATED_ML2; break;
        case StorageLevel::TAPE: new_status = MigrationStatus::MIGRATED_TAPE; break;
        default: return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Invalid target level");
    }
    
    ds.status = new_status;
    ds.level = target;
    ds.migrated_date = SystemClock::now();
    
    stats_.migrations++;
    stats_.bytes_migrated += ds.size_bytes;
    
    switch (target) {
        case StorageLevel::ML1: stats_.migrated_ml1++; break;
        case StorageLevel::ML2: stats_.migrated_ml2++; break;
        case StorageLevel::TAPE: stats_.migrated_tape++; break;
    }
    
    return make_success();
}

Result<void> StorageManager::recall(const String& dataset_name) {
    std::unique_lock lock(mutex_);
    
    auto it = datasets_.find(dataset_name);
    if (it == datasets_.end()) {
        return make_error<void>(ErrorCode::DATASET_NOT_FOUND, "Dataset not found");
    }
    
    auto& ds = it->second;
    if (ds.status == MigrationStatus::RESIDENT) {
        return make_error<void>(ErrorCode::INVALID_STATE, "Dataset not migrated");
    }
    
    switch (ds.status) {
        case MigrationStatus::MIGRATED_ML1: stats_.migrated_ml1--; break;
        case MigrationStatus::MIGRATED_ML2: stats_.migrated_ml2--; break;
        case MigrationStatus::MIGRATED_TAPE: stats_.migrated_tape--; break;
        default: break;
    }
    
    ds.status = MigrationStatus::RESIDENT;
    ds.level = StorageLevel::ML1;
    ds.last_access = SystemClock::now();
    
    stats_.recalls++;
    stats_.bytes_recalled += ds.size_bytes;
    
    return make_success();
}

Result<HsmDataset> StorageManager::get_status(const String& dataset_name) {
    std::shared_lock lock(mutex_);
    
    auto it = datasets_.find(dataset_name);
    if (it == datasets_.end()) {
        return make_error<HsmDataset>(ErrorCode::DATASET_NOT_FOUND, "Dataset not found");
    }
    
    return make_success(it->second);
}

std::vector<HsmDataset> StorageManager::list_migrated() {
    std::shared_lock lock(mutex_);
    std::vector<HsmDataset> result;
    
    for (const auto& [name, ds] : datasets_) {
        if (ds.status != MigrationStatus::RESIDENT) {
            result.push_back(ds);
        }
    }
    
    return result;
}

} // namespace cics::dfsmshsm
