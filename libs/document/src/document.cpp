// =============================================================================
// CICS Emulation - Document Handler Implementation
// =============================================================================

#include <cics/document/document.hpp>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <cstring>

namespace cics {
namespace document {

// =============================================================================
// Utility Functions
// =============================================================================

String document_type_to_string(DocumentType type) {
    switch (type) {
        case DocumentType::TEXT: return "TEXT";
        case DocumentType::HTML: return "HTML";
        case DocumentType::XML: return "XML";
        case DocumentType::JSON: return "JSON";
        case DocumentType::BINARY: return "BINARY";
        default: return "UNKNOWN";
    }
}

String insert_position_to_string(InsertPosition pos) {
    switch (pos) {
        case InsertPosition::ATEND: return "ATEND";
        case InsertPosition::ATSTART: return "ATSTART";
        case InsertPosition::BEFORE: return "BEFORE";
        case InsertPosition::AFTER: return "AFTER";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// SymbolTable Implementation
// =============================================================================

void SymbolTable::set(StringView name, StringView value) {
    std::lock_guard<std::mutex> lock(mutex_);
    symbols_[String(name)] = String(value);
}

void SymbolTable::set(StringView name, Int64 value) {
    set(name, std::to_string(value));
}

void SymbolTable::set(StringView name, double value) {
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    set(name, oss.str());
}

Optional<String> SymbolTable::get(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = symbols_.find(String(name));
    if (it != symbols_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SymbolTable::has(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symbols_.find(String(name)) != symbols_.end();
}

void SymbolTable::remove(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    symbols_.erase(String(name));
}

void SymbolTable::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    symbols_.clear();
}

std::vector<String> SymbolTable::list_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> names;
    names.reserve(symbols_.size());
    for (const auto& [name, value] : symbols_) {
        names.push_back(name);
    }
    return names;
}

UInt32 SymbolTable::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(symbols_.size());
}

// =============================================================================
// Document Implementation
// =============================================================================

Document::Document(StringView token, DocumentType type)
    : token_(token)
    , type_(type)
    , created_(std::chrono::steady_clock::now())
    , modified_(created_)
{
}

UInt32 Document::length() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<UInt32>(content_.size());
}

bool Document::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return content_.empty();
}

Result<void> Document::set(StringView content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (content.size() > MAX_DOCUMENT_SIZE) {
        return make_error<void>(ErrorCode::LENGERR, "Document content exceeds maximum size");
    }
    
    content_ = String(content);
    modified_ = std::chrono::steady_clock::now();
    return make_success();
}

Result<void> Document::set(const ByteBuffer& content) {
    return set(StringView(reinterpret_cast<const char*>(content.data()), content.size()));
}

Result<void> Document::insert(StringView text, InsertPosition pos) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (content_.size() + text.size() > MAX_DOCUMENT_SIZE) {
        return make_error<void>(ErrorCode::LENGERR, "Insert would exceed maximum document size");
    }
    
    switch (pos) {
        case InsertPosition::ATSTART:
            content_.insert(0, text);
            break;
        case InsertPosition::ATEND:
        default:
            content_.append(text);
            break;
    }
    
    modified_ = std::chrono::steady_clock::now();
    return make_success();
}

Result<void> Document::insert(StringView text, StringView bookmark, InsertPosition pos) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = bookmarks_.find(String(bookmark));
    if (it == bookmarks_.end()) {
        return make_error<void>(ErrorCode::NOTFND, "Bookmark not found: " + String(bookmark));
    }
    
    UInt32 position = it->second;
    if (position > content_.size()) {
        position = static_cast<UInt32>(content_.size());
    }
    
    if (pos == InsertPosition::AFTER) {
        // Find end of current line or use position
        position = std::min(position + 1, static_cast<UInt32>(content_.size()));
    }
    
    content_.insert(position, text);
    
    // Update bookmark positions after insertion point
    for (auto& [name, bm_pos] : bookmarks_) {
        if (bm_pos >= position) {
            bm_pos += static_cast<UInt32>(text.size());
        }
    }
    
    modified_ = std::chrono::steady_clock::now();
    return make_success();
}

Result<void> Document::insert_template(StringView template_name) {
    auto& mgr = DocumentManager::instance();
    auto tmpl = mgr.templates().get_template(template_name);
    if (tmpl.is_error()) {
        return make_error<void>(tmpl.error().code, tmpl.error().message);
    }
    return insert(tmpl.value());
}

Result<void> Document::add_bookmark(StringView name) {
    return add_bookmark(name, static_cast<UInt32>(content_.size()));
}

Result<void> Document::add_bookmark(StringView name, UInt32 position) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (name.size() > MAX_SYMBOL_NAME) {
        return make_error<void>(ErrorCode::INVREQ, "Bookmark name too long");
    }
    
    bookmarks_[String(name)] = position;
    return make_success();
}

bool Document::has_bookmark(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bookmarks_.find(String(name)) != bookmarks_.end();
}

Optional<UInt32> Document::get_bookmark_position(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = bookmarks_.find(String(name));
    if (it != bookmarks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Document::set_symbol(StringView name, StringView value) {
    symbols_.set(name, value);
    modified_ = std::chrono::steady_clock::now();
}

void Document::set_symbol(StringView name, Int64 value) {
    symbols_.set(name, value);
    modified_ = std::chrono::steady_clock::now();
}

Optional<String> Document::get_symbol(StringView name) const {
    return symbols_.get(name);
}

Result<String> Document::retrieve() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return make_success(content_);
}

Result<ByteBuffer> Document::retrieve_binary() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ByteBuffer buffer(content_.begin(), content_.end());
    return make_success(buffer);
}

Result<String> Document::retrieve_with_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return make_success(substitute_symbols(content_));
}

String Document::substitute_symbols(const String& content) const {
    String result = content;
    
    // Pattern: &symbol; or ${symbol}
    for (const auto& name : symbols_.list_symbols()) {
        auto value = symbols_.get(name);
        if (value.has_value()) {
            // Replace &name;
            String pattern1 = "&" + name + ";";
            size_t pos = 0;
            while ((pos = result.find(pattern1, pos)) != String::npos) {
                result.replace(pos, pattern1.size(), value.value());
                pos += value.value().size();
            }
            
            // Replace ${name}
            String pattern2 = "${" + name + "}";
            pos = 0;
            while ((pos = result.find(pattern2, pos)) != String::npos) {
                result.replace(pos, pattern2.size(), value.value());
                pos += value.value().size();
            }
        }
    }
    
    return result;
}

DocumentInfo Document::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DocumentInfo info;
    info.token = token_;
    info.type = type_;
    info.length = static_cast<UInt32>(content_.size());
    info.created = created_;
    info.modified = modified_;
    info.symbol_count = symbols_.count();
    info.bookmark_count = static_cast<UInt32>(bookmarks_.size());
    return info;
}

// =============================================================================
// TemplateRegistry Implementation
// =============================================================================

Result<void> TemplateRegistry::register_template(StringView name, StringView content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (name.size() > MAX_TEMPLATE_NAME) {
        return make_error<void>(ErrorCode::INVREQ, "Template name too long");
    }
    
    templates_[String(name)] = String(content);
    return make_success();
}

Result<void> TemplateRegistry::register_template_file(StringView name, StringView filepath) {
    (void)name;      // Reserved for future implementation
    (void)filepath;  // Reserved for future implementation
    // Would read from file - simplified for now
    return make_error<void>(ErrorCode::NOT_SUPPORTED, "File templates not yet implemented");
}

Result<String> TemplateRegistry::get_template(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = templates_.find(String(name));
    if (it == templates_.end()) {
        return make_error<String>(ErrorCode::NOTFND, "Template not found: " + String(name));
    }
    
    return make_success(it->second);
}

bool TemplateRegistry::has_template(StringView name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return templates_.find(String(name)) != templates_.end();
}

void TemplateRegistry::remove_template(StringView name) {
    std::lock_guard<std::mutex> lock(mutex_);
    templates_.erase(String(name));
}

std::vector<String> TemplateRegistry::list_templates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<String> names;
    names.reserve(templates_.size());
    for (const auto& [name, content] : templates_) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// DocumentManager Implementation
// =============================================================================

DocumentManager& DocumentManager::instance() {
    static DocumentManager instance;
    return instance;
}

void DocumentManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    
    documents_.clear();
    token_counter_ = 0;
    reset_stats();
    initialized_ = true;
}

void DocumentManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    documents_.clear();
    initialized_ = false;
}

String DocumentManager::generate_token() {
    ++token_counter_;
    std::ostringstream oss;
    oss << "DOC" << std::setfill('0') << std::setw(12) << token_counter_;
    return oss.str();
}

Result<String> DocumentManager::create(DocumentType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return make_error<String>(ErrorCode::NOT_INITIALIZED, "DocumentManager not initialized");
    }
    
    String token = generate_token();
    auto doc = std::make_unique<Document>(token, type);
    documents_[token] = std::move(doc);
    
    ++stats_.documents_created;
    return make_success(token);
}

Result<Document*> DocumentManager::get(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(String(token));
    if (it == documents_.end()) {
        return make_error<Document*>(ErrorCode::NOTFND, "Document not found: " + String(token));
    }
    
    return make_success(it->second.get());
}

Result<void> DocumentManager::delete_document(StringView token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(String(token));
    if (it == documents_.end()) {
        return make_error<void>(ErrorCode::NOTFND, "Document not found");
    }
    
    documents_.erase(it);
    ++stats_.documents_deleted;
    return make_success();
}

bool DocumentManager::exists(StringView token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return documents_.find(String(token)) != documents_.end();
}

Result<void> DocumentManager::set(StringView token, StringView content) {
    auto doc_result = get(token);
    if (doc_result.is_error()) {
        return make_error<void>(doc_result.error().code, doc_result.error().message);
    }
    
    auto result = doc_result.value()->set(content);
    if (result.is_success()) {
        stats_.bytes_written += content.size();
    }
    return result;
}

Result<void> DocumentManager::insert(StringView token, StringView text, InsertPosition pos) {
    auto doc_result = get(token);
    if (doc_result.is_error()) {
        return make_error<void>(doc_result.error().code, doc_result.error().message);
    }
    
    auto result = doc_result.value()->insert(text, pos);
    if (result.is_success()) {
        ++stats_.inserts_executed;
        stats_.bytes_written += text.size();
    }
    return result;
}

Result<String> DocumentManager::retrieve(StringView token) {
    auto doc_result = get(token);
    if (doc_result.is_error()) {
        return make_error<String>(doc_result.error().code, doc_result.error().message);
    }
    
    auto result = doc_result.value()->retrieve_with_symbols();
    if (result.is_success()) {
        ++stats_.retrieves_executed;
        stats_.bytes_read += result.value().size();
    }
    return result;
}

Result<void> DocumentManager::set_symbol(StringView token, StringView name, StringView value) {
    auto doc_result = get(token);
    if (doc_result.is_error()) {
        return make_error<void>(doc_result.error().code, doc_result.error().message);
    }
    
    doc_result.value()->set_symbol(name, value);
    ++stats_.symbols_substituted;
    return make_success();
}

DocumentStats DocumentManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void DocumentManager::reset_stats() {
    stats_ = DocumentStats{};
}

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

Result<String> exec_cics_document_create() {
    return DocumentManager::instance().create();
}

Result<String> exec_cics_document_create(DocumentType type) {
    return DocumentManager::instance().create(type);
}

Result<void> exec_cics_document_set(StringView token, StringView content) {
    return DocumentManager::instance().set(token, content);
}

Result<void> exec_cics_document_set_symbol(StringView token, StringView name, StringView value) {
    return DocumentManager::instance().set_symbol(token, name, value);
}

Result<void> exec_cics_document_insert(StringView token, StringView text) {
    return DocumentManager::instance().insert(token, text);
}

Result<void> exec_cics_document_insert(StringView token, StringView text, InsertPosition pos) {
    return DocumentManager::instance().insert(token, text, pos);
}

Result<void> exec_cics_document_insert_template(StringView token, StringView template_name) {
    auto doc_result = DocumentManager::instance().get(token);
    if (doc_result.is_error()) {
        return make_error<void>(doc_result.error().code, doc_result.error().message);
    }
    return doc_result.value()->insert_template(template_name);
}

Result<String> exec_cics_document_retrieve(StringView token) {
    return DocumentManager::instance().retrieve(token);
}

Result<UInt32> exec_cics_document_retrieve_into(StringView token, void* buffer, UInt32 max_len) {
    auto result = DocumentManager::instance().retrieve(token);
    if (result.is_error()) {
        return make_error<UInt32>(result.error().code, result.error().message);
    }
    
    UInt32 copy_len = std::min(max_len, static_cast<UInt32>(result.value().size()));
    if (copy_len > 0 && buffer) {
        std::memcpy(buffer, result.value().data(), copy_len);
    }
    return make_success(copy_len);
}

Result<void> exec_cics_document_delete(StringView token) {
    return DocumentManager::instance().delete_document(token);
}

} // namespace document
} // namespace cics
