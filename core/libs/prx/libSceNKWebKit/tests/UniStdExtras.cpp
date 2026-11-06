#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "UniStdExtras check failed at line %u\n", location.line());
        std::abort();
    }
}

constexpr int kGetDescriptorFlags = 1;
constexpr int kSetDescriptorFlags = 2;
constexpr int kGetStatusFlags = 3;
constexpr int kCloseOnExec = 1;
constexpr int kNonBlock = 4;
constexpr int kBadDescriptor = 9;

void TestSetNonBlock() {
    int descriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    for (int descriptor : descriptors) {
        const int before = fcntl_nid_postfix(descriptor, kGetStatusFlags, 0);
        Require(before != -1 && (before & kNonBlock) == 0);
        Require(_ZN3WTF11setNonBlockEi(descriptor));
        const int after = fcntl_nid_postfix(descriptor, kGetStatusFlags, 0);
        Require(after == (before | kNonBlock));
        Require(_ZN3WTF11setNonBlockEi(descriptor));
        Require(fcntl_nid_postfix(descriptor, kGetStatusFlags, 0) == after);
    }
    char byte = 0;
    Require(read_nid_postfix(descriptors[0], &byte, 1) == -1);
    Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0);
    *__error_nid_postfix() = 0;
    Require(!_ZN3WTF11setNonBlockEi(descriptors[0]));
    Require(*__error_nid_postfix() == kBadDescriptor);
    std::printf("setNonBlock: pipe ends become non-blocking, a closed descriptor fails with EBADF\n");
}

void TestDupCloseOnExec() {
    int descriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    Require(fcntl_nid_postfix(descriptors[1], kSetDescriptorFlags, 0) == 0);
    const int duplicate = _ZN3WTF14dupCloseOnExecEi(descriptors[1]);
    Require(duplicate >= 0 && duplicate != descriptors[0] && duplicate != descriptors[1]);
    Require(fcntl_nid_postfix(duplicate, kGetDescriptorFlags) == kCloseOnExec);
    Require(fcntl_nid_postfix(descriptors[1], kGetDescriptorFlags) == 0);
    const char sent = 'w';
    Require(write_nid_postfix(duplicate, &sent, 1) == 1);
    char received = 0;
    Require(read_nid_postfix(descriptors[0], &received, 1) == 1 && received == sent);
    Require(close_nid_postfix(duplicate) == 0);
    Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(descriptors[1]) == 0);
    *__error_nid_postfix() = 0;
    Require(_ZN3WTF14dupCloseOnExecEi(descriptors[0]) == -1);
    Require(*__error_nid_postfix() == kBadDescriptor);
    Require(_ZN3WTF14dupCloseOnExecEi(-1) == -1);
    std::printf("dupCloseOnExec: duplicate shares the pipe, has FD_CLOEXEC, and bad descriptors fail\n");
}

}

int main() {
    TestSetNonBlock();
    TestDupCloseOnExec();
}
