#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ReadWriteLock check failed at line %u\n", location.line());
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

using Method = void (APS5_VABI *)(WTF::ReadWriteLock*);
static_assert(std::is_same_v<decltype(&_ZN3WTF13ReadWriteLock8readLockEv), Method>);
static_assert(std::is_same_v<decltype(&_ZN3WTF13ReadWriteLock10readUnlockEv), Method>);
static_assert(std::is_same_v<decltype(&_ZN3WTF13ReadWriteLock9writeLockEv), Method>);
static_assert(std::is_same_v<decltype(&_ZN3WTF13ReadWriteLock11writeUnlockEv), Method>);
static_assert(std::is_same_v<decltype(&_ZN3WTF5sleepENS_7SecondsE), void (APS5_VABI *)(WTF::Seconds)>);

void ReadLock(WTF::ReadWriteLock& lock) { _ZN3WTF13ReadWriteLock8readLockEv(&lock); }
void ReadUnlock(WTF::ReadWriteLock& lock) { _ZN3WTF13ReadWriteLock10readUnlockEv(&lock); }
void WriteLock(WTF::ReadWriteLock& lock) { _ZN3WTF13ReadWriteLock9writeLockEv(&lock); }
void WriteUnlock(WTF::ReadWriteLock& lock) { _ZN3WTF13ReadWriteLock11writeUnlockEv(&lock); }

struct Guarded {
    std::uint32_t before = 0xA5A5A5A5;
    WTF::ReadWriteLock lock{};
    std::uint32_t after = 0x5A5A5A5A;
};

void TestSingleThread() {
    Guarded value;
    ReadLock(value.lock);
    ReadLock(value.lock);
    ReadLock(value.lock);
    Require(value.lock.numReaders == 3 && !value.lock.isWriteLocked && value.lock.lock == 0);
    ReadUnlock(value.lock);
    ReadUnlock(value.lock);
    ReadUnlock(value.lock);
    Require(value.lock.numReaders == 0);
    WriteLock(value.lock);
    Require(value.lock.isWriteLocked && value.lock.numWaitingWriters == 0 && value.lock.lock == 0);
    WriteUnlock(value.lock);
    Require(!value.lock.isWriteLocked && value.lock.condition == 0);
    Require(value.before == 0xA5A5A5A5 && value.after == 0x5A5A5A5A);
    RequireThrows([&] { ReadUnlock(value.lock); });
    Require(value.lock.numReaders == 0 && value.lock.lock == 0);
    RequireThrows([] { _ZN3WTF13ReadWriteLock8readLockEv(nullptr); });
    RequireThrows([] { _ZN3WTF13ReadWriteLock10readUnlockEv(nullptr); });
    RequireThrows([] { _ZN3WTF13ReadWriteLock9writeLockEv(nullptr); });
    RequireThrows([] { _ZN3WTF13ReadWriteLock11writeUnlockEv(nullptr); });
    WTF::ReadWriteLock full{};
    full.numReaders = 0xFFFFFFFFu;
    RequireThrows([&] { ReadLock(full); });
    Require(full.numReaders == 0xFFFFFFFFu && full.lock == 0);
    std::printf("ReadWriteLock: single-thread state changes passed\n");
}

void TestWaitingWriterBlocksNewReaders() {
    WTF::ReadWriteLock lock{};
    std::atomic<int> order{0};
    std::atomic<int> writerOrder{0};
    std::atomic<int> readerOrder{0};
    ReadLock(lock);
    std::thread writer([&] {
        WriteLock(lock);
        writerOrder = ++order;
        WriteUnlock(lock);
    });
    while (std::atomic_ref<std::uint32_t>(lock.numWaitingWriters).load() == 0) std::this_thread::yield();
    std::thread reader([&] {
        ReadLock(lock);
        readerOrder = ++order;
        ReadUnlock(lock);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(order == 0);
    ReadUnlock(lock);
    writer.join();
    reader.join();
    Require(writerOrder == 1 && readerOrder == 2);
    Require(lock.numReaders == 0 && lock.numWaitingWriters == 0 && !lock.isWriteLocked && lock.lock == 0);
    std::printf("ReadWriteLock: a waiting writer goes before later readers\n");
}

void TestContention() {
    WTF::ReadWriteLock lock{};
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::atomic<bool> torn{false};
    std::atomic<bool> stop{false};
    constexpr int writers = 4;
    constexpr int increments = 5000;
    std::vector<std::thread> threads;
    for (int t = 0; t < writers; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < increments; ++i) {
                WriteLock(lock);
                ++first;
                ++second;
                WriteUnlock(lock);
            }
        });
    }
    std::vector<std::thread> readers;
    for (int t = 0; t < 3; ++t) {
        readers.emplace_back([&] {
            while (!stop) {
                ReadLock(lock);
                if (first != second) torn = true;
                ReadUnlock(lock);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    stop = true;
    for (auto& thread : readers) thread.join();
    Require(!torn);
    Require(first == std::uint64_t(writers) * increments && second == first);
    Require(lock.numReaders == 0 && lock.numWaitingWriters == 0 && !lock.isWriteLocked && lock.lock == 0);
    std::printf("ReadWriteLock: %d writers and 3 readers kept the guarded pair consistent\n", writers);
}

void TestSleep() {
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{0.02});
    Require(Clock::now() - start >= std::chrono::milliseconds(20));
    start = Clock::now();
    _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{0.0});
    _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{-5.0});
    _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{-std::numeric_limits<double>::infinity()});
    _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds{std::numeric_limits<double>::quiet_NaN()});
    Require(Clock::now() - start < std::chrono::seconds(2));
    std::printf("WTF::sleep(Seconds): positive, zero, negative and NaN durations passed\n");
}

}

int main() {
    TestSingleThread();
    TestWaitingWriterBlocksNewReaders();
    TestContention();
    TestSleep();
}
