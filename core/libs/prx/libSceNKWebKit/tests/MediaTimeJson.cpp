#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "MediaTime JSON check failed at line %u\n", location.line());
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

using WTF::MediaTime;
using NKWebKit::WtfAbi::String;

std::string Narrow(const String& string) {
    Require(string.impl != nullptr && (string.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

MediaTime Rational(std::int64_t value, std::uint32_t scale) {
    return MediaTime(value, scale, MediaTime::Valid);
}

MediaTime Double(double value) {
    MediaTime time(0, MediaTime::DefaultTimeScale, MediaTime::Valid | MediaTime::DoubleValue);
    time.m_timeValueAsDouble = value;
    return time;
}

std::string ToString(const MediaTime& time) { return Narrow(_ZNK3WTF9MediaTime8toStringEv(&time)); }
std::string ToJson(const MediaTime& time) { return Narrow(_ZNK3WTF9MediaTime12toJSONStringEv(&time)); }

void TestToString() {
    Require(ToString(Rational(1, 2)) == "{1/2 = 0.5}");
    Require(ToString(Rational(-3, 4)) == "{-3/4 = -0.75}");
    Require(ToString(Rational(30000, 1001)) == "{30000/1001 = 29.97002997002997}");
    Require(ToString(Double(1.25)) == "{1.25}");
    Require(ToString(_ZN3WTF9MediaTime11invalidTimeEv()) == "{-1/1 = NaN, invalid}");
    Require(ToString(_ZN3WTF9MediaTime20positiveInfiniteTimeEv()) == "{0/1 = Infinity}");
    Require(ToString(_ZN3WTF9MediaTime20negativeInfiniteTimeEv()) == "{-1/1 = -Infinity}");
    Require(ToString(_ZN3WTF9MediaTime14indefiniteTimeEv()) == "{0/1 = NaN}");
    std::printf("MediaTime::toString passed\n");
}

void TestToJson() {
    Require(ToJson(Rational(1, 2)) == "{\"value\":0.5,\"numerator\":1,\"denominator\":2,\"flags\":1}");
    Require(ToJson(Double(1.25)) == "{\"value\":1.25}");
    Require(ToJson(_ZN3WTF9MediaTime11invalidTimeEv()) == "{\"invalid\":true,\"numerator\":-1,\"denominator\":1,\"flags\":0}");
    Require(ToJson(_ZN3WTF9MediaTime14indefiniteTimeEv()) == "{\"value\":\"NaN\",\"numerator\":0,\"denominator\":1,\"flags\":17}");
    Require(ToJson(_ZN3WTF9MediaTime20positiveInfiniteTimeEv()) == "{\"value\":\"POSITIVE_INFINITY\",\"numerator\":0,\"denominator\":1,\"flags\":5}");
    Require(ToJson(_ZN3WTF9MediaTime20negativeInfiniteTimeEv()) == "{\"value\":\"NEGATIVE_INFINITY\",\"numerator\":-1,\"denominator\":1,\"flags\":9}");
    auto object = _ZNK3WTF9MediaTime12toJSONObjectEv(&static_cast<const MediaTime&>(Rational(3, 1)));
    Require(object->refCount == 1 && object->valueType == NKWebKit::JsonAbi::ValueType::Object);
    std::printf("MediaTime::toJSONObject and toJSONString passed\n");
}

void TestRange() {
    WTF::MediaTimeRange range{Rational(1, 2), Double(2.0)};
    Require(Narrow(_ZNK3WTF14MediaTimeRange12toJSONStringEv(&range))
        == "{\"start\":{\"value\":0.5,\"numerator\":1,\"denominator\":2,\"flags\":1},\"end\":{\"value\":2}}");
    std::printf("MediaTimeRange::toJSONString passed\n");
}

void TestInvalidArguments() {
    RequireThrows([] { _ZNK3WTF9MediaTime8toStringEv(nullptr); });
    RequireThrows([] { _ZNK3WTF9MediaTime12toJSONObjectEv(nullptr); });
    RequireThrows([] { _ZNK3WTF9MediaTime12toJSONStringEv(nullptr); });
    RequireThrows([] { _ZNK3WTF14MediaTimeRange12toJSONStringEv(nullptr); });
    std::printf("MediaTime JSON: invalid arguments throw\n");
}

}

int main() {
    TestToString();
    TestToJson();
    TestRange();
    TestInvalidArguments();
}
