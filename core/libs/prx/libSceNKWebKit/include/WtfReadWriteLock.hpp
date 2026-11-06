#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WTF {

struct ReadWriteLock {
    std::uint8_t lock;
    std::uint8_t condition;
    bool isWriteLocked;
    std::uint32_t numReaders;
    std::uint32_t numWaitingWriters;
};

static_assert(std::is_standard_layout_v<ReadWriteLock>);
static_assert(sizeof(ReadWriteLock) == 12);
static_assert(alignof(ReadWriteLock) == 4);
static_assert(offsetof(ReadWriteLock, lock) == 0);
static_assert(offsetof(ReadWriteLock, condition) == 1);
static_assert(offsetof(ReadWriteLock, isWriteLocked) == 2);
static_assert(offsetof(ReadWriteLock, numReaders) == 4);
static_assert(offsetof(ReadWriteLock, numWaitingWriters) == 8);

}

extern "C" {

void APS5_VABI _ZN3WTF13ReadWriteLock8readLockEv(WTF::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock10readUnlockEv(WTF::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock9writeLockEv(WTF::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock11writeUnlockEv(WTF::ReadWriteLock* self);

}
