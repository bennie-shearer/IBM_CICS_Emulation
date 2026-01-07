#include "../framework/test_framework.hpp"
#include "cics/common/error.hpp"

using namespace cics;
using namespace cics::test;

void test_error_code() {
    auto ec = make_error_code(ErrorCode::FILE_NOT_FOUND);
    ASSERT_EQ(ec.value(), static_cast<int>(ErrorCode::FILE_NOT_FOUND));
    ASSERT_EQ(ec.category().name(), std::string("cics"));
}

void test_result_success() {
    Result<int> result = make_success(42);
    ASSERT_TRUE(result.is_success());
    ASSERT_FALSE(result.is_error());
    ASSERT_EQ(result.value(), 42);
    ASSERT_EQ(*result, 42);
}

void test_result_error() {
    Result<int> result = make_error<int>(ErrorCode::FILE_NOT_FOUND, "Not found");
    ASSERT_FALSE(result.is_success());
    ASSERT_TRUE(result.is_error());
    ASSERT_EQ(result.error().code, ErrorCode::FILE_NOT_FOUND);
}

void test_result_value_or() {
    Result<int> success = make_success(42);
    ASSERT_EQ(success.value_or(0), 42);
    
    Result<int> error = make_error<int>(ErrorCode::UNKNOWN_ERROR, "Error");
    ASSERT_EQ(error.value_or(99), 99);
}

void test_result_map() {
    Result<int> result = make_success(10);
    Result<int> mapped = result.map([](int x) { return x * 2; });
    ASSERT_EQ(mapped.value(), 20);
}

void test_result_and_then() {
    Result<int> result = make_success(10);
    Result<String> chained = result.and_then([](int x) { 
        return make_success(std::to_string(x)); 
    });
    ASSERT_EQ(chained.value(), "10");
}

void test_result_void() {
    Result<void> success = make_success();
    ASSERT_TRUE(success.is_success());
    
    Result<void> error = make_error<void>(ErrorCode::IO_ERROR, "IO Error");
    ASSERT_TRUE(error.is_error());
}

void test_error_info() {
    ErrorInfo info(ErrorCode::FILE_NOT_FOUND, "File missing", "FileManager");
    info.with_context("filename", "test.dat");
    
    ASSERT_EQ(info.code, ErrorCode::FILE_NOT_FOUND);
    ASSERT_EQ(info.component, "FileManager");
    ASSERT_TRUE(info.context.contains("filename"));
    
    String str = info.to_string();
    ASSERT_FALSE(str.empty());
    
    String json = info.to_json();
    ASSERT_TRUE(json.find("FILE_NOT_FOUND") != String::npos || json.find("1101") != String::npos);
}

void test_cics_exception() {
    CicsException ex(ErrorCode::VSAM_ERROR, "VSAM failure");
    ASSERT_EQ(ex.code(), ErrorCode::VSAM_ERROR);
    
    String msg = ex.detailed_message();
    ASSERT_FALSE(msg.empty());
}

void test_error_statistics() {
    ErrorStatistics::instance().reset();
    
    ErrorInfo info(ErrorCode::IO_ERROR, "Test error", "Test");
    ErrorStatistics::instance().record_error(info);
    
    ASSERT_EQ(ErrorStatistics::instance().get_error_count(ErrorCode::IO_ERROR), 1u);
    ASSERT_GE(ErrorStatistics::instance().total_errors(), 1u);
}

int main() {
    TestSuite suite("Error Handling Tests");
    
    suite.add_test("ErrorCode", test_error_code);
    suite.add_test("Result Success", test_result_success);
    suite.add_test("Result Error", test_result_error);
    suite.add_test("Result value_or", test_result_value_or);
    suite.add_test("Result map", test_result_map);
    suite.add_test("Result and_then", test_result_and_then);
    suite.add_test("Result<void>", test_result_void);
    suite.add_test("ErrorInfo", test_error_info);
    suite.add_test("CicsException", test_cics_exception);
    suite.add_test("ErrorStatistics", test_error_statistics);
    
    TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
