#include "cics/catalog/master_catalog.hpp"
#include <algorithm>
#include <regex>

namespace cics::catalog {

MasterCatalog::MasterCatalog(String name) : name_(std::move(name)) {}

Result<void> MasterCatalog::define_dataset(const CatalogEntry& entry) {
    std::unique_lock lock(mutex_);
    
    if (entries_.contains(entry.name)) {
        return make_error<void>(ErrorCode::DUPLICATE_KEY, "Dataset already exists: " + entry.name);
    }
    
    entries_[entry.name] = entry;
    entries_[entry.name].created = SystemClock::now();
    entries_[entry.name].cataloged = true;
    
    stats_.total_entries++;
    if (entry.type == EntryType::CLUSTER || entry.type == EntryType::DATA || entry.type == EntryType::INDEX) {
        stats_.vsam_entries++;
    } else {
        stats_.nonvsam_entries++;
    }
    if (entry.type == EntryType::GDG_BASE) stats_.gdg_bases++;
    
    return make_success();
}

Result<CatalogEntry> MasterCatalog::get_dataset(const String& name) {
    std::shared_lock lock(mutex_);
    stats_.lookups++;
    
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return make_error<CatalogEntry>(ErrorCode::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    return make_success(it->second);
}

Result<void> MasterCatalog::delete_dataset(const String& name) {
    std::unique_lock lock(mutex_);
    
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return make_error<void>(ErrorCode::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    auto& entry = it->second;
    if (entry.type == EntryType::CLUSTER || entry.type == EntryType::DATA || entry.type == EntryType::INDEX) {
        stats_.vsam_entries--;
    } else {
        stats_.nonvsam_entries--;
    }
    if (entry.type == EntryType::GDG_BASE) stats_.gdg_bases--;
    stats_.total_entries--;
    
    entries_.erase(it);
    stats_.updates++;
    
    return make_success();
}

Result<void> MasterCatalog::rename_dataset(const String& old_name, const String& new_name) {
    std::unique_lock lock(mutex_);
    
    auto it = entries_.find(old_name);
    if (it == entries_.end()) {
        return make_error<void>(ErrorCode::DATASET_NOT_FOUND, "Dataset not found: " + old_name);
    }
    
    if (entries_.contains(new_name)) {
        return make_error<void>(ErrorCode::DUPLICATE_KEY, "Target name exists: " + new_name);
    }
    
    CatalogEntry entry = std::move(it->second);
    entries_.erase(it);
    entry.name = new_name;
    entries_[new_name] = std::move(entry);
    stats_.updates++;
    
    return make_success();
}

std::vector<CatalogEntry> MasterCatalog::list_datasets(const String& pattern) {
    std::shared_lock lock(mutex_);
    std::vector<CatalogEntry> result;
    
    for (const auto& [name, entry] : entries_) {
        if (pattern == "*" || matches_pattern(name, pattern)) {
            result.push_back(entry);
        }
    }
    
    return result;
}

std::vector<CatalogEntry> MasterCatalog::list_by_type(EntryType type) {
    std::shared_lock lock(mutex_);
    std::vector<CatalogEntry> result;
    
    for (const auto& [name, entry] : entries_) {
        if (entry.type == type) result.push_back(entry);
    }
    
    return result;
}

Size MasterCatalog::entry_count() const {
    std::shared_lock lock(mutex_);
    return entries_.size();
}

bool MasterCatalog::matches_pattern(const String& name, const String& pattern) {
    // Convert MVS pattern to regex (* -> .*, % -> .)
    String regex_pattern;
    for (char c : pattern) {
        if (c == '*') regex_pattern += ".*";
        else if (c == '%') regex_pattern += ".";
        else if (c == '.') regex_pattern += "\\.";
        else regex_pattern += c;
    }
    
    try {
        std::regex re(regex_pattern, std::regex::icase);
        return std::regex_match(name, re);
    } catch (...) {
        return false;
    }
}

static SharedPtr<MasterCatalog> g_default_catalog;

SharedPtr<MasterCatalog> MasterCatalogFactory::create(const String& name) {
    return std::make_shared<MasterCatalog>(name);
}

SharedPtr<MasterCatalog> MasterCatalogFactory::get_default() {
    if (!g_default_catalog) {
        g_default_catalog = create("MASTER.CATALOG");
    }
    return g_default_catalog;
}

} // namespace cics::catalog
