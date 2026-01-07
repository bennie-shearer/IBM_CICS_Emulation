# IBM CICS (Customer Information Control System) Emulation - Testing Guide
Version 3.4.6

## Running Tests

### All Tests
```bash
cd build
ctest --output-on-failure
```

### Verbose Output
```bash
ctest -V
```

### Specific Test
```bash
ctest -R test_types
```

### Parallel Execution
```bash
ctest -j4
```

## Test Categories

### Unit Tests
Located in `tests/unit/`

- `test_types` - Core type system
- `test_error` - Error handling
- `test_vsam` - VSAM operations
- `test_cics` - CICS types

### Integration Tests
Located in `tests/integration/`

- `test_vsam_integration` - Full VSAM workflows

### Benchmarks
Located in `tests/benchmarks/`

```bash
# Enable benchmarks
cmake .. -DCICS_BUILD_BENCHMARKS=ON

# Run
./bin/benchmark-vsam
```

## Test Framework

Custom lightweight framework in `tests/framework/test_framework.hpp`.

### Writing Tests

```cpp
#include "../framework/test_framework.hpp"
using namespace cics::test;

void test_example() {
    ASSERT_TRUE(condition);
    ASSERT_FALSE(condition);
    ASSERT_EQ(a, b);
    ASSERT_NE(a, b);
    ASSERT_LT(a, b);
    ASSERT_LE(a, b);
    ASSERT_GT(a, b);
    ASSERT_GE(a, b);
    ASSERT_THROW(expr, ExceptionType);
    ASSERT_NO_THROW(expr);
}

int main() {
    TestSuite suite("My Tests");
    suite.add_test("Example", test_example);
    
    TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
```

## Code Coverage

```bash
cmake .. -DCICS_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest
# Generate coverage report (requires lcov/gcov)
```
