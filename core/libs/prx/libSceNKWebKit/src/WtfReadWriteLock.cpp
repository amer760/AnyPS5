#include "prx/libSceNKWebKit/include/WtfReadWriteLock.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

using RwLock = WTF::ReadWriteLock;

constexpr std::uint8_t kHeldBit = 1;
constexpr unsigned kSpinsBeforeSleep = 64;

void Backoff(unsigned& spins) {
    if (spins < kSpinsBeforeSleep) {
        ++spins;
        std::this_thread::yield();
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

void LockByte(std::uint8_t& byte) {
    std::atomic_ref<std::uint8_t> lock(byte);
    unsigned spins = 0;
    std::uint8_t expected = 0;
    while (!lock.compare_exchange_weak(expected, kHeldBit, std::memory_order_acquire)) {
        if (expected & ~kHeldBit) throw std::runtime_error("ReadWriteLock: lock has parked threads");
        expected = 0;
        Backoff(spins);
    }
}

void UnlockByte(std::uint8_t& byte) {
    std::atomic_ref<std::uint8_t>(byte).store(0, std::memory_order_release);
}

class Locker {
public:
    explicit Locker(RwLock* self)
        : m_self(self) {
        if (!m_self) throw std::invalid_argument("ReadWriteLock: null lock");
        LockByte(m_self->lock);
        m_held = true;
    }

    ~Locker() {
        if (m_held) UnlockByte(m_self->lock);
    }

    Locker(const Locker&) = delete;
    Locker& operator=(const Locker&) = delete;

    void Wait(unsigned& spins) {
        m_held = false;
        UnlockByte(m_self->lock);
        Backoff(spins);
        LockByte(m_self->lock);
        m_held = true;
    }

private:
    RwLock* m_self;
    bool m_held = false;
};

}

extern "C" void APS5_VABI _ZN3WTF13ReadWriteLock8readLockEv(WTF::ReadWriteLock* self) {
    Locker locker(self);
    unsigned spins = 0;
    while (self->isWriteLocked || self->numWaitingWriters) locker.Wait(spins);
    if (self->numReaders == 0xFFFFFFFFu) throw std::overflow_error("ReadWriteLock::readLock: reader count overflow");
    ++self->numReaders;
}

extern "C" void APS5_VABI _ZN3WTF13ReadWriteLock10readUnlockEv(WTF::ReadWriteLock* self) {
    Locker locker(self);
    if (!self->numReaders) throw std::logic_error("ReadWriteLock::readUnlock: no reader holds the lock");
    --self->numReaders;
}

extern "C" void APS5_VABI _ZN3WTF13ReadWriteLock9writeLockEv(WTF::ReadWriteLock* self) {
    Locker locker(self);
    unsigned spins = 0;
    while (self->isWriteLocked || self->numReaders) {
        if (self->numWaitingWriters == 0xFFFFFFFFu) throw std::overflow_error("ReadWriteLock::writeLock: waiting writer count overflow");
        ++self->numWaitingWriters;
        locker.Wait(spins);
        --self->numWaitingWriters;
    }
    self->isWriteLocked = true;
}

extern "C" void APS5_VABI _ZN3WTF13ReadWriteLock11writeUnlockEv(WTF::ReadWriteLock* self) {
    Locker locker(self);
    self->isWriteLocked = false;
}
