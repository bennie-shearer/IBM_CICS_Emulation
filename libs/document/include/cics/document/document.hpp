// =============================================================================
// CICS Emulation - Document Handler
// =============================================================================
// Provides DOCUMENT CREATE, INSERT, SET, RETRIEVE functionality.
// Implements document composition services for dynamic content generation.
//
// Copyright (c) 2025 Bennie Shearer. All rights reserved.
// =============================================================================

#ifndef CICS_DOCUMENT_HPP
#define CICS_DOCUMENT_HPP

#include <cics/common/types.hpp>
#include <cics/common/error.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>

namespace cics {
namespace document {

// =============================================================================
// Constants
// =============================================================================

constexpr UInt32 MAX_DOCUMENT_SIZE = 16 * 1024 * 1024;  // 16 MB
constexpr UInt32 MAX_SYMBOL_NAME = 32;
constexpr UInt32 MAX_TEMPLATE_NAME = 48;

// =============================================================================
// Enumerations
// =============================================================================

enum class DocumentType {
    TEXT,           // Plain text document
    HTML,           // HTML document
    XML,            // XML document
    JSON,           // JSON document
    BINARY          // Binary data
};

enum class InsertPosition {
    ATEND,          // Insert at end (default)
    ATSTART,        // Insert at start
    BEFORE,         // Insert before bookmark
    AFTER           // Insert after bookmark
};

// =============================================================================
// Symbol Table
// =============================================================================

class SymbolTable {
public:
    void set(StringView name, StringView value);
    void set(StringView name, Int64 value);
    void set(StringView name, double value);
    
    [[nodiscard]] Optional<String> get(StringView name) const;
    [[nodiscard]] bool has(StringView name) const;
    void remove(StringView name);
    void clear();
    
    [[nodiscard]] std::vector<String> list_symbols() const;
    [[nodiscard]] UInt32 count() const;
    
private:
    std::unordered_map<String, String> symbols_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Document Class
// =============================================================================

struct DocumentInfo {
    String token;
    DocumentType type;
    UInt32 length;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point modified;
    UInt32 symbol_count;
    UInt32 bookmark_count;
};

class Document {
public:
    explicit Document(StringView token, DocumentType type = DocumentType::TEXT);
    
    // Properties
    [[nodiscard]] const String& token() const { return token_; }
    [[nodiscard]] DocumentType type() const { return type_; }
    [[nodiscard]] UInt32 length() const;
    [[nodiscard]] bool empty() const;
    
    // Content operations
    Result<void> set(StringView content);
    Result<void> set(const ByteBuffer& content);
    Result<void> insert(StringView text, InsertPosition pos = InsertPosition::ATEND);
    Result<void> insert(StringView text, StringView bookmark, InsertPosition pos);
    Result<void> insert_template(StringView template_name);
    
    // Bookmark operations
    Result<void> add_bookmark(StringView name);
    Result<void> add_bookmark(StringView name, UInt32 position);
    [[nodiscard]] bool has_bookmark(StringView name) const;
    [[nodiscard]] Optional<UInt32> get_bookmark_position(StringView name) const;
    
    // Symbol operations
    void set_symbol(StringView name, StringView value);
    void set_symbol(StringView name, Int64 value);
    [[nodiscard]] Optional<String> get_symbol(StringView name) const;
    
    // Retrieve content
    [[nodiscard]] Result<String> retrieve() const;
    [[nodiscard]] Result<ByteBuffer> retrieve_binary() const;
    [[nodiscard]] Result<String> retrieve_with_symbols() const;
    
    // Information
    [[nodiscard]] DocumentInfo get_info() const;
    [[nodiscard]] SymbolTable& symbols() { return symbols_; }
    [[nodiscard]] const SymbolTable& symbols() const { return symbols_; }
    
private:
    String substitute_symbols(const String& content) const;
    
    String token_;
    DocumentType type_;
    String content_;
    SymbolTable symbols_;
    std::unordered_map<String, UInt32> bookmarks_;
    std::chrono::steady_clock::time_point created_;
    std::chrono::steady_clock::time_point modified_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Template Registry
// =============================================================================

class TemplateRegistry {
public:
    Result<void> register_template(StringView name, StringView content);
    Result<void> register_template_file(StringView name, StringView filepath);
    [[nodiscard]] Result<String> get_template(StringView name) const;
    [[nodiscard]] bool has_template(StringView name) const;
    void remove_template(StringView name);
    [[nodiscard]] std::vector<String> list_templates() const;
    
private:
    std::unordered_map<String, String> templates_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Document Statistics
// =============================================================================

struct DocumentStats {
    UInt64 documents_created{0};
    UInt64 documents_deleted{0};
    UInt64 inserts_executed{0};
    UInt64 retrieves_executed{0};
    UInt64 bytes_written{0};
    UInt64 bytes_read{0};
    UInt64 symbols_substituted{0};
};

// =============================================================================
// Document Manager
// =============================================================================

class DocumentManager {
public:
    static DocumentManager& instance();
    
    // Lifecycle
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }
    
    // Document operations
    Result<String> create(DocumentType type = DocumentType::TEXT);
    Result<Document*> get(StringView token);
    Result<void> delete_document(StringView token);
    [[nodiscard]] bool exists(StringView token) const;
    
    // Content operations
    Result<void> set(StringView token, StringView content);
    Result<void> insert(StringView token, StringView text, InsertPosition pos = InsertPosition::ATEND);
    Result<String> retrieve(StringView token);
    
    // Symbol operations
    Result<void> set_symbol(StringView token, StringView name, StringView value);
    
    // Template registry
    [[nodiscard]] TemplateRegistry& templates() { return templates_; }
    
    // Statistics
    [[nodiscard]] DocumentStats get_stats() const;
    void reset_stats();
    
private:
    DocumentManager() = default;
    ~DocumentManager() = default;
    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;
    
    String generate_token();
    
    bool initialized_ = false;
    std::unordered_map<String, std::unique_ptr<Document>> documents_;
    TemplateRegistry templates_;
    UInt64 token_counter_ = 0;
    DocumentStats stats_;
    mutable std::mutex mutex_;
};

// =============================================================================
// EXEC CICS Interface Functions
// =============================================================================

// DOCUMENT CREATE
Result<String> exec_cics_document_create();
Result<String> exec_cics_document_create(DocumentType type);

// DOCUMENT SET
Result<void> exec_cics_document_set(StringView token, StringView content);
Result<void> exec_cics_document_set_symbol(StringView token, StringView name, StringView value);

// DOCUMENT INSERT
Result<void> exec_cics_document_insert(StringView token, StringView text);
Result<void> exec_cics_document_insert(StringView token, StringView text, InsertPosition pos);
Result<void> exec_cics_document_insert_template(StringView token, StringView template_name);

// DOCUMENT RETRIEVE
Result<String> exec_cics_document_retrieve(StringView token);
Result<UInt32> exec_cics_document_retrieve_into(StringView token, void* buffer, UInt32 max_len);

// DOCUMENT DELETE
Result<void> exec_cics_document_delete(StringView token);

// =============================================================================
// Utility Functions
// =============================================================================

[[nodiscard]] String document_type_to_string(DocumentType type);
[[nodiscard]] String insert_position_to_string(InsertPosition pos);

} // namespace document
} // namespace cics

#endif // CICS_DOCUMENT_HPP
