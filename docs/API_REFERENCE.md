# IBM CICS (Customer Information Control System) Emulation - API Reference
Version 3.4.6

This document provides a comprehensive API reference for all modules.

## Table of Contents

1. [Interval Control](#interval-control)
2. [Task Control](#task-control)
3. [Storage Control](#storage-control)
4. [Program Control](#program-control)
5. [EBCDIC Conversion](#ebcdic-conversion)
6. [BMS (Basic Mapping Support)](#bms)
7. [Copybook Parser](#copybook-parser)
8. [Dump Utilities](#dump-utilities)

---

## Interval Control

**Header:** `<cics/interval/interval_control.hpp>`  
**Namespace:** `cics::interval`

### Types

```cpp
struct AbsTime {
    Int64 value;           // Microseconds since epoch
    
    static AbsTime now();
    static AbsTime from_hhmmss(UInt32 hhmmss);
    static AbsTime from_yyyymmdd(UInt32 yyyymmdd);
    String to_string() const;
};

struct TimeInfo {
    UInt32 date;           // YYYYMMDD format
    UInt32 time;           // HHMMSS format
    UInt16 year;
    UInt8 month;
    UInt8 day;
    UInt8 day_of_week;     // 0=Sunday
    UInt16 milliseconds;
    AbsTime abstime;
};

struct StartRequest {
    String transaction_id;
    UInt32 interval;       // Seconds until start
    ByteBuffer data;       // Optional data to pass
    String reqid;          // Request identifier
};

enum class EventState { WAITING, POSTED, EXPIRED };
```

### Manager Class

```cpp
class IntervalControlManager {
public:
    static IntervalControlManager& instance();
    
    void initialize();
    void shutdown();
    
    Result<TimeInfo> asktime();
    Result<void> delay(UInt32 milliseconds);
    Result<void> delay_interval(UInt32 hhmmss);
    Result<String> post(UInt32 interval_seconds);
    Result<EventState> wait_event(StringView event_id, UInt32 timeout_ms);
    Result<String> start(const StartRequest& request);
    Result<ByteBuffer> retrieve(StringView reqid);
    Result<void> cancel(StringView reqid);
    
    IntervalStats get_stats() const;
};
```

### EXEC CICS Functions

```cpp
Result<TimeInfo> exec_cics_asktime();
Result<AbsTime> exec_cics_asktime_abstime();
Result<void> exec_cics_delay(UInt32 milliseconds);
Result<void> exec_cics_delay_interval(UInt32 hhmmss);
Result<String> exec_cics_post(UInt32 interval);
Result<EventState> exec_cics_wait_event(StringView event_id, UInt32 timeout = 0);
Result<String> exec_cics_start(StringView transid, UInt32 interval = 0);
Result<String> exec_cics_start_data(StringView transid, const ByteBuffer& data, UInt32 interval = 0);
Result<ByteBuffer> exec_cics_retrieve();
Result<ByteBuffer> exec_cics_retrieve_reqid(StringView reqid);
Result<void> exec_cics_cancel(StringView reqid);
```

### Example

```cpp
#include <cics/interval/interval_control.hpp>
using namespace cics::interval;

auto& mgr = IntervalControlManager::instance();
mgr.initialize();

// Get current time
auto time = exec_cics_asktime();
std::cout << "Date: " << time.value().date << "\n";

// Delay for 2 seconds
exec_cics_delay(2000);

// Schedule a transaction
auto reqid = exec_cics_start("TRNA", 60);  // Start in 60 seconds

// Cancel scheduled transaction
exec_cics_cancel(reqid.value());

mgr.shutdown();
```

---

## Task Control

**Header:** `<cics/task/task_control.hpp>`  
**Namespace:** `cics::task`

### Types

```cpp
enum class LockType { EXCLUSIVE, SHARED, UPDATE };
enum class LockScope { TASK, UOW, PERMANENT };

struct ResourceId {
    String name;
    UInt16 length;
    
    ResourceId(StringView n);
    ResourceId(StringView n, UInt16 len);
};

struct TaskInfo {
    UInt32 task_id;
    String transaction_id;
    TaskState state;
    std::chrono::steady_clock::time_point start_time;
};

struct TaskStats {
    UInt64 enq_count;
    UInt64 deq_count;
    UInt64 enq_wait_count;
    UInt64 deadlock_detections;
    UInt64 tasks_created;
    UInt64 tasks_completed;
};
```

### Manager Class

```cpp
class TaskControlManager {
public:
    static TaskControlManager& instance();
    
    Result<void> enq(const ResourceId& resource, 
                     LockType type = LockType::EXCLUSIVE,
                     LockScope scope = LockScope::TASK,
                     bool nosuspend = false);
    Result<void> deq(const ResourceId& resource);
    Result<void> suspend();
    
    Result<UInt32> create_task(StringView transid);
    Result<void> end_task(UInt32 task_id);
    
    bool is_locked(const ResourceId& resource) const;
    bool detect_deadlock() const;
    
    std::vector<ResourceId> list_locks() const;
    std::vector<TaskInfo> list_tasks() const;
    TaskStats get_stats() const;
};
```

### RAII Guard

```cpp
class ResourceLock {
public:
    ResourceLock(const ResourceId& resource, 
                 LockType type = LockType::EXCLUSIVE);
    ~ResourceLock();  // Automatically releases lock
    
    bool acquired() const;
    void release();
};
```

### EXEC CICS Functions

```cpp
Result<void> exec_cics_enq(StringView resource);
Result<void> exec_cics_enq(StringView resource, UInt16 length);
Result<void> exec_cics_enq_shared(StringView resource);
Result<void> exec_cics_enq_nosuspend(StringView resource);
Result<void> exec_cics_deq(StringView resource);
Result<void> exec_cics_deq(StringView resource, UInt16 length);
Result<void> exec_cics_suspend();
```

### Example

```cpp
#include <cics/task/task_control.hpp>
using namespace cics::task;

// Exclusive lock
exec_cics_enq("CUSTOMER-FILE");
// ... work with resource ...
exec_cics_deq("CUSTOMER-FILE");

// Using RAII guard
{
    ResourceLock lock("TEMP-RESOURCE");
    if (lock.acquired()) {
        // Lock automatically released when scope ends
    }
}
```

---

## Storage Control

**Header:** `<cics/storage/storage_control.hpp>`  
**Namespace:** `cics::storage`

### Types

```cpp
enum class StorageClass {
    USER,      // User storage (default)
    CICSDSA,   // CICS Dynamic Storage Area
    CDSA,      // CICS DSA below the line
    UDSA,      // User DSA below the line
    SDSA,      // Shared DSA
    RDSA,      // Read-only DSA
    SHARED     // Shared storage
};

enum class StorageInit {
    DEFAULT,   // No initialization
    ZERO,      // Zero-fill
    HIGH,      // Fill with 0xFF
    LOW        // Fill with 0x00
};

struct StorageBlock {
    void* address;
    UInt32 size;
    StorageClass storage_class;
    UInt32 task_id;
    std::chrono::steady_clock::time_point alloc_time;
};

struct StorageStats {
    UInt64 total_allocated;
    UInt64 total_freed;
    UInt64 current_usage;
    UInt64 peak_usage;
    UInt64 allocation_count;
    UInt64 free_count;
};
```

### Manager Class

```cpp
class StorageControlManager {
public:
    static StorageControlManager& instance();
    
    Result<void*> getmain(UInt32 length,
                          StorageClass sc = StorageClass::USER,
                          StorageInit init = StorageInit::DEFAULT,
                          Byte init_value = 0);
    Result<void> freemain(void* address);
    Result<void> freemain_task(UInt32 task_id);
    
    Result<StorageBlock> get_block_info(void* address) const;
    StorageStats get_stats() const;
};
```

### RAII Guard

```cpp
class StorageGuard {
public:
    explicit StorageGuard(UInt32 size, 
                          StorageClass sc = StorageClass::USER,
                          StorageInit init = StorageInit::DEFAULT);
    ~StorageGuard();  // Automatically frees storage
    
    bool valid() const;
    void* get() const;
    UInt32 size() const;
    
    template<typename T>
    T* as() { return static_cast<T*>(ptr_); }
    
    void release();  // Transfer ownership
};
```

### Utility Functions

```cpp
void storage_init_zero(void* ptr, UInt32 length);
void storage_init_value(void* ptr, UInt32 length, Byte value);
void storage_copy(void* dest, const void* src, UInt32 length);
void storage_move(void* dest, const void* src, UInt32 length);
int storage_compare(const void* p1, const void* p2, UInt32 length);
bool storage_equal(const void* p1, const void* p2, UInt32 length);
```

### EXEC CICS Functions

```cpp
Result<void*> exec_cics_getmain(UInt32 length);
Result<void*> exec_cics_getmain_initimg(UInt32 length, Byte value);
Result<void*> exec_cics_getmain_below(UInt32 length);
Result<void*> exec_cics_getmain_shared(UInt32 length);
Result<void> exec_cics_freemain(void* address);
```

### Example

```cpp
#include <cics/storage/storage_control.hpp>
using namespace cics::storage;

// Basic allocation
auto result = exec_cics_getmain(1024);
if (result.is_success()) {
    void* ptr = result.value();
    // Use storage...
    exec_cics_freemain(ptr);
}

// Using RAII guard with typed access
{
    StorageGuard guard(sizeof(int) * 100, StorageClass::USER, StorageInit::ZERO);
    if (guard.valid()) {
        int* array = guard.as<int>();
        for (int i = 0; i < 100; i++) {
            array[i] = i * i;
        }
    }
    // Automatically freed
}
```

---

## Program Control

**Header:** `<cics/program/program_control.hpp>`  
**Namespace:** `cics::program`

### Types

```cpp
enum class ProgramType {
    ASSEMBLER, COBOL, PLI, C, CPP, JAVA, NATIVE
};

enum class ProgramStatus {
    NOT_LOADED, LOADED, ENABLED, DISABLED, NEW_COPY
};

using ProgramEntry = std::function<Result<void>(void* commarea, UInt32 length)>;

struct ProgramDefinition {
    String name;
    ProgramType type;
    ProgramStatus status;
    ProgramEntry entry_point;
    UInt32 load_count;
    UInt32 use_count;
};

struct LinkLevel {
    String program;
    void* commarea;
    UInt32 commarea_length;
    int level;
};
```

### Manager Class

```cpp
class ProgramControlManager {
public:
    static ProgramControlManager& instance();
    
    Result<void> define_program(StringView name, 
                                 ProgramEntry entry,
                                 ProgramType type = ProgramType::NATIVE);
    
    Result<void> link(StringView program, void* commarea = nullptr, UInt32 length = 0);
    Result<void> xctl(StringView program, void* commarea = nullptr, UInt32 length = 0);
    Result<void> return_program(void* commarea = nullptr, UInt32 length = 0);
    
    Result<void*> load(StringView program);
    Result<void> release(StringView program);
    
    Result<void> enable_program(StringView program);
    Result<void> disable_program(StringView program);
    Result<void> newcopy_program(StringView program);
    
    String current_program() const;
    int current_level() const;
    std::vector<LinkLevel> get_link_stack() const;
};
```

### Helper Classes

```cpp
class ProgramRegistrar {
public:
    ProgramRegistrar(StringView name, ProgramEntry entry, 
                     ProgramType type = ProgramType::NATIVE);
};

// Registration macro
#define CICS_REGISTER_PROGRAM(name, entry) \
    static cics::program::ProgramRegistrar _reg_##name(#name, entry)
```

### EXEC CICS Functions

```cpp
Result<void> exec_cics_link(StringView program);
Result<void> exec_cics_link(StringView program, void* commarea, UInt32 length);
Result<void> exec_cics_xctl(StringView program);
Result<void> exec_cics_xctl(StringView program, void* commarea, UInt32 length);
Result<void> exec_cics_return();
Result<void> exec_cics_return(void* commarea, UInt32 length);
Result<void*> exec_cics_load(StringView program);
Result<void> exec_cics_release(StringView program);
```

### Example

```cpp
#include <cics/program/program_control.hpp>
using namespace cics::program;

// Define a program
auto& mgr = ProgramControlManager::instance();
mgr.define_program("CUSTINQ", [](void* comm, UInt32 len) -> Result<void> {
    // Program logic here
    return make_success();
});

// Or use the macro
CICS_REGISTER_PROGRAM(ACCTUPD, [](void* comm, UInt32 len) -> Result<void> {
    return make_success();
});

// Call a program
struct Commarea { char data[100]; };
Commarea comm;
exec_cics_link("CUSTINQ", &comm, sizeof(comm));
```

---

## EBCDIC Conversion

**Header:** `<cics/ebcdic/ebcdic.hpp>`  
**Namespace:** `cics::ebcdic`

### Character Conversion

```cpp
// Single character
Byte ascii_to_ebcdic(Byte ascii);
Byte ebcdic_to_ascii(Byte ebcdic);

// Buffer conversion (returns new buffer)
ByteBuffer string_to_ebcdic(StringView ascii);
String ebcdic_to_string(const ByteBuffer& ebcdic);
String ebcdic_to_string(const Byte* data, UInt32 length);

// In-place conversion
void ascii_to_ebcdic_inplace(Byte* data, UInt32 length);
void ebcdic_to_ascii_inplace(Byte* data, UInt32 length);
```

### Packed Decimal (COMP-3)

```cpp
struct PackedDecimal {
    ByteBuffer data;
    UInt8 precision;   // Total digits
    UInt8 scale;       // Decimal places
    
    static PackedDecimal from_int64(Int64 value, UInt8 precision, UInt8 scale = 0);
    static PackedDecimal from_double(double value, UInt8 precision, UInt8 scale);
    static PackedDecimal from_string(StringView str, UInt8 precision, UInt8 scale);
    
    Int64 to_int64() const;
    double to_double() const;
    String to_string() const;
    String to_display() const;  // With formatting
    
    bool is_positive() const;
    bool is_negative() const;
    bool is_valid() const;
};

// Standalone functions
Int64 packed_to_int64(const Byte* data, UInt32 length);
double packed_to_double(const Byte* data, UInt32 length, UInt8 scale);
String packed_to_string(const Byte* data, UInt32 length);
ByteBuffer int64_to_packed(Int64 value, UInt32 length);
ByteBuffer string_to_packed(StringView str, UInt32 length);
bool is_valid_packed(const Byte* data, UInt32 length);
```

### Zoned Decimal (DISPLAY)

```cpp
Int64 zoned_to_int64(const Byte* data, UInt32 length);
String zoned_to_string(const Byte* data, UInt32 length);
ByteBuffer int64_to_zoned(Int64 value, UInt32 length);
```

### Binary (COMP)

```cpp
Int16 binary_to_int16(const Byte* data);
Int32 binary_to_int32(const Byte* data);
Int64 binary_to_int64(const Byte* data);
ByteBuffer int16_to_binary(Int16 value);
ByteBuffer int32_to_binary(Int32 value);
ByteBuffer int64_to_binary(Int64 value);
```

### Character Classification

```cpp
bool is_ebcdic_alpha(Byte b);
bool is_ebcdic_digit(Byte b);
bool is_ebcdic_alnum(Byte b);
bool is_ebcdic_space(Byte b);
bool is_ebcdic_printable(Byte b);
```

### Constants

```cpp
constexpr Byte EBCDIC_SPACE = 0x40;
constexpr Byte EBCDIC_ZERO = 0xF0;
constexpr Byte EBCDIC_A = 0xC1;
constexpr Byte EBCDIC_Z = 0xE9;
```

### Example

```cpp
#include <cics/ebcdic/ebcdic.hpp>
using namespace cics::ebcdic;

// Convert string
ByteBuffer ebcdic = string_to_ebcdic("HELLO");
String ascii = ebcdic_to_string(ebcdic);

// Packed decimal
auto pd = PackedDecimal::from_double(1234.56, 8, 2);
std::cout << pd.to_display() << "\n";  // "1234.56"

// Low-level packed operations
ByteBuffer packed = int64_to_packed(12345, 4);
Int64 value = packed_to_int64(packed.data(), 4);
```

---

## BMS

**Header:** `<cics/bms/bms.hpp>`  
**Namespace:** `cics::bms`

### Types

```cpp
enum class FieldAttribute {
    NORMAL, BRIGHT, DARK, PROTECTED, NUMERIC, 
    ASKIP, UNPROT, MDT, FSET
};

enum class FieldColor {
    DEFAULT, BLUE, RED, PINK, GREEN, TURQUOISE, YELLOW, WHITE
};

enum class FieldHighlight {
    NORMAL, BLINK, REVERSE, UNDERLINE
};

enum class AIDKey {
    ENTER, CLEAR, PA1, PA2, PA3,
    PF1, PF2, PF3, PF4, PF5, PF6, PF7, PF8, PF9, PF10, PF11, PF12,
    PF13, PF14, PF15, PF16, PF17, PF18, PF19, PF20, PF21, PF22, PF23, PF24
};

struct MapField {
    String name;
    UInt8 row, col;
    UInt16 length;
    FieldAttribute attribute;
    FieldColor color;
    FieldHighlight highlight;
    String initial_value;
    String value;
};

struct MapDefinition {
    String name;
    UInt8 rows, cols;
    std::vector<MapField> fields;
};

struct Mapset {
    String name;
    std::vector<MapDefinition> maps;
};

struct MapData {
    std::unordered_map<String, String> fields;
    AIDKey aid_key;
    UInt8 cursor_row, cursor_col;
};
```

### Manager Class

```cpp
class BMSManager {
public:
    static BMSManager& instance();
    
    Result<void> define_mapset(const Mapset& mapset);
    Result<void> send_map(StringView mapset, StringView map, const MapData& data);
    Result<MapData> receive_map(StringView mapset, StringView map);
    Result<void> send_text(StringView text, UInt8 row = 1, UInt8 col = 1);
    
    Result<void> set_field_value(StringView map, StringView field, StringView value);
    Result<String> get_field_value(StringView map, StringView field);
    Result<void> set_field_attribute(StringView map, StringView field, FieldAttribute attr);
    
    ScreenBuffer& get_screen();
};
```

### Builder Class

```cpp
class MapBuilder {
public:
    MapBuilder(StringView name, UInt8 rows = 24, UInt8 cols = 80);
    
    MapBuilder& field(StringView name, UInt8 row, UInt8 col, UInt16 length);
    MapBuilder& attribute(FieldAttribute attr);
    MapBuilder& color(FieldColor c);
    MapBuilder& highlight(FieldHighlight h);
    MapBuilder& initial(StringView value);
    
    MapDefinition build();
};
```

### Example

```cpp
#include <cics/bms/bms.hpp>
using namespace cics::bms;

// Build a map
auto map = MapBuilder("MENU01")
    .field("TITLE", 1, 30, 20)
        .attribute(FieldAttribute::PROTECTED)
        .color(FieldColor::WHITE)
        .initial("MAIN MENU")
    .field("OPTION", 10, 35, 2)
        .attribute(FieldAttribute::UNPROT)
        .color(FieldColor::GREEN)
    .build();

// Send map
MapData data;
data.fields["TITLE"] = "CUSTOMER INQUIRY";
exec_cics_send_map("MENUMAP", "MENU01", data);

// Receive input
auto input = exec_cics_receive_map("MENUMAP", "MENU01");
if (input.is_success()) {
    String option = input.value().fields["OPTION"];
}
```

---

## Copybook Parser

**Header:** `<cics/copybook/copybook.hpp>`  
**Namespace:** `cics::copybook`

### Types

```cpp
enum class DataType {
    ALPHANUMERIC, NUMERIC, PACKED, BINARY, FLOAT, GROUP
};

enum class UsageType {
    DISPLAY, COMP, COMP_1, COMP_2, COMP_3
};

struct PictureClause {
    String pattern;
    UInt16 total_digits;
    UInt16 decimal_places;
    bool is_signed;
    bool is_numeric;
};

struct CopybookField {
    UInt8 level;
    String name;
    DataType type;
    UsageType usage;
    UInt16 size;
    UInt32 offset;
    UInt16 occurs;
    PictureClause picture;
    std::vector<std::unique_ptr<CopybookField>> children;
};

struct CopybookDefinition {
    String name;
    UInt32 record_length;
    std::vector<std::unique_ptr<CopybookField>> fields;
    
    const CopybookField* find_field(StringView name) const;
    std::vector<const CopybookField*> all_fields() const;
    String to_string() const;
    String generate_source() const;  // Generate C++ struct
};
```

### Parser Class

```cpp
class CopybookParser {
public:
    Result<CopybookDefinition> parse(StringView source);
    Result<CopybookDefinition> parse_file(StringView filename);
    
    // Code generation
    String generate_struct(const CopybookDefinition& copybook);
    Result<void> generate_file(const CopybookDefinition& copybook, StringView path);
};
```

### Record Accessor

```cpp
class RecordAccessor {
public:
    RecordAccessor(const CopybookDefinition& copybook, void* data, UInt32 length);
    
    Result<String> get_string(StringView field_name) const;
    Result<Int64> get_integer(StringView field_name) const;
    Result<double> get_decimal(StringView field_name) const;
    Result<Int64> get_packed(StringView field_name) const;
    Result<ByteBuffer> get_raw(StringView field_name) const;
    
    Result<void> set_string(StringView field_name, StringView value);
    Result<void> set_integer(StringView field_name, Int64 value);
    Result<void> set_decimal(StringView field_name, double value);
    Result<void> set_packed(StringView field_name, Int64 value);
    Result<void> set_raw(StringView field_name, const ByteBuffer& value);
    
    // Array access
    Result<String> get_string(StringView field_name, UInt16 index) const;
    Result<void> set_string(StringView field_name, UInt16 index, StringView value);
};
```

### Example

```cpp
#include <cics/copybook/copybook.hpp>
using namespace cics::copybook;

const char* copybook_source = R"(
       01  CUSTOMER-RECORD.
           05  CUST-ID           PIC 9(8).
           05  CUST-NAME         PIC X(30).
           05  CUST-BALANCE      PIC S9(9)V99 COMP-3.
)";

CopybookParser parser;
auto result = parser.parse(copybook_source);
if (result.is_success()) {
    auto& copybook = result.value();
    
    // Access record data
    Byte record[100];
    RecordAccessor accessor(copybook, record, sizeof(record));
    
    accessor.set_string("CUST-NAME", "JOHN DOE");
    accessor.set_integer("CUST-ID", 12345678);
    accessor.set_packed("CUST-BALANCE", 150075);  // $1500.75
    
    String name = accessor.get_string("CUST-NAME").value();
}
```

---

## Dump Utilities

**Header:** `<cics/dump/dump.hpp>`  
**Namespace:** `cics::dump`

### Types

```cpp
struct DumpOptions {
    UInt8 bytes_per_line = 16;
    bool show_offset = true;
    bool show_hex = true;
    bool show_ascii = true;
    bool show_ebcdic = false;
    bool uppercase_hex = true;
    UInt8 group_size = 4;      // Group bytes in hex output
};

struct StorageDumpHeader {
    String timestamp;
    String transaction_id;
    String task_number;
    String program_name;
    UInt64 address;
    UInt32 length;
};

struct FieldInfo {
    String name;
    UInt32 offset;
    UInt32 length;
};

struct DumpStats {
    UInt32 total_bytes;
    UInt32 printable_ascii;
    UInt32 printable_ebcdic;
    UInt32 null_bytes;
    UInt32 high_bytes;       // 0x80-0xFF
    std::array<UInt32, 256> byte_histogram;
};
```

### Functions

```cpp
// Basic hex dump
String hex_dump(const void* data, UInt32 length, const DumpOptions& opts = {});
void hex_dump(std::ostream& os, const void* data, UInt32 length, const DumpOptions& opts = {});
Result<void> hex_dump_file(StringView filename, const void* data, UInt32 length, const DumpOptions& opts = {});

// CICS-style storage dump
String storage_dump(const void* data, UInt32 length, const StorageDumpHeader& header);
void storage_dump(std::ostream& os, const void* data, UInt32 length, const StorageDumpHeader& header);

// Comparison dump
String compare_dump(const void* buf1, UInt32 len1, const void* buf2, UInt32 len2);

// Record dump (with record boundaries)
String record_dump(const void* data, UInt32 total_length, UInt32 record_length);

// Field dump
String field_dump(const void* data, const std::vector<FieldInfo>& fields);

// Utility functions
String byte_to_hex(Byte b, bool uppercase = true);
String bytes_to_hex(const Byte* data, UInt32 length, bool uppercase = true);
ByteBuffer hex_to_bytes(StringView hex_string);

// Analysis
DumpStats analyze_dump(const void* data, UInt32 length);
bool is_printable_ascii(Byte b);
bool is_printable_ebcdic(Byte b);
```

### DumpWriter Class

```cpp
class DumpWriter {
public:
    explicit DumpWriter(StringView filename);
    ~DumpWriter();
    
    void write_header(const StorageDumpHeader& header);
    void write_hex(const void* data, UInt32 length, const DumpOptions& opts = {});
    void write_separator();
    void write_text(StringView text);
    void close();
};
```

### DumpBrowser Class

```cpp
class DumpBrowser {
public:
    DumpBrowser(const void* data, UInt32 length, UInt8 lines_per_page = 20);
    
    String current_page();
    bool next_page();
    bool prev_page();
    bool goto_offset(UInt32 offset);
    
    std::vector<UInt32> find(const ByteBuffer& pattern);
    std::vector<UInt32> find(StringView text);
    
    UInt32 total_pages() const;
    UInt32 current_page_number() const;
};
```

### Example

```cpp
#include <cics/dump/dump.hpp>
using namespace cics::dump;

Byte data[64];
// ... fill data ...

// Simple hex dump
std::cout << hex_dump(data, sizeof(data));

// Custom options
DumpOptions opts;
opts.bytes_per_line = 32;
opts.uppercase_hex = false;
std::cout << hex_dump(data, sizeof(data), opts);

// CICS-style storage dump
StorageDumpHeader header;
header.timestamp = "2025-12-22 10:30:45";
header.transaction_id = "TRNA";
header.program_name = "CUSTINQ";
header.address = 0xABC000;
header.length = sizeof(data);
std::cout << storage_dump(data, sizeof(data), header);

// Comparison
Byte data2[64];
std::cout << compare_dump(data, 64, data2, 64);

// Analysis
auto stats = analyze_dump(data, sizeof(data));
std::cout << "Null bytes: " << stats.null_bytes << "\n";
```

---

## Common Types

**Header:** `<cics/common/types.hpp>`  
**Namespace:** `cics`

```cpp
// Integer types
using Int8 = std::int8_t;
using Int16 = std::int16_t;
using Int32 = std::int32_t;
using Int64 = std::int64_t;
using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;
using Byte = std::uint8_t;

// String types
using String = std::string;
using StringView = std::string_view;
using ByteBuffer = std::vector<Byte>;

// Result type
template<typename T>
class Result {
public:
    bool is_success() const;
    bool is_error() const;
    T& value();
    const T& value() const;
    ErrorCode error_code() const;
    const String& error_message() const;
};

template<typename T>
Result<T> make_success(T value);
Result<void> make_success();
template<typename T>
Result<T> make_error(ErrorCode code, StringView message);
```

---

Copyright (c) 2025 Bennie Shearer

---

## Syncpoint Control Module

### Header
```cpp
#include <cics/syncpoint/syncpoint.hpp>
```

### SyncpointManager

```cpp
class SyncpointManager {
public:
    static SyncpointManager& instance();
    
    void initialize();
    void shutdown();
    
    // UOW management
    Result<String> begin_uow();
    Result<void> end_uow(StringView uow_id);
    UnitOfWork* current_uow();
    
    // Syncpoint operations
    Result<void> syncpoint();
    Result<void> rollback();
    
    // Resource registration
    Result<void> register_resource(std::shared_ptr<RecoveryResource> resource);
    Result<void> register_resource(StringView name, ResourceType type,
                                    PrepareCallback prepare,
                                    CommitCallback commit,
                                    RollbackCallback rollback);
    
    SyncpointStats get_stats() const;
};
```

### SyncpointGuard (RAII)

```cpp
class SyncpointGuard {
public:
    explicit SyncpointGuard(bool auto_commit = true);
    ~SyncpointGuard();
    
    bool is_active() const;
    const String& uow_id() const;
    
    Result<void> commit();
    Result<void> rollback();
    void release();
};
```

### EXEC CICS Functions

```cpp
Result<void> exec_cics_syncpoint();
Result<void> exec_cics_syncpoint_rollback();
Result<void> exec_cics_syncpoint_rollbackuow();
```

---

## Abend Handler Module

### Header
```cpp
#include <cics/abend/abend.hpp>
```

### Standard Abend Codes

| Code | Description |
|------|-------------|
| ASRA | Program check exception |
| ASRB | Operating system abend |
| AICA | Runaway task |
| AICC | Storage violation |
| AICE | Transaction timeout |
| AEI0 | EXEC CICS error |
| AEI9 | User-requested abend |

### AbendManager

```cpp
class AbendManager {
public:
    static AbendManager& instance();
    
    void initialize();
    void shutdown();
    
    // Handler registration
    Result<void> handle_abend_program(StringView program);
    Result<void> handle_abend_callback(AbendCallback callback);
    Result<void> handle_abend_cancel();
    Result<void> handle_abend_reset();
    
    // Stack operations
    Result<void> push_handler();
    Result<void> pop_handler();
    
    // Abend execution
    [[noreturn]] void abend(StringView code);
    [[noreturn]] void abend(StringView code, bool nodump);
};
```

### EXEC CICS Functions

```cpp
Result<void> exec_cics_handle_abend_program(StringView program);
Result<void> exec_cics_handle_abend_cancel();
Result<void> exec_cics_push_handle();
Result<void> exec_cics_pop_handle();
[[noreturn]] void exec_cics_abend(StringView code);
[[noreturn]] void exec_cics_abend_nodump(StringView code);
```

---

## Channel/Container Module

### Header
```cpp
#include <cics/channel/channel.hpp>
```

### ChannelManager

```cpp
class ChannelManager {
public:
    static ChannelManager& instance();
    
    void initialize();
    void shutdown();
    
    // Channel operations
    Result<Channel*> create_channel(StringView name);
    Result<Channel*> get_channel(StringView name);
    Result<void> delete_channel(StringView name);
    
    // Container operations
    Result<void> put_container(StringView container, const void* data, UInt32 length);
    Result<void> put_container(StringView container, StringView channel, 
                                const void* data, UInt32 length);
    Result<ByteBuffer> get_container(StringView container);
    Result<ByteBuffer> get_container(StringView container, StringView channel);
    Result<void> delete_container(StringView container);
};
```

### EXEC CICS Functions

```cpp
// PUT CONTAINER
Result<void> exec_cics_put_container(StringView container, const void* data, UInt32 length);
Result<void> exec_cics_put_container(StringView container, StringView channel,
                                      const void* data, UInt32 length);

// GET CONTAINER
Result<ByteBuffer> exec_cics_get_container(StringView container);
Result<ByteBuffer> exec_cics_get_container(StringView container, StringView channel);

// DELETE CONTAINER
Result<void> exec_cics_delete_container(StringView container);
Result<void> exec_cics_delete_container(StringView container, StringView channel);

// CHANNEL
Result<void> exec_cics_create_channel(StringView channel);
Result<void> exec_cics_delete_channel(StringView channel);
```

---

## Terminal Control Module

### Header
```cpp
#include <cics/terminal/terminal.hpp>
```

### AID Keys

```cpp
enum class AIDKey : UInt8 {
    ENTER = 0x7D,
    CLEAR = 0x6D,
    PA1 = 0x6C, PA2 = 0x6E, PA3 = 0x6B,
    PF1 = 0xF1, PF2 = 0xF2, ... PF24 = 0x4C
};
```

### SendOptions

```cpp
struct SendOptions {
    EraseOption erase = EraseOption::NONE;
    CursorOption cursor = CursorOption::NONE;
    UInt16 cursor_row = 1;
    UInt16 cursor_col = 1;
    bool freekb = false;
    bool alarm = false;
};
```

### TerminalManager

```cpp
class TerminalManager {
public:
    static TerminalManager& instance();
    
    void initialize();
    void shutdown();
    
    // Session management
    Result<TerminalSession*> create_session(StringView terminal_id, TerminalType type);
    Result<TerminalSession*> get_session(StringView terminal_id);
    Result<void> close_session(StringView terminal_id);
    
    // Operations
    Result<void> send(StringView text, const SendOptions& opts = {});
    Result<TerminalInput> receive(const ReceiveOptions& opts = {});
};
```

### EXEC CICS Functions

```cpp
// SEND
Result<void> exec_cics_send_text(StringView text);
Result<void> exec_cics_send_text_erase(StringView text);
Result<void> exec_cics_send_control_freekb();

// RECEIVE
Result<TerminalInput> exec_cics_receive();
Result<String> exec_cics_receive_text();

// CONVERSE
Result<TerminalInput> exec_cics_converse(StringView text);
```

---

## Spool Control Module

### Header
```cpp
#include <cics/spool/spool.hpp>
```

### SpoolAttributes

```cpp
struct SpoolAttributes {
    String name;
    SpoolType type = SpoolType::OUTPUT;
    SpoolClass spool_class = SpoolClass::A;
    SpoolDisposition disposition = SpoolDisposition::KEEP;
    UInt32 record_length = 80;
    UInt32 copies = 1;
};
```

### SpoolManager

```cpp
class SpoolManager {
public:
    static SpoolManager& instance();
    
    void initialize();
    void shutdown();
    
    void set_spool_directory(StringView dir);
    
    Result<String> open(const SpoolAttributes& attrs);
    Result<void> close(StringView token);
    Result<void> close(StringView token, SpoolDisposition disp);
    
    Result<void> write(StringView token, StringView data);
    Result<void> write_line(StringView token, StringView line);
    Result<ByteBuffer> read(StringView token);
    Result<String> read_line(StringView token);
};
```

### EXEC CICS Functions

```cpp
// SPOOLOPEN
Result<String> exec_cics_spoolopen_output(StringView name);
Result<String> exec_cics_spoolopen_output(StringView name, SpoolClass spool_class);
Result<String> exec_cics_spoolopen_input(StringView name);

// SPOOLWRITE
Result<void> exec_cics_spoolwrite(StringView token, StringView data);
Result<void> exec_cics_spoolwrite_line(StringView token, StringView line);

// SPOOLREAD
Result<ByteBuffer> exec_cics_spoolread(StringView token);
Result<String> exec_cics_spoolread_line(StringView token);

// SPOOLCLOSE
Result<void> exec_cics_spoolclose(StringView token);
Result<void> exec_cics_spoolclose(StringView token, SpoolDisposition disp);
```
