#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JSON object check failed at line %u\n", location.line());
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

using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::ObjectBase;
using NKWebKit::JsonAbi::ObjectSetValue;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;

String Latin1(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

std::string Narrow(const String& string) {
    Require(string.impl != nullptr);
    Require((string.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

std::string Json(const Value* value) {
    return Narrow(_ZNK3WTF8JSONImpl5Value12toJSONStringEv(value));
}

const unsigned* Metadata(const ObjectBase& object) {
    return reinterpret_cast<const unsigned*>(object.map.table);
}

unsigned TableSize(const ObjectBase& object) { return object.map.table ? Metadata(object)[-1] : 0; }
unsigned KeyCount(const ObjectBase& object) { return object.map.table ? Metadata(object)[-3] : 0; }
unsigned DeletedCount(const ObjectBase& object) { return object.map.table ? Metadata(object)[-4] : 0; }

void Set(Object* object, const std::string& key, Ref<Value>&& value) {
    ObjectSetValue(object, Latin1(key), std::move(value));
}

bool GetString(const Object* object, const std::string& key, std::string& output) {
    String result;
    if (!_ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(object, Latin1(key), result)) return false;
    output = Narrow(result);
    return true;
}

void Remove(Object* object, const std::string& key) {
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object, Latin1(key));
}

void TestCreate() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    Require(object->valueType == ValueType::Object && object->refCount == 1);
    Require(object->map.table == nullptr && object->order.buffer == nullptr && object->order.size == 0);
    Require(Json(object.Get()) == "{}");
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == sizeof(Value*));
    auto asObject = object->vptr->asObject(object.Get());
    Require(asObject.Get() == object.Get() && object->refCount == 2);
    Require(!object->vptr->asArray(object.Get()));
    std::printf("JSON object: create passed\n");
}

void TestSetGetAndWrite() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    Set(object.Get(), "a", _ZN3WTF8JSONImpl5Value6createEi(1));
    Set(object.Get(), "b", _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("x")));
    Set(object.Get(), "c", _ZN3WTF8JSONImpl5Value6createEb(true));
    Require(TableSize(*object) == 8 && KeyCount(*object) == 3 && DeletedCount(*object) == 0);
    Require(Json(object.Get()) == "{\"a\":1,\"b\":\"x\",\"c\":true}");
    std::string text;
    Require(GetString(object.Get(), "b", text) && text == "x");
    Require(!GetString(object.Get(), "a", text));
    Require(!GetString(object.Get(), "missing", text));
    Set(object.Get(), "a", _ZN3WTF8JSONImpl5Value6createEd(2.5));
    Require(KeyCount(*object) == 3 && object->order.size == 3);
    Require(Json(object.Get()) == "{\"a\":2.5,\"b\":\"x\",\"c\":true}");
    const std::size_t expected = sizeof(Value*) + 3 * (sizeof(NKWebKit::WtfAbi::StringImpl) + 1)
        + 3 * sizeof(Value*) + sizeof(NKWebKit::WtfAbi::StringImpl) + 1;
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == expected);
    std::printf("JSON object: set, getString, overwrite, memoryCost and writeJSON passed\n");
}

void TestGrowAndShrink() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    constexpr int count = 1000;
    for (int i = 0; i < count; ++i) Set(object.Get(), "key" + std::to_string(i), _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("v" + std::to_string(i))));
    Require(KeyCount(*object) == count && TableSize(*object) == 2048 && object->order.size == count);
    std::string text;
    for (int i = 0; i < count; ++i) Require(GetString(object.Get(), "key" + std::to_string(i), text) && text == "v" + std::to_string(i));
    std::string expected = "{";
    for (int i = 0; i < count; ++i) expected += (i ? ",\"key" : "\"key") + std::to_string(i) + "\":\"v" + std::to_string(i) + "\"";
    Require(Json(object.Get()) == expected + "}");
    for (int i = 0; i < count; ++i) {
        if (i % 20) Remove(object.Get(), "key" + std::to_string(i));
    }
    Require(KeyCount(*object) == 50 && object->order.size == 50);
    Require(TableSize(*object) < 2048 && TableSize(*object) >= 8);
    Require(KeyCount(*object) * 6 >= TableSize(*object) || TableSize(*object) == 8);
    for (int i = 0; i < count; ++i) Require(GetString(object.Get(), "key" + std::to_string(i), text) == (i % 20 == 0));
    expected = "{";
    for (int i = 0; i < count; i += 20) expected += (i ? ",\"key" : "\"key") + std::to_string(i) + "\":\"v" + std::to_string(i) + "\"";
    Require(Json(object.Get()) == expected + "}");
    Remove(object.Get(), "not there");
    Require(KeyCount(*object) == 50);
    for (int i = 0; i < count; i += 20) Remove(object.Get(), "key" + std::to_string(i));
    Require(KeyCount(*object) == 0 && object->order.size == 0 && TableSize(*object) == 8);
    Require(Json(object.Get()) == "{}");
    Set(object.Get(), "again", _ZN3WTF8JSONImpl5Value4nullEv());
    Require(Json(object.Get()) == "{\"again\":null}");
    std::printf("JSON object: 1000 keys grew to 2048 buckets, shrank on removal and kept their order\n");
}

void TestNestingAndReferences() {
    auto key = Latin1("shared");
    const unsigned keyRefs = key.impl->refCount;
    auto inner = _ZN3WTF8JSONImpl6Object6createEv();
    Set(inner.Get(), "k", _ZN3WTF8JSONImpl5Value4nullEv());
    auto array = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(array.Get(), 1);
    {
        auto outer = _ZN3WTF8JSONImpl6Object6createEv();
        ObjectSetValue(outer.Get(), key, Ref<Value>(inner));
        Set(outer.Get(), "arr", Ref<Value>(array));
        Require(key.impl->refCount > keyRefs && inner->refCount == 2 && array->refCount == 2);
        Require(Json(outer.Get()) == "{\"shared\":{\"k\":null},\"arr\":[1]}");
    }
    Require(key.impl->refCount == keyRefs && inner->refCount == 1 && array->refCount == 1);
    std::printf("JSON object: nested values, key and value references released on destruction\n");
}

void TestInvalidArguments() {
    String output;
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    RequireThrows([&] { _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(nullptr, Latin1("a")); });
    RequireThrows([&] { _ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(nullptr, Latin1("a"), output); });
    RequireThrows([&] { _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(nullptr); });
    RequireThrows([&] { Remove(object.Get(), std::string()); Remove(object.Get(), "x"); _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), String()); });
    RequireThrows([&] { ObjectSetValue(object.Get(), String(), _ZN3WTF8JSONImpl5Value4nullEv()); });
    auto array = _ZN3WTF8JSONImpl5Array6createEv();
    RequireThrows([&] { _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(reinterpret_cast<const ObjectBase*>(array.Get())); });
    Require(output.impl == nullptr && KeyCount(*object) == 0);
    std::printf("JSON object: invalid arguments throw\n");
}

}

int main() {
    TestCreate();
    TestSetGetAndWrite();
    TestGrowAndShrink();
    TestNestingAndReferences();
    TestInvalidArguments();
}
