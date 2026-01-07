#include "../framework/test_framework.hpp"
#include "cics/common/types.hpp"

using namespace cics;
using namespace cics::test;

void test_string_utilities() {
    ASSERT_EQ(to_upper("hello"), "HELLO");
    ASSERT_EQ(to_lower("HELLO"), "hello");
    ASSERT_EQ(trim("  hello  "), "hello");
    ASSERT_EQ(trim_left("  hello"), "hello");
    ASSERT_EQ(trim_right("hello  "), "hello");
    
    auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    ASSERT_EQ(parts[0], "a");
    ASSERT_EQ(parts[1], "b");
    ASSERT_EQ(parts[2], "c");
    
    ASSERT_EQ(join({"a", "b", "c"}, "-"), "a-b-c");
    ASSERT_TRUE(starts_with("hello", "hel"));
    ASSERT_TRUE(ends_with("hello", "llo"));
    ASSERT_TRUE(contains("hello", "ell"));
    
    ASSERT_EQ(replace_all("aaa", "a", "b"), "bbb");
    ASSERT_EQ(pad_left("42", 5, '0'), "00042");
    ASSERT_EQ(pad_right("42", 5, ' '), "42   ");
}

void test_fixed_string() {
    FixedString<8> fs1("HELLO");
    ASSERT_EQ(fs1.str(), "HELLO   ");
    ASSERT_EQ(fs1.trimmed(), "HELLO");
    
    FixedString<8> fs2("HELLO   ");
    ASSERT_TRUE(fs1 == fs2);
    
    FixedString<4> fs3("AB");
    fs3[2] = 'C';
    ASSERT_EQ(fs3[2], 'C');
}

void test_uuid() {
    UUID uuid1 = UUID::generate();
    UUID uuid2 = UUID::generate();
    
    ASSERT_FALSE(uuid1.is_nil());
    ASSERT_NE(uuid1.to_string(), uuid2.to_string());
    ASSERT_EQ(uuid1.to_string().length(), 36u);
    
    UUID nil{};
    ASSERT_TRUE(nil.is_nil());
}

void test_ebcdic_conversion() {
    String ascii = "HELLO";
    EbcdicString ebcdic = ascii_to_ebcdic(ascii);
    ASSERT_EQ(ebcdic.size(), 5u);
    
    String back = ebcdic_to_ascii(ConstByteSpan(ebcdic.data(), ebcdic.size()));
    ASSERT_EQ(back, ascii);
}

void test_packed_decimal() {
    PackedDecimal pd;
    ASSERT_TRUE(pd.from_string("12345"));
    ASSERT_EQ(pd.to_int64(), 12345);
    
    PackedDecimal neg;
    ASSERT_TRUE(neg.from_string("-999"));
    ASSERT_TRUE(neg.is_negative());
    ASSERT_EQ(neg.to_int64(), -999);
    
    PackedDecimal zero;
    ASSERT_TRUE(zero.from_string("0"));
    ASSERT_TRUE(zero.is_zero());
}

void test_hash_functions() {
    String data = "test data";
    ConstByteSpan span(reinterpret_cast<const Byte*>(data.data()), data.size());
    
    UInt32 crc = crc32(span);
    ASSERT_NE(crc, 0u);
    
    UInt64 fnv = fnv1a_hash(span);
    ASSERT_NE(fnv, 0u);
    
    String hex = to_hex_string(span);
    ASSERT_EQ(hex.length(), data.size() * 2);
    
    ByteBuffer back = from_hex_string(hex);
    ASSERT_EQ(back.size(), span.size());
}

void test_version() {
    Version v = Version::parse("3.4.6");
    ASSERT_EQ(v.major, 3);
    ASSERT_EQ(v.minor, 4);
    ASSERT_EQ(v.patch, 6);
    
    Version v2{3, 5, 0, ""};
    ASSERT_TRUE(v < v2);
    ASSERT_EQ(v.to_string(), "3.4.6");
}

void test_atomic_counter() {
    AtomicCounter<> counter;
    ASSERT_EQ(counter.get(), 0u);
    
    counter++;
    ASSERT_EQ(counter.get(), 1u);
    
    counter += 5;
    ASSERT_EQ(counter.get(), 6u);
    
    counter--;
    ASSERT_EQ(counter.get(), 5u);
}

void test_buffer_view() {
    ByteBuffer buf = {0x01, 0x02, 0x03, 0x04, 0x05};
    BufferView view(buf);
    
    ASSERT_EQ(view.size(), 5u);
    ASSERT_EQ(view[0], 0x01);
    
    BufferView sub = view.subview(1, 3);
    ASSERT_EQ(sub.size(), 3u);
    ASSERT_EQ(sub[0], 0x02);
}

int main() {
    TestSuite suite("Types Tests");
    
    suite.add_test("String Utilities", test_string_utilities);
    suite.add_test("FixedString", test_fixed_string);
    suite.add_test("UUID", test_uuid);
    suite.add_test("EBCDIC Conversion", test_ebcdic_conversion);
    suite.add_test("Packed Decimal", test_packed_decimal);
    suite.add_test("Hash Functions", test_hash_functions);
    suite.add_test("Version", test_version);
    suite.add_test("AtomicCounter", test_atomic_counter);
    suite.add_test("BufferView", test_buffer_view);
    
    TestRunner runner;
    runner.add_suite(&suite);
    return runner.run_all();
}
