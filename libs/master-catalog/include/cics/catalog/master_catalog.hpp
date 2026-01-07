#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <shared_mutex>

namespace cics::catalog {

enum class DatasetOrganization : UInt8 {
    SEQUENTIAL = 1, PARTITIONED = 2, VSAM_KSDS = 3, VSAM_ESDS = 4,
    VSAM_RRDS = 5, VSAM_LDS = 6, DIRECT = 7, GDG = 8
};

enum class EntryType : UInt8 {
    NONVSAM = 1, CLUSTER = 2, DATA = 3, INDEX = 4, PATH = 5, ALIAS = 6, GDG_BASE = 7, USER_CATALOG = 8
};

struct CatalogEntry {
    String name;
    EntryType type = EntryType::NONVSAM;
    DatasetOrganization organization = DatasetOrganization::SEQUENTIAL;
    String volume;
    UInt64 size_bytes = 0;
    SystemTimePoint created;
    SystemTimePoint last_referenced;
    String owner;
    bool cataloged = true;
    std::unordered_map<String, String> attributes;
};

struct CatalogStatistics {
    AtomicCounter<> total_entries;
    AtomicCounter<> vsam_entries;
    AtomicCounter<> nonvsam_entries;
    AtomicCounter<> gdg_bases;
    AtomicCounter<> lookups;
    AtomicCounter<> updates;
};

class MasterCatalog {
private:
    String name_;
    std::unordered_map<String, CatalogEntry> entries_;
    mutable std::shared_mutex mutex_;
    CatalogStatistics stats_;
    
public:
    explicit MasterCatalog(String name = "MASTER.CATALOG");
    
    Result<void> define_dataset(const CatalogEntry& entry);
    Result<CatalogEntry> get_dataset(const String& name);
    Result<void> delete_dataset(const String& name);
    Result<void> rename_dataset(const String& old_name, const String& new_name);
    
    std::vector<CatalogEntry> list_datasets(const String& pattern = "*");
    std::vector<CatalogEntry> list_by_type(EntryType type);
    
    [[nodiscard]] const String& name() const { return name_; }
    [[nodiscard]] const CatalogStatistics& statistics() const { return stats_; }
    [[nodiscard]] Size entry_count() const;
    
    bool matches_pattern(const String& name, const String& pattern);
};

class MasterCatalogFactory {
public:
    static SharedPtr<MasterCatalog> create(const String& name = "MASTER.CATALOG");
    static SharedPtr<MasterCatalog> get_default();
};

} // namespace cics::catalog
