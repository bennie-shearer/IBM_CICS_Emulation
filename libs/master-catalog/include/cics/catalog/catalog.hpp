#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <map>

namespace cics::catalog {

enum class EntryType : UInt8 { DATASET = 1, GDG_BASE = 2, ALIAS = 3, CLUSTER = 4, PATH = 5, USERCATALOG = 6 };
enum class DatasetOrg : UInt8 { PS = 1, PO = 2, DA = 3, VSAM = 4 };

struct CatalogEntry {
    String name; EntryType type = EntryType::DATASET; DatasetOrg organization = DatasetOrg::PS;
    VolumeName volume; String owner; UInt64 allocated_bytes = 0, used_bytes = 0;
    SystemTimePoint created, last_accessed, last_modified, expires;
    std::unordered_map<String, String> attributes;
    
    [[nodiscard]] bool is_expired() const { return expires < SystemClock::now(); }
    [[nodiscard]] double utilization() const { return allocated_bytes > 0 ? double(used_bytes)/allocated_bytes*100 : 0; }
};

class MasterCatalog {
    std::map<String, CatalogEntry> entries_;
    String catalog_name_; Path storage_path_;
    mutable std::shared_mutex mutex_;
    AtomicCounter<> total_entries_;
public:
    explicit MasterCatalog(String name = "MASTER.CATALOG");
    
    Result<void> add_entry(CatalogEntry entry);
    Result<void> remove_entry(const String& name);
    Result<CatalogEntry*> find_entry(const String& name);
    Result<const CatalogEntry*> find_entry(const String& name) const;
    Result<std::vector<CatalogEntry>> search(StringView pattern) const;
    [[nodiscard]] Size entry_count() const { return total_entries_; }
    [[nodiscard]] const String& name() const { return catalog_name_; }
    
    Result<void> save(const Path& path) const;
    Result<void> load(const Path& path);
    void clear();
};

MasterCatalog& default_catalog();

} // namespace cics::catalog
