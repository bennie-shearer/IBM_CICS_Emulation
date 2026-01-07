// =============================================================================
// IBM CICS Emulation - File Utilities Implementation
// Version: 3.4.6
// =============================================================================

#include <cics/fileutils/file_utils.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <pwd.h>
#endif

namespace cics::fileutils {

// =============================================================================
// FileInfo Implementation
// =============================================================================

String FileInfo::to_string() const {
    std::ostringstream oss;
    oss << "Path: " << path.string() << "\n"
        << "Size: " << size << " bytes\n"
        << "Type: " << (is_directory ? "Directory" : (is_regular_file ? "File" : "Other")) << "\n"
        << "Readable: " << (is_readable ? "Yes" : "No") << "\n"
        << "Writable: " << (is_writable ? "Yes" : "No");
    return oss.str();
}

// =============================================================================
// File Existence and Properties
// =============================================================================

bool file_exists(const Path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool is_directory(const Path& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

bool is_regular_file(const Path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool is_readable(const Path& path) {
#ifdef _WIN32
    return _access(path.string().c_str(), 4) == 0;
#else
    return access(path.c_str(), R_OK) == 0;
#endif
}

bool is_writable(const Path& path) {
#ifdef _WIN32
    return _access(path.string().c_str(), 2) == 0;
#else
    return access(path.c_str(), W_OK) == 0;
#endif
}

// =============================================================================
// File Information
// =============================================================================

Result<FileInfo> get_file_info(const Path& path) {
    std::error_code ec;
    
    if (!std::filesystem::exists(path, ec)) {
        return make_error<FileInfo>(ErrorCode::FILE_NOT_FOUND, 
            "File not found: " + path.string());
    }
    
    FileInfo info;
    info.path = path;
    info.is_directory = std::filesystem::is_directory(path, ec);
    info.is_regular_file = std::filesystem::is_regular_file(path, ec);
    info.is_symlink = std::filesystem::is_symlink(path, ec);
    
    if (info.is_regular_file) {
        info.size = std::filesystem::file_size(path, ec);
    }
    
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + 
            std::chrono::system_clock::now());
        info.modified = sctp;
    }
    
    info.is_readable = is_readable(path);
    info.is_writable = is_writable(path);
    
    return info;
}

Result<UInt64> get_file_size(const Path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return make_error<UInt64>(ErrorCode::IO_ERROR, 
            "Cannot get file size: " + ec.message());
    }
    return size;
}

// =============================================================================
// Read Operations
// =============================================================================

Result<String> read_text_file(const Path& path) {
    std::ifstream file(path, std::ios::in);
    if (!file) {
        return make_error<String>(ErrorCode::FILE_NOT_FOUND, 
            "Cannot open file: " + path.string());
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

Result<ByteBuffer> read_binary_file(const Path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return make_error<ByteBuffer>(ErrorCode::FILE_NOT_FOUND, 
            "Cannot open file: " + path.string());
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    ByteBuffer buffer(static_cast<Size>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return make_error<ByteBuffer>(ErrorCode::IO_ERROR, "Read failed");
    }
    
    return buffer;
}

Result<Vector<String>> read_lines(const Path& path) {
    std::ifstream file(path);
    if (!file) {
        return make_error<Vector<String>>(ErrorCode::FILE_NOT_FOUND, 
            "Cannot open file: " + path.string());
    }
    
    Vector<String> lines;
    String line;
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
    }
    
    return lines;
}

// =============================================================================
// Write Operations
// =============================================================================

Result<void> write_text_file(const Path& path, StringView content) {
    std::ofstream file(path);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create file: " + path.string());
    }
    
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, "Write failed");
    }
    
    return {};
}

Result<void> write_binary_file(const Path& path, ConstByteSpan data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create file: " + path.string());
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), 
               static_cast<std::streamsize>(data.size()));
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, "Write failed");
    }
    
    return {};
}

Result<void> write_lines(const Path& path, const Vector<String>& lines) {
    std::ofstream file(path);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create file: " + path.string());
    }
    
    for (const auto& line : lines) {
        file << line << '\n';
    }
    
    return {};
}

Result<void> append_text_file(const Path& path, StringView content) {
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot open file for append: " + path.string());
    }
    
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return {};
}

// =============================================================================
// Directory Operations
// =============================================================================

Result<void> create_directory(const Path& path) {
    std::error_code ec;
    if (!std::filesystem::create_directory(path, ec) && ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create directory: " + ec.message());
    }
    return {};
}

Result<void> create_directories(const Path& path) {
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec) && ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot create directories: " + ec.message());
    }
    return {};
}

Result<void> remove_file(const Path& path) {
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) && ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot remove file: " + ec.message());
    }
    return {};
}

Result<void> remove_directory(const Path& path) {
    return remove_file(path);
}

Result<void> remove_all(const Path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot remove: " + ec.message());
    }
    return {};
}

// =============================================================================
// Copy and Move
// =============================================================================

Result<void> copy_file(const Path& source, const Path& dest, bool overwrite) {
    std::error_code ec;
    auto options = overwrite ? 
        std::filesystem::copy_options::overwrite_existing : 
        std::filesystem::copy_options::none;
    
    std::filesystem::copy_file(source, dest, options, ec);
    if (ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot copy file: " + ec.message());
    }
    return {};
}

Result<void> move_file(const Path& source, const Path& dest) {
    std::error_code ec;
    std::filesystem::rename(source, dest, ec);
    if (ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot move file: " + ec.message());
    }
    return {};
}

Result<void> rename_file(const Path& old_path, const Path& new_path) {
    return move_file(old_path, new_path);
}

// =============================================================================
// Directory Listing
// =============================================================================

Result<Vector<Path>> list_directory(const Path& path) {
    std::error_code ec;
    Vector<Path> entries;
    
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        entries.push_back(entry.path());
    }
    
    if (ec) {
        return make_error<Vector<Path>>(ErrorCode::IO_ERROR, 
            "Cannot list directory: " + ec.message());
    }
    
    return entries;
}

Result<Vector<Path>> list_files(const Path& path, StringView pattern) {
    auto result = list_directory(path);
    if (!result.is_success()) return result;
    
    Vector<Path> filtered;
    for (const auto& entry : result.value()) {
        if (std::filesystem::is_regular_file(entry)) {
            if (pattern == "*" || entry.filename().string().find(pattern) != String::npos) {
                filtered.push_back(entry);
            }
        }
    }
    
    return filtered;
}

Result<Vector<Path>> find_files_recursive(const Path& path, StringView pattern) {
    std::error_code ec;
    Vector<Path> files;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
        if (entry.is_regular_file()) {
            if (pattern == "*" || entry.path().filename().string().find(pattern) != String::npos) {
                files.push_back(entry.path());
            }
        }
    }
    
    if (ec) {
        return make_error<Vector<Path>>(ErrorCode::IO_ERROR, 
            "Error traversing directory: " + ec.message());
    }
    
    return files;
}

// =============================================================================
// Path Operations
// =============================================================================

Path get_absolute_path(const Path& path) {
    std::error_code ec;
    return std::filesystem::absolute(path, ec);
}

Path get_canonical_path(const Path& path) {
    std::error_code ec;
    auto result = std::filesystem::canonical(path, ec);
    return ec ? path : result;
}

String get_filename(const Path& path) {
    return path.filename().string();
}

String get_extension(const Path& path) {
    return path.extension().string();
}

String get_stem(const Path& path) {
    return path.stem().string();
}

Path get_parent_path(const Path& path) {
    return path.parent_path();
}

Path join_paths(const Path& base, const Path& relative) {
    return base / relative;
}

// =============================================================================
// Temporary Files
// =============================================================================

Result<Path> create_temp_file(StringView prefix) {
    auto temp_dir = get_temp_directory();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    
    for (int i = 0; i < 100; ++i) {
        auto filename = String(prefix) + std::to_string(dis(gen)) + ".tmp";
        auto path = temp_dir / filename;
        
        if (!file_exists(path)) {
            std::ofstream file(path);
            if (file) {
                return path;
            }
        }
    }
    
    return make_error<Path>(ErrorCode::IO_ERROR, "Cannot create temp file");
}

Result<Path> create_temp_directory(StringView prefix) {
    auto temp_dir = get_temp_directory();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    
    for (int i = 0; i < 100; ++i) {
        auto dirname = String(prefix) + std::to_string(dis(gen));
        auto path = temp_dir / dirname;
        
        if (!file_exists(path)) {
            auto result = cics::fileutils::create_directory(path);
            if (result.is_success()) {
                return path;
            }
        }
    }
    
    return make_error<Path>(ErrorCode::IO_ERROR, "Cannot create temp directory");
}

Path get_temp_directory() {
    std::error_code ec;
    return std::filesystem::temp_directory_path(ec);
}

// =============================================================================
// Platform-Specific
// =============================================================================

Path get_current_directory() {
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

Result<void> set_current_directory(const Path& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (ec) {
        return make_error<void>(ErrorCode::IO_ERROR, 
            "Cannot change directory: " + ec.message());
    }
    return {};
}

Path get_home_directory() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (!home) {
        const char* drive = std::getenv("HOMEDRIVE");
        const char* path = std::getenv("HOMEPATH");
        if (drive && path) {
            return Path(String(drive) + path);
        }
    }
    return home ? Path(home) : get_current_directory();
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    return home ? Path(home) : get_current_directory();
#endif
}

String get_path_separator() {
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

// =============================================================================
// FileLock Implementation
// =============================================================================

FileLock::FileLock(const Path& path) : path_(path) {}

FileLock::~FileLock() {
    if (locked_) {
        (void)unlock();
    }
}

FileLock::FileLock(FileLock&& other) noexcept
    : path_(std::move(other.path_))
    , locked_(other.locked_)
    , mode_(other.mode_)
#ifdef _WIN32
    , handle_(other.handle_)
#else
    , fd_(other.fd_)
#endif
{
    other.locked_ = false;
#ifdef _WIN32
    other.handle_ = nullptr;
#else
    other.fd_ = -1;
#endif
}

FileLock& FileLock::operator=(FileLock&& other) noexcept {
    if (this != &other) {
        if (locked_) (void)unlock();
        path_ = std::move(other.path_);
        locked_ = other.locked_;
        mode_ = other.mode_;
#ifdef _WIN32
        handle_ = other.handle_;
        other.handle_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
        other.locked_ = false;
    }
    return *this;
}

Result<void> FileLock::lock(LockMode mode) {
    if (locked_) {
        return make_error<void>(ErrorCode::INVALID_ARGUMENT, "Already locked");
    }
    
#ifdef _WIN32
    handle_ = CreateFileA(
        path_.string().c_str(),
        mode == LockMode::EXCLUSIVE ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
        mode == LockMode::EXCLUSIVE ? 0 : FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (handle_ == INVALID_HANDLE_VALUE) {
        handle_ = nullptr;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot lock file");
    }
#else
    fd_ = open(path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot open file for lock");
    }
    
    int op = (mode == LockMode::EXCLUSIVE) ? LOCK_EX : LOCK_SH;
    if (flock(fd_, op) != 0) {
        close(fd_);
        fd_ = -1;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot acquire lock");
    }
#endif
    
    locked_ = true;
    mode_ = mode;
    return {};
}

Result<bool> FileLock::try_lock(LockMode mode) {
    if (locked_) {
        return false;
    }
    
#ifdef _WIN32
    handle_ = CreateFileA(
        path_.string().c_str(),
        mode == LockMode::EXCLUSIVE ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
        mode == LockMode::EXCLUSIVE ? 0 : FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (handle_ == INVALID_HANDLE_VALUE) {
        handle_ = nullptr;
        return false;
    }
#else
    fd_ = open(path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        return false;
    }
    
    int op = (mode == LockMode::EXCLUSIVE) ? (LOCK_EX | LOCK_NB) : (LOCK_SH | LOCK_NB);
    if (flock(fd_, op) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }
#endif
    
    locked_ = true;
    mode_ = mode;
    return true;
}

Result<void> FileLock::unlock() {
    if (!locked_) {
        return {};
    }
    
#ifdef _WIN32
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    if (fd_ >= 0) {
        flock(fd_, LOCK_UN);
        close(fd_);
        fd_ = -1;
    }
#endif
    
    locked_ = false;
    return {};
}

// =============================================================================
// FileLockGuard Implementation
// =============================================================================

FileLockGuard::FileLockGuard(FileLock& lock, LockMode mode) : lock_(lock) {
    auto result = lock_.lock(mode);
    owns_lock_ = result.is_success();
}

FileLockGuard::~FileLockGuard() {
    if (owns_lock_) {
        (void)lock_.unlock();
    }
}

void FileLockGuard::release() {
    if (owns_lock_) {
        (void)lock_.unlock();
        owns_lock_ = false;
    }
}

// =============================================================================
// MemoryMappedFile Implementation
// =============================================================================

MemoryMappedFile::~MemoryMappedFile() {
    close();
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : path_(std::move(other.path_))
    , data_(other.data_)
    , size_(other.size_)
    , writable_(other.writable_)
#ifdef _WIN32
    , file_handle_(other.file_handle_)
    , mapping_handle_(other.mapping_handle_)
#else
    , fd_(other.fd_)
#endif
{
    other.data_ = nullptr;
    other.size_ = 0;
#ifdef _WIN32
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#else
    other.fd_ = -1;
#endif
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        data_ = other.data_;
        size_ = other.size_;
        writable_ = other.writable_;
#ifdef _WIN32
        file_handle_ = other.file_handle_;
        mapping_handle_ = other.mapping_handle_;
        other.file_handle_ = nullptr;
        other.mapping_handle_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

Result<void> MemoryMappedFile::open(const Path& path, bool writable) {
    if (is_open()) {
        close();
    }
    
    path_ = path;
    writable_ = writable;
    
#ifdef _WIN32
    DWORD access = writable ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
    DWORD share = writable ? 0 : FILE_SHARE_READ;
    
    file_handle_ = CreateFileA(
        path.string().c_str(),
        access,
        share,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (file_handle_ == INVALID_HANDLE_VALUE) {
        file_handle_ = nullptr;
        return make_error<void>(ErrorCode::FILE_NOT_FOUND, "Cannot open file");
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle_, &file_size)) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot get file size");
    }
    
    size_ = static_cast<Size>(file_size.QuadPart);
    
    DWORD protect = writable ? PAGE_READWRITE : PAGE_READONLY;
    mapping_handle_ = CreateFileMappingA(
        file_handle_,
        nullptr,
        protect,
        0,
        0,
        nullptr
    );
    
    if (!mapping_handle_) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot create file mapping");
    }
    
    DWORD map_access = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
    data_ = MapViewOfFile(mapping_handle_, map_access, 0, 0, 0);
    
    if (!data_) {
        CloseHandle(mapping_handle_);
        CloseHandle(file_handle_);
        mapping_handle_ = nullptr;
        file_handle_ = nullptr;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot map file");
    }
#else
    int flags = writable ? O_RDWR : O_RDONLY;
    fd_ = ::open(path.c_str(), flags);
    
    if (fd_ < 0) {
        return make_error<void>(ErrorCode::FILE_NOT_FOUND, "Cannot open file");
    }
    
    struct stat st;
    if (fstat(fd_, &st) < 0) {
        ::close(fd_);
        fd_ = -1;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot stat file");
    }
    
    size_ = static_cast<Size>(st.st_size);
    
    int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    data_ = mmap(nullptr, size_, prot, MAP_SHARED, fd_, 0);
    
    if (data_ == MAP_FAILED) {
        data_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        return make_error<void>(ErrorCode::IO_ERROR, "Cannot map file");
    }
#endif
    
    return {};
}

void MemoryMappedFile::close() {
    if (!is_open()) return;
    
#ifdef _WIN32
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_handle_) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    if (file_handle_) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
    }
#else
    if (data_) {
        munmap(data_, size_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
    
    size_ = 0;
}

Result<void> MemoryMappedFile::flush() {
    if (!is_open() || !writable_) {
        return {};
    }
    
#ifdef _WIN32
    if (!FlushViewOfFile(data_, 0)) {
        return make_error<void>(ErrorCode::IO_ERROR, "Flush failed");
    }
#else
    if (msync(data_, size_, MS_SYNC) != 0) {
        return make_error<void>(ErrorCode::IO_ERROR, "Flush failed");
    }
#endif
    
    return {};
}

// =============================================================================
// Utility Functions
// =============================================================================

String to_native_path(const Path& path) {
    return path.string();
}

String normalize_path(StringView path) {
    String result(path);
#ifdef _WIN32
    std::replace(result.begin(), result.end(), '/', '\\');
#else
    std::replace(result.begin(), result.end(), '\\', '/');
#endif
    return result;
}

bool is_absolute_path(const Path& path) {
    return path.is_absolute();
}

Result<UInt32> get_file_crc32(const Path& path) {
    auto data = read_binary_file(path);
    if (!data.is_success()) {
        return make_error<UInt32>(data.error().code, data.error().message);
    }
    
    return crc32(ConstByteSpan(data.value()));
}

Result<String> get_file_md5(const Path& path) {
    // Simplified MD5 - return CRC32 as hex for now
    auto crc = get_file_crc32(path);
    if (!crc.is_success()) {
        return make_error<String>(crc.error().code, crc.error().message);
    }
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << crc.value();
    return oss.str();
}

} // namespace cics::fileutils
