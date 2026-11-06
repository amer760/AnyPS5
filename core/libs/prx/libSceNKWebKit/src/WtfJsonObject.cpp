#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Deref;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::ObjectBase;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::StringBuilder;
using NKWebKit::JsonAbi::StringValuePair;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::JsonAbi::ValueVtableStorage;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

struct Bucket {
    StringImpl* key;
    Value* value;
};

static_assert(sizeof(Bucket) == sizeof(StringValuePair));
static_assert(sizeof(String) == sizeof(StringImpl*));

constexpr std::size_t kMetadataSize = 4 * sizeof(unsigned);
constexpr unsigned kMinimumTableSize = 8;
constexpr unsigned kMaxSmallTableCapacity = 1024;
constexpr unsigned kMinLoad = 6;
constexpr unsigned kMinimumOrderCapacity = 16;

StringImpl* DeletedKey() {
    return reinterpret_cast<StringImpl*>(std::uintptr_t(-1));
}

template<typename T>
T& Checked(T* ptr) {
    if (!ptr) throw std::invalid_argument("null WTF JSON object");
    return *ptr;
}

ObjectBase& CheckedObject(const ObjectBase* self) {
    auto& object = const_cast<ObjectBase&>(Checked(self));
    if (object.valueType != ValueType::Object) throw std::invalid_argument("WTF JSON value is not an object");
    return object;
}

void AddRef(StringImpl* impl) {
    if (impl) impl->refCount += 2U;
}

unsigned KeyHash(StringImpl* key) {
    if (!key || key == DeletedKey()) throw std::invalid_argument("WTF JSON object key is a null string");
    const unsigned hash = key->hashAndFlags >> 8;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
}

Bucket* Table(const ObjectBase& object) {
    return reinterpret_cast<Bucket*>(object.map.table);
}

unsigned* Metadata(Bucket* table) {
    return reinterpret_cast<unsigned*>(table);
}

unsigned TableSize(Bucket* table) { return table ? Metadata(table)[-1] : 0U; }
unsigned TableSizeMask(Bucket* table) { return table ? Metadata(table)[-2] : 0U; }
unsigned KeyCount(Bucket* table) { return table ? Metadata(table)[-3] : 0U; }
unsigned DeletedCount(Bucket* table) { return table ? Metadata(table)[-4] : 0U; }
void SetKeyCount(Bucket* table, unsigned value) { Metadata(table)[-3] = value; }
void SetDeletedCount(Bucket* table, unsigned value) { Metadata(table)[-4] = value; }

bool IsLive(const Bucket& bucket) {
    return bucket.key && bucket.key != DeletedKey();
}

void CheckTable(Bucket* table) {
    if (!table) return;
    const unsigned size = TableSize(table);
    if (size < kMinimumTableSize || (size & (size - 1)) || TableSizeMask(table) != size - 1)
        throw std::runtime_error("WTF JSON object hash table has an invalid size");
    if (std::uint64_t(KeyCount(table)) + DeletedCount(table) > size)
        throw std::runtime_error("WTF JSON object hash table has invalid counts");
}

Bucket* AllocateTable(unsigned size) {
    const std::size_t bytes = kMetadataSize + std::size_t(size) * sizeof(Bucket);
    auto* memory = static_cast<char*>(_ZN3WTF10fastMallocEm(bytes));
    std::memset(memory, 0, bytes);
    auto* table = reinterpret_cast<Bucket*>(memory + kMetadataSize);
    Metadata(table)[-1] = size;
    Metadata(table)[-2] = size - 1;
    return table;
}

void FreeTable(Bucket* table) {
    if (table) _ZN3WTF8fastFreeEPv(reinterpret_cast<char*>(table) - kMetadataSize);
}

void SetTable(ObjectBase& object, Bucket* table) {
    object.map.table = reinterpret_cast<StringValuePair*>(table);
}

Bucket* Find(const ObjectBase& object, StringImpl* key) {
    const unsigned hash = KeyHash(key);
    Bucket* table = Table(object);
    CheckTable(table);
    if (!table) return nullptr;
    const unsigned mask = TableSizeMask(table);
    unsigned index = hash & mask;
    for (unsigned probe = 1; probe <= mask + 1; ++probe) {
        Bucket& bucket = table[index];
        if (!bucket.key) return nullptr;
        if (bucket.key != DeletedKey() && _ZN3WTF5equalEPKNS_10StringImplES2_(bucket.key, key)) return &bucket;
        index = (index + probe) & mask;
    }
    return nullptr;
}

void Rehash(ObjectBase& object, unsigned newSize) {
    Bucket* oldTable = Table(object);
    const unsigned oldSize = TableSize(oldTable);
    Bucket* newTable = AllocateTable(newSize);
    SetKeyCount(newTable, KeyCount(oldTable));
    const unsigned mask = newSize - 1;
    for (unsigned i = 0; i < oldSize; ++i) {
        const Bucket& old = oldTable[i];
        if (!IsLive(old)) continue;
        unsigned index = KeyHash(old.key) & mask;
        unsigned probe = 0;
        while (newTable[index].key) {
            if (++probe > mask) {
                FreeTable(newTable);
                throw std::runtime_error("WTF JSON object hash table is full during rehash");
            }
            index = (index + probe) & mask;
        }
        newTable[index] = old;
    }
    SetTable(object, newTable);
    FreeTable(oldTable);
}

bool ShouldExpand(std::uint64_t keyAndDeleteCount, std::uint64_t tableSize) {
    if (tableSize <= kMaxSmallTableCapacity) return keyAndDeleteCount * 4 >= tableSize * 3;
    return keyAndDeleteCount * 2 >= tableSize;
}

void Expand(ObjectBase& object) {
    Bucket* table = Table(object);
    const unsigned size = TableSize(table);
    unsigned newSize = kMinimumTableSize;
    if (size) {
        if (std::uint64_t(KeyCount(table)) * kMinLoad < std::uint64_t(size) * 2) newSize = size;
        else if (size > std::numeric_limits<unsigned>::max() / 2) throw std::length_error("WTF JSON object hash table size overflow");
        else newSize = size * 2;
    }
    Rehash(object, newSize);
}

StringImpl** OrderBuffer(ObjectBase& object) {
    return reinterpret_cast<StringImpl**>(object.order.buffer);
}

void AppendOrder(ObjectBase& object, StringImpl* key) {
    auto& order = object.order;
    if (order.size == order.capacity) {
        const std::size_t capacity = order.capacity;
        const std::size_t newCapacity = std::max<std::size_t>(capacity + 1, std::max<std::size_t>(kMinimumOrderCapacity, capacity + capacity / 4 + 1));
        if (newCapacity > std::numeric_limits<unsigned>::max() / sizeof(String)) throw std::length_error("WTF JSON object key order overflow");
        auto* buffer = static_cast<StringImpl**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(String)));
        if (order.size) std::memcpy(buffer, order.buffer, order.size * sizeof(String));
        if (order.buffer) _ZN3WTF8fastFreeEPv(order.buffer);
        order.buffer = reinterpret_cast<String*>(buffer);
        order.capacity = static_cast<unsigned>(newCapacity);
    }
    AddRef(key);
    OrderBuffer(object)[order.size++] = key;
}

void RemoveFirstOrder(ObjectBase& object, StringImpl* key) {
    auto& order = object.order;
    StringImpl** buffer = OrderBuffer(object);
    for (unsigned i = 0; i < order.size; ++i) {
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(buffer[i], key)) continue;
        NKWebKit::WtfAbi::Release(buffer[i]);
        std::memmove(buffer + i, buffer + i + 1, (order.size - i - 1) * sizeof(String));
        --order.size;
        return;
    }
}

void RemoveBucket(ObjectBase& object, Bucket& bucket) {
    Bucket* table = Table(object);
    StringImpl* key = std::exchange(bucket.key, DeletedKey());
    Value* value = std::exchange(bucket.value, nullptr);
    SetDeletedCount(table, DeletedCount(table) + 1);
    SetKeyCount(table, KeyCount(table) - 1);
    NKWebKit::WtfAbi::Release(key);
    Deref(value);
    const unsigned size = TableSize(table);
    if (std::uint64_t(KeyCount(table)) * kMinLoad < size && size > kMinimumTableSize) Rehash(object, size / 2);
}

void DestroyObject(ObjectBase& object) {
    Bucket* table = Table(object);
    const unsigned size = TableSize(table);
    SetTable(object, nullptr);
    for (unsigned i = 0; i < size; ++i) {
        if (!IsLive(table[i])) continue;
        NKWebKit::WtfAbi::Release(table[i].key);
        Deref(table[i].value);
    }
    FreeTable(table);
    StringImpl** buffer = OrderBuffer(object);
    for (unsigned i = 0; i < object.order.size; ++i) NKWebKit::WtfAbi::Release(buffer[i]);
    if (buffer) _ZN3WTF8fastFreeEPv(buffer);
    object.order.buffer = nullptr;
    object.order.capacity = 0;
    object.order.size = 0;
}

void APS5_VABI ObjectVtableD1(Value* self) {
    auto& object = CheckedObject(static_cast<ObjectBase*>(self));
    DestroyObject(object);
    _ZN3WTF8JSONImpl5ValueD2Ev(&object);
}

void APS5_VABI ObjectVtableD0(Value* self) {
    ObjectVtableD1(self);
    _ZN3WTF8fastFreeEPv(self);
}

RefPtr<Object> APS5_VABI ObjectAsObject(Value* self) {
    return RefPtr<Object>::Share(static_cast<Object*>(&CheckedObject(static_cast<ObjectBase*>(self))));
}

RefPtr<const Object> APS5_VABI ObjectAsObjectConst(const Value* self) {
    return RefPtr<const Object>::Share(static_cast<const Object*>(&CheckedObject(static_cast<const ObjectBase*>(self))));
}

RefPtr<Array> APS5_VABI ObjectAsArray(Value* self) {
    CheckedObject(static_cast<ObjectBase*>(self));
    return RefPtr<Array>(nullptr);
}

void APS5_VABI ObjectWriteJSON(const Value* self, StringBuilder& output) {
    auto& object = CheckedObject(static_cast<const ObjectBase*>(self));
    NKWebKit::WtfAbi::AppendCharacter(output, '{');
    StringImpl** order = OrderBuffer(object);
    for (unsigned i = 0; i < object.order.size; ++i) {
        Bucket* bucket = Find(object, order[i]);
        if (!bucket) throw std::runtime_error("WTF JSON object key order names a missing key");
        if (i) NKWebKit::WtfAbi::AppendCharacter(output, ',');
        String key(bucket->key);
        AddRef(key.impl);
        _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&output, key);
        NKWebKit::WtfAbi::AppendCharacter(output, ':');
        const Value* value = &Checked(bucket->value);
        value->vptr->writeJSON(value, output);
    }
    NKWebKit::WtfAbi::AppendCharacter(output, '}');
}

std::size_t APS5_VABI ObjectMemoryCost(const Value* self) {
    return _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(static_cast<const ObjectBase*>(self));
}

const ValueVtableStorage ObjectVtableInstance = {
    0,
    nullptr,
    {
        &ObjectVtableD1,
        &ObjectVtableD0,
        &ObjectAsObject,
        &ObjectAsObjectConst,
        &ObjectAsArray,
        &ObjectWriteJSON,
        &ObjectMemoryCost,
    },
};

}

namespace NKWebKit::JsonAbi {

void ObjectSetValue(ObjectBase* self, const String& name, Ref<Value>&& value) {
    auto& object = CheckedObject(self);
    Checked(value.Get());
    if (Bucket* existing = Find(object, name.impl)) {
        Deref(std::exchange(existing->value, value.Leak()));
        return;
    }
    if (!Table(object)) Expand(object);
    Bucket* table = Table(object);
    const unsigned mask = TableSizeMask(table);
    unsigned index = KeyHash(name.impl) & mask;
    Bucket* deleted = nullptr;
    for (unsigned probe = 1; table[index].key; ++probe) {
        if (table[index].key == DeletedKey()) deleted = &table[index];
        if (probe > mask) break;
        index = (index + probe) & mask;
    }
    Bucket* slot = deleted ? deleted : &table[index];
    if (slot->key && slot->key != DeletedKey()) throw std::runtime_error("WTF JSON object hash table is full");
    if (deleted) SetDeletedCount(table, DeletedCount(table) - 1);
    AddRef(name.impl);
    slot->key = name.impl;
    slot->value = value.Leak();
    SetKeyCount(table, KeyCount(table) + 1);
    if (ShouldExpand(std::uint64_t(KeyCount(table)) + DeletedCount(table), TableSize(table))) Expand(object);
    AppendOrder(object, name.impl);
}

}

Ref<Object> APS5_VABI _ZN3WTF8JSONImpl6Object6createEv() {
    auto* self = static_cast<Object*>(_ZN3WTF10fastMallocEm(sizeof(Object)));
    _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(self, ValueType::Object);
    self->vptr = &ObjectVtableInstance.dispatch;
    self->map.table = nullptr;
    self->order.buffer = nullptr;
    self->order.capacity = 0U;
    self->order.size = 0U;
    return Ref<Object>::Adopt(self);
}

void APS5_VABI _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(ObjectBase* self, const String& name) {
    auto& object = CheckedObject(self);
    if (Bucket* bucket = Find(object, name.impl)) RemoveBucket(object, *bucket);
    RemoveFirstOrder(object, name.impl);
}

bool APS5_VABI _ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(const ObjectBase* self, const String& name, String& output) {
    auto& object = CheckedObject(self);
    Bucket* bucket = Find(object, name.impl);
    if (!bucket) return false;
    const Value& value = Checked(bucket->value);
    if (value.valueType != ValueType::String || !value.data.string) return false;
    String result(value.data.string);
    AddRef(result.impl);
    output = std::move(result);
    return true;
}

std::size_t APS5_VABI _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(const ObjectBase* self) {
    auto& object = CheckedObject(self);
    std::size_t cost = _ZNK3WTF8JSONImpl5Value10memoryCostEv(&object);
    Bucket* table = Table(object);
    CheckTable(table);
    for (unsigned i = 0; i < TableSize(table); ++i) {
        if (!IsLive(table[i])) continue;
        cost += _ZNK3WTF10StringImpl11sizeInBytesEv(table[i].key);
        const Value* value = &Checked(table[i].value);
        cost += value->vptr->memoryCost(value);
    }
    return cost;
}
