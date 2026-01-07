# IBM CICS (Customer Information Control System) Emulation - API Reference
Version 3.4.6

## Core Types (cics/common/types.hpp)

### Type Aliases

```cpp
using Byte = std::uint8_t;
using Int8 = std::int8_t;
using UInt8 = std::uint8_t;
using Int16 = std::int16_t;
using UInt16 = std::uint16_t;
using Int32 = std::int32_t;
using UInt32 = std::uint32_t;
using Int64 = std::int64_t;
using UInt64 = std::uint64_t;
using Float32 = float;
using Float64 = double;
using Size = std::size_t;
using String = std::string;
using StringView = std::string_view;
using ByteBuffer = std::vector<Byte>;
using ByteSpan = std::span<Byte>;
using ConstByteSpan = std::span<const Byte>;
```

### FixedString<N>

Fixed-length string for mainframe compatibility.

```cpp
FixedString<8> field("VALUE");
field.str();      // "VALUE   " (padded)
field.trimmed();  // "VALUE"
field[0];         // 'V'
field.length();   // 8
```

### UUID

RFC 4122 UUID generation.

```cpp
UUID id = UUID::generate();
String str = id.to_string();  // "550e8400-e29b-41d4-a716-446655440000"
bool nil = id.is_nil();
```

### AtomicCounter<T>

Thread-safe counter.

```cpp
AtomicCounter<> counter;
counter++;
counter += 5;
UInt64 val = counter.get();
```

## Error Handling (cics/common/error.hpp)

### Result<T>

Monadic error handling.

```cpp
Result<int> success = make_success(42);
Result<int> error = make_error<int>(ErrorCode::NOT_FOUND, "Missing");

if (success.is_success()) {
    int val = success.value();
    int val2 = *success;
}

int fallback = error.value_or(0);

auto mapped = success.map([](int x) { return x * 2; });
auto chained = success.and_then([](int x) { return make_success(x + 1); });
```

### ErrorInfo

Detailed error information.

```cpp
ErrorInfo info(ErrorCode::IO_ERROR, "Read failed", "FileSystem");
info.with_context("path", "/data/file.dat");
String str = info.to_string();
String json = info.to_json();
```

## VSAM (cics/vsam/vsam_types.hpp)

### VsamKey

VSAM record key.

```cpp
VsamKey key("CUSTOMER01");
Size len = key.length();
bool match = key.starts_with(VsamKey("CUST"));
String hex = key.to_hex();
```

### VsamRecord

VSAM data record.

```cpp
VsamRecord rec(key, data_span);
ConstByteSpan data = rec.span();
Size len = rec.length();
VsamAddress addr = rec.address();
```

### IVsamFile Interface

```cpp
Result<void> open(AccessMode mode, ProcessingMode proc);
Result<void> close();
Result<VsamRecord> read(const VsamKey& key);
Result<void> write(const VsamRecord& record);
Result<void> update(const VsamRecord& record);
Result<void> erase(const VsamKey& key);
Result<String> start_browse(const VsamKey& key, bool gteq, bool backward);
Result<VsamRecord> read_next(const String& browse_id);
Result<void> end_browse(const String& browse_id);
```

## CICS (cics/cics/cics_types.hpp)

### EIB (Execute Interface Block)

```cpp
EIB eib;
eib.reset();
eib.set_time_date();
eib.eibtrnid = FixedString<4>("MENU");
eib.eibresp = CicsResponse::NORMAL;
```

### Commarea

```cpp
Commarea comm(500);
comm.set_string(0, "DATA", 10);
String str = comm.get_string(0, 10);
comm.set_value<UInt32>(20, 12345);
UInt32 val = comm.get_value<UInt32>(20);
```

### CicsTask

```cpp
CicsTask task(1001, "MENU", "TERM01");
task.set_status(TransactionStatus::RUNNING);
TransactionStatus status = task.status();
```

## Logging (cics/common/logging.hpp)

### Logger

```cpp
auto logger = LogManager::instance().get_logger("MyApp");
logger->set_level(LogLevel::DEBUG);
logger->info("Message with {} parameters", 42);
logger->error("Error occurred: {}", error_msg);
```

### Sinks

```cpp
auto console = make_shared<ConsoleSink>(LogLevel::INFO, true);
auto file = make_shared<FileSink>("app.log", LogLevel::DEBUG, 10*1024*1024, 5);
auto async = make_shared<AsyncSink>(std::move(file));
logger->add_sink(console);
```

## Threading (cics/common/threading.hpp)

### ThreadPool

```cpp
ThreadPoolConfig config;
config.min_threads = 2;
config.max_threads = 8;
ThreadPool pool(config);

pool.execute([]() { /* task */ }, TaskPriority::NORMAL);
pool.wait_all();
pool.shutdown();
```

### ConcurrentQueue

```cpp
ConcurrentQueue<int> queue;
queue.push(42);
int val;
if (queue.try_pop(val)) { /* use val */ }
auto opt = queue.wait_pop(Seconds(5));
```
