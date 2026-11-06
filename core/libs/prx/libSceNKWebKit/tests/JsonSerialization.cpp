#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JSON serialization check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;

std::u16string Text(const String& string) {
    const auto* impl = string.impl;
    Require(impl != nullptr);
    std::u16string result;
    if (impl->hashAndFlags & 4) {
        const auto* characters = static_cast<const unsigned char*>(impl->data);
        for (unsigned i = 0; i < impl->length; ++i) result += static_cast<char16_t>(characters[i]);
    } else {
        result.assign(static_cast<const char16_t*>(impl->data), impl->length);
    }
    return result;
}

std::u16string Wide(const char* text) {
    std::u16string result;
    for (; *text; ++text) result += static_cast<char16_t>(static_cast<unsigned char>(*text));
    return result;
}

String Latin1(const char* text, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), length);
}

std::u16string Json(const Value* value) {
    return Text(_ZNK3WTF8JSONImpl5Value12toJSONStringEv(value));
}

void RequireNumber(double number, const char* expected) {
    auto value = _ZN3WTF8JSONImpl5Value6createEd(number);
    Require(Json(value.Get()) == Wide(expected));
}

void TestScalars() {
    Require(Json(_ZN3WTF8JSONImpl5Value4nullEv().Get()) == u"null");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEb(true).Get()) == u"true");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEb(false).Get()) == u"false");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEi(42).Get()) == u"42");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEi(-2147483647 - 1).Get()) == u"-2147483648");
    RequireNumber(0.0, "0");
    RequireNumber(-0.0, "0");
    RequireNumber(0.5, "0.5");
    RequireNumber(-2.25, "-2.25");
    RequireNumber(0.1, "0.1");
    RequireNumber(1.0 / 3.0, "0.3333333333333333");
    RequireNumber(123456789012.0, "123456789012");
    RequireNumber(1e20, "100000000000000000000");
    RequireNumber(1e21, "1e+21");
    RequireNumber(1.5e-7, "1.5e-7");
    RequireNumber(0.000001, "0.000001");
    RequireNumber(std::numeric_limits<double>::max(), "1.7976931348623157e+308");
    RequireNumber(std::numeric_limits<double>::denorm_min(), "5e-324");
    RequireNumber(std::numeric_limits<double>::quiet_NaN(), "null");
    RequireNumber(std::numeric_limits<double>::infinity(), "null");
    RequireNumber(-std::numeric_limits<double>::infinity(), "null");
    std::printf("JSON serialization: scalars passed\n");
}

void TestStrings() {
    auto plain = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("plain", 5));
    Require(Json(plain.Get()) == u"\"plain\"");
    auto escaped = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("a\"b\\c\nd\te\x01", 10));
    Require(Json(escaped.Get()) == u"\"a\\\"b\\\\c\\nd\\te\\u0001\"");
    auto empty = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("", 0));
    Require(Json(empty.Get()) == u"\"\"");
    const char16_t wide[] = {u'é', u'中', u'"'};
    auto utf16 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(_ZN3WTF10StringImpl6createEPKDsj(wide, 3));
    Require(Json(utf16.Get()) == u"\"é中\\\"\"");
    auto shared = Latin1("shared", 6);
    const auto before = shared.impl->refCount;
    auto value = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(shared);
    const auto held = shared.impl->refCount;
    Require(held > before);
    Json(value.Get());
    Require(shared.impl->refCount == held);
    std::printf("JSON serialization: strings passed\n");
}

void TestArrays() {
    auto empty = _ZN3WTF8JSONImpl5Array6createEv();
    Require(Json(empty.Get()) == u"[]");
    auto array = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(array.Get(), true);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(array.Get(), 7);
    _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(array.Get(), 2.5);
    _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(array.Get(), std::numeric_limits<double>::quiet_NaN());
    _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(array.Get(), Latin1("x\"y", 3));
    Require(Json(array.Get()) == u"[true,7,2.5,null,\"x\\\"y\"]");
    std::u16string large = u"[";
    auto many = _ZN3WTF8JSONImpl5Array6createEv();
    for (int i = 0; i < 300; ++i) {
        _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(many.Get(), i * 1000);
        if (i) large += u',';
        large += Wide(std::to_string(i * 1000).c_str());
    }
    large += u']';
    Require(Json(many.Get()) == large);
    std::printf("JSON serialization: arrays passed\n");
}

void TestAppendsToBuilder() {
    StringBuilder builder{};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, reinterpret_cast<const unsigned char*>("pre:"), 4);
    auto value = _ZN3WTF8JSONImpl5Value6createEi(5);
    _ZNK3WTF8JSONImpl5Value9writeJSONERNS_13StringBuilderE(value.Get(), builder);
    auto array = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(array.Get(), false);
    _ZNK3WTF8JSONImpl9ArrayBase9writeJSONERNS_13StringBuilderE(array.Get(), builder);
    Require(Text(NKWebKit::WtfAbi::ToString(builder)) == u"pre:5[false]");
    NKWebKit::WtfAbi::Clear(builder);
    std::printf("JSON serialization: appends to an existing builder passed\n");
}

void TestInvalidArguments() {
    StringBuilder builder{};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    RequireThrows([] { _ZNK3WTF8JSONImpl5Value12toJSONStringEv(nullptr); });
    RequireThrows([&] { _ZNK3WTF8JSONImpl5Value9writeJSONERNS_13StringBuilderE(nullptr, builder); });
    RequireThrows([&] { _ZNK3WTF8JSONImpl9ArrayBase9writeJSONERNS_13StringBuilderE(nullptr, builder); });
    Value object{};
    _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(&object, ValueType::Object);
    RequireThrows([&] { _ZNK3WTF8JSONImpl5Value9writeJSONERNS_13StringBuilderE(&object, builder); });
    RequireThrows([&] { _ZNK3WTF8JSONImpl5Value12toJSONStringEv(&object); });
    _ZN3WTF8JSONImpl5ValueD1Ev(&object);
    Require(builder.length == 0);
    NKWebKit::WtfAbi::Clear(builder);
    std::printf("JSON serialization: invalid arguments throw\n");
}

}

int main() {
    TestScalars();
    TestStrings();
    TestArrays();
    TestAppendsToBuilder();
    TestInvalidArguments();
}
