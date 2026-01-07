#include "cics/vsam/vsam_types.hpp"
#include <map>

namespace cics::vsam {

class KsdsFile : public IVsamFile {
private:
    VsamDefinition def_;
    VsamStatistics stats_;
    std::map<VsamKey, VsamRecord> records_;  // Sorted by key
    std::unordered_map<String, BrowseContext> browse_contexts_;
    mutable std::shared_mutex mutex_;
    AccessMode access_mode_ = AccessMode::INPUT;
    ProcessingMode proc_mode_ = ProcessingMode::DYNAMIC;
    bool open_ = false;
    RBA next_rba_ = 0;
    
public:
    explicit KsdsFile(VsamDefinition def) : def_(std::move(def)) {
        stats_.allocated_bytes = def_.ci_size * def_.ca_size * 100;
    }
    
    Result<void> open(AccessMode mode, ProcessingMode proc) override {
        std::unique_lock lock(mutex_);
        if (open_) return make_error<void>(ErrorCode::VSAM_ERROR, "Already open");
        access_mode_ = mode;
        proc_mode_ = proc;
        open_ = true;
        return make_success();
    }
    
    Result<void> close() override {
        std::unique_lock lock(mutex_);
        browse_contexts_.clear();
        open_ = false;
        return make_success();
    }
    
    bool is_open() const override { return open_; }
    
    Result<VsamRecord> read(const VsamKey& key) override {
        auto start = Clock::now();
        std::shared_lock lock(mutex_);
        
        if (!open_) return make_error<VsamRecord>(ErrorCode::VSAM_FILE_NOT_OPEN, "File not open");
        
        auto it = records_.find(key);
        if (it == records_.end()) {
            return make_error<VsamRecord>(ErrorCode::VSAM_RECORD_NOT_FOUND, "Record not found");
        }
        
        stats_.record_read(Clock::now() - start);
        return make_success(it->second);
    }
    
    Result<VsamRecord> read_by_rba(RBA rba) override {
        std::shared_lock lock(mutex_);
        for (const auto& [k, rec] : records_) {
            if (rec.rba() == rba) return make_success(rec);
        }
        return make_error<VsamRecord>(ErrorCode::VSAM_RBA_NOT_FOUND, "RBA not found");
    }
    
    Result<VsamRecord> read_by_rrn(RRN) override {
        return make_error<VsamRecord>(ErrorCode::VSAM_INVALID_REQUEST, "RRN not valid for KSDS");
    }
    
    Result<void> write(const VsamRecord& record) override {
        auto start = Clock::now();
        std::unique_lock lock(mutex_);
        
        if (!open_) return make_error<void>(ErrorCode::VSAM_FILE_NOT_OPEN, "File not open");
        if (access_mode_ == AccessMode::INPUT) {
            return make_error<void>(ErrorCode::VSAM_INVALID_REQUEST, "File open for input");
        }
        
        if (records_.contains(record.key())) {
            return make_error<void>(ErrorCode::VSAM_DUPLICATE_KEY, "Duplicate key");
        }
        
        VsamRecord new_rec = record;
        VsamAddress addr;
        addr.rba = next_rba_;
        next_rba_ += record.length() + def_.key_length;
        new_rec.set_address(addr);
        
        records_[record.key()] = std::move(new_rec);
        stats_.record_count++;
        stats_.record_write(Clock::now() - start, record.length());
        
        return make_success();
    }
    
    Result<void> update(const VsamRecord& record) override {
        auto start = Clock::now();
        std::unique_lock lock(mutex_);
        
        if (!open_) return make_error<void>(ErrorCode::VSAM_FILE_NOT_OPEN, "File not open");
        
        auto it = records_.find(record.key());
        if (it == records_.end()) {
            return make_error<void>(ErrorCode::VSAM_RECORD_NOT_FOUND, "Record not found");
        }
        
        it->second.set_data(record.span());
        stats_.record_update(Clock::now() - start);
        
        return make_success();
    }
    
    Result<void> erase(const VsamKey& key) override {
        std::unique_lock lock(mutex_);
        
        if (!open_) return make_error<void>(ErrorCode::VSAM_FILE_NOT_OPEN, "File not open");
        
        auto it = records_.find(key);
        if (it == records_.end()) {
            return make_error<void>(ErrorCode::VSAM_RECORD_NOT_FOUND, "Record not found");
        }
        
        records_.erase(it);
        stats_.record_count--;
        stats_.record_delete();
        
        return make_success();
    }
    
    Result<String> start_browse(const VsamKey& key, bool gteq, bool backward) override {
        std::unique_lock lock(mutex_);
        
        BrowseContext ctx;
        ctx.set_mode(proc_mode_);
        ctx.set_backward(backward);
        
        auto it = gteq ? records_.lower_bound(key) : records_.find(key);
        if (it != records_.end()) {
            ctx.set_current(it->second.key(), it->second.address());
        } else {
            ctx.set_at_end(true);
        }
        
        String id = ctx.id();
        browse_contexts_[id] = std::move(ctx);
        
        return make_success(id);
    }
    
    Result<VsamRecord> read_next(const String& browse_id) override {
        std::shared_lock lock(mutex_);
        
        auto ctx_it = browse_contexts_.find(browse_id);
        if (ctx_it == browse_contexts_.end()) {
            return make_error<VsamRecord>(ErrorCode::VSAM_ERROR, "Invalid browse ID");
        }
        
        auto& ctx = ctx_it->second;
        if (ctx.at_end()) {
            return make_error<VsamRecord>(ErrorCode::VSAM_END_OF_FILE, "End of file");
        }
        
        auto it = records_.upper_bound(ctx.current_key());
        if (it == records_.end()) {
            ctx.set_at_end(true);
            return make_error<VsamRecord>(ErrorCode::VSAM_END_OF_FILE, "End of file");
        }
        
        ctx.set_current(it->second.key(), it->second.address());
        ctx.increment_records();
        stats_.browses++;
        
        return make_success(it->second);
    }
    
    Result<VsamRecord> read_prev(const String& browse_id) override {
        std::shared_lock lock(mutex_);
        
        auto ctx_it = browse_contexts_.find(browse_id);
        if (ctx_it == browse_contexts_.end()) {
            return make_error<VsamRecord>(ErrorCode::VSAM_ERROR, "Invalid browse ID");
        }
        
        auto& ctx = ctx_it->second;
        auto it = records_.find(ctx.current_key());
        if (it == records_.begin()) {
            ctx.set_at_start(true);
            return make_error<VsamRecord>(ErrorCode::VSAM_END_OF_FILE, "Beginning of file");
        }
        
        --it;
        ctx.set_current(it->second.key(), it->second.address());
        ctx.increment_records();
        
        return make_success(it->second);
    }
    
    Result<void> end_browse(const String& browse_id) override {
        std::unique_lock lock(mutex_);
        browse_contexts_.erase(browse_id);
        return make_success();
    }
    
    Result<void> reset_browse(const String& browse_id, const VsamKey& key) override {
        std::unique_lock lock(mutex_);
        
        auto ctx_it = browse_contexts_.find(browse_id);
        if (ctx_it == browse_contexts_.end()) {
            return make_error<void>(ErrorCode::VSAM_ERROR, "Invalid browse ID");
        }
        
        auto it = records_.lower_bound(key);
        if (it != records_.end()) {
            ctx_it->second.set_current(it->second.key(), it->second.address());
            ctx_it->second.set_at_end(false);
        }
        
        return make_success();
    }
    
    const VsamDefinition& definition() const override { return def_; }
    const VsamStatistics& statistics() const override { return stats_; }
    VsamType type() const override { return VsamType::KSDS; }
    UInt64 record_count() const override { return stats_.record_count.get(); }
};

UniquePtr<IVsamFile> create_vsam_file(const VsamDefinition& def, const Path&) {
    switch (def.type) {
        case VsamType::KSDS:
            return make_unique<KsdsFile>(def);
        default:
            return nullptr;
    }
}

} // namespace cics::vsam
