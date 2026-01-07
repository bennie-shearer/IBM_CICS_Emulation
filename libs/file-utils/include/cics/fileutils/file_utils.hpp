#pragma once
// =============================================================================
// IBM CICS Emulation - File Utilities
// Version: 3.4.6
// Cross-platform file operations for Windows, Linux, and macOS
// =============================================================================

#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <fstream>

namespace cics::fileutils {

// =============================================================================
// File Information
// =============================================================================

struct FileInfo {
    Path path;
    UInt64 size = 0;
    SystemTimePoint created;
    SystemTimePoint modified;
    SystemTimePoint accessed;
    bool is_directory = false;
    bool is_regular_file = false;
    bool is_symlink = false;
    bool is_readable = false;
    bool is_writable = false;
    bool is_executable = false;
    
    [[nodiscard]] String to_string() const;
};

// =============================================================================
// File Operations
// =============================================================================

// Check file existence and properties
[[nodiscard]] bool file_exists(const Path& path);
[[nodiscard]] bool is_directory(const Path& path);
[[nodiscard]] bool is_regular_file(const Path& path);
[[nodiscard]] bool is_readable(const Path& path);
[[nodiscard]] bool is_writable(const Path& path);

// Get file information
[[nodiscard]] Result<FileInfo> get_file_info(const Path& path);
[[nodiscard]] Result<UInt64> get_file_size(const Path& path);

// Read operations
[[nodiscard]] Result<String> read_text_file(const Path& path);
[[nodiscard]] Result<ByteBuffer> read_binary_file(const Path& path);
[[nodiscard]] Result<Vector<String>> read_lines(const Path& path);

// Write operations
[[nodiscard]] Result<void> write_text_file(const Path& path, StringView content);
[[nodiscard]] Result<void> write_binary_file(const Path& path, ConstByteSpan data);
[[nodiscard]] Result<void> write_lines(const Path& path, const Vector<String>& lines);
[[nodiscard]] Result<void> append_text_file(const Path& path, StringView content);

// Directory operations
[[nodiscard]] Result<void> create_directory(const Path& path);
[[nodiscard]] Result<void> create_directories(const Path& path);
[[nodiscard]] Result<void> remove_file(const Path& path);
[[nodiscard]] Result<void> remove_directory(const Path& path);
[[nodiscard]] Result<void> remove_all(const Path& path);

// Copy and move
[[nodiscard]] Result<void> copy_file(const Path& source, const Path& dest, bool overwrite = false);
[[nodiscard]] Result<void> move_file(const Path& source, const Path& dest);
[[nodiscard]] Result<void> rename_file(const Path& old_path, const Path& new_path);

// Directory listing
[[nodiscard]] Result<Vector<Path>> list_directory(const Path& path);
[[nodiscard]] Result<Vector<Path>> list_files(const Path& path, StringView pattern = "*");
[[nodiscard]] Result<Vector<Path>> find_files_recursive(const Path& path, StringView pattern = "*");

// Path operations
[[nodiscard]] Path get_absolute_path(const Path& path);
[[nodiscard]] Path get_canonical_path(const Path& path);
[[nodiscard]] String get_filename(const Path& path);
[[nodiscard]] String get_extension(const Path& path);
[[nodiscard]] String get_stem(const Path& path);
[[nodiscard]] Path get_parent_path(const Path& path);
[[nodiscard]] Path join_paths(const Path& base, const Path& relative);

// Temporary files
[[nodiscard]] Result<Path> create_temp_file(StringView prefix = "cics_");
[[nodiscard]] Result<Path> create_temp_directory(StringView prefix = "cics_");
[[nodiscard]] Path get_temp_directory();

// Platform-specific
[[nodiscard]] Path get_current_directory();
[[nodiscard]] Result<void> set_current_directory(const Path& path);
[[nodiscard]] Path get_home_directory();
[[nodiscard]] String get_path_separator();

// =============================================================================
// File Lock (Cross-platform)
// =============================================================================

enum class LockMode {
    SHARED,     // Read lock - multiple readers allowed
    EXCLUSIVE   // Write lock - exclusive access
};

class FileLock {
private:
    Path path_;
    bool locked_ = false;
    LockMode mode_ = LockMode::SHARED;
#ifdef _WIN32
    void* handle_ = nullptr;
#else
    int fd_ = -1;
#endif

public:
    FileLock() = default;
    explicit FileLock(const Path& path);
    ~FileLock();
    
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    FileLock(FileLock&& other) noexcept;
    FileLock& operator=(FileLock&& other) noexcept;
    
    [[nodiscard]] Result<void> lock(LockMode mode = LockMode::EXCLUSIVE);
    [[nodiscard]] Result<bool> try_lock(LockMode mode = LockMode::EXCLUSIVE);
    [[nodiscard]] Result<void> unlock();
    
    [[nodiscard]] bool is_locked() const { return locked_; }
    [[nodiscard]] LockMode mode() const { return mode_; }
    [[nodiscard]] const Path& path() const { return path_; }
};

// =============================================================================
// RAII File Lock Guard
// =============================================================================

class FileLockGuard {
private:
    FileLock& lock_;
    bool owns_lock_ = false;
    
public:
    explicit FileLockGuard(FileLock& lock, LockMode mode = LockMode::EXCLUSIVE);
    ~FileLockGuard();
    
    FileLockGuard(const FileLockGuard&) = delete;
    FileLockGuard& operator=(const FileLockGuard&) = delete;
    
    void release();
    [[nodiscard]] bool owns_lock() const { return owns_lock_; }
};

// =============================================================================
// Memory-Mapped File
// =============================================================================

class MemoryMappedFile {
private:
    Path path_;
    void* data_ = nullptr;
    Size size_ = 0;
    bool writable_ = false;
#ifdef _WIN32
    void* file_handle_ = nullptr;
    void* mapping_handle_ = nullptr;
#else
    int fd_ = -1;
#endif

public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile();
    
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;
    
    [[nodiscard]] Result<void> open(const Path& path, bool writable = false);
    void close();
    
    [[nodiscard]] bool is_open() const { return data_ != nullptr; }
    [[nodiscard]] void* data() { return data_; }
    [[nodiscard]] const void* data() const { return data_; }
    [[nodiscard]] Size size() const { return size_; }
    [[nodiscard]] bool is_writable() const { return writable_; }
    [[nodiscard]] const Path& path() const { return path_; }
    
    [[nodiscard]] ByteSpan span() { 
        return ByteSpan(static_cast<Byte*>(data_), size_); 
    }
    [[nodiscard]] ConstByteSpan span() const { 
        return ConstByteSpan(static_cast<const Byte*>(data_), size_); 
    }
    
    [[nodiscard]] Result<void> flush();
};

// =============================================================================
// Utility Functions
// =============================================================================

// Convert path to native format
[[nodiscard]] String to_native_path(const Path& path);

// Normalize path separators
[[nodiscard]] String normalize_path(StringView path);

// Check if path is absolute
[[nodiscard]] bool is_absolute_path(const Path& path);

// Get file checksum
[[nodiscard]] Result<UInt32> get_file_crc32(const Path& path);
[[nodiscard]] Result<String> get_file_md5(const Path& path);

} // namespace cics::fileutils
