#include "prx/libSceNKWebKit/include/WtfUniStdExtras.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

constexpr int kDuplicateCloseOnExec = 17;
constexpr int kGetDescriptorFlags = 1;
constexpr int kSetDescriptorFlags = 2;
constexpr int kGetStatusFlags = 3;
constexpr int kSetStatusFlags = 4;
constexpr int kCloseOnExec = 1;
constexpr int kNonBlock = 4;
constexpr int kInterrupted = 4;

bool Interrupted() {
    return *__error_nid_postfix() == kInterrupted;
}

bool SetCloseOnExec(int descriptor) {
    int result = -1;
    do {
        const int flags = fcntl_nid_postfix(descriptor, kGetDescriptorFlags);
        if (flags != -1) result = fcntl_nid_postfix(descriptor, kSetDescriptorFlags, flags | kCloseOnExec);
    } while (result == -1 && Interrupted());
    return result != -1;
}

void CloseWithRetry(int descriptor) {
    while (close_nid_postfix(descriptor) == -1 && Interrupted()) {
    }
}

}

extern "C" bool APS5_VABI _ZN3WTF11setNonBlockEi(int descriptor) {
    int result = -1;
    const int flags = fcntl_nid_postfix(descriptor, kGetStatusFlags, 0);
    while ((result = fcntl_nid_postfix(descriptor, kSetStatusFlags, flags | kNonBlock)) == -1 && Interrupted()) {
    }
    return result != -1;
}

extern "C" int APS5_VABI _ZN3WTF14dupCloseOnExecEi(int descriptor) {
    int duplicate = -1;
    while ((duplicate = fcntl_nid_postfix(descriptor, kDuplicateCloseOnExec, 0)) == -1 && Interrupted()) {
    }
    if (duplicate != -1) return duplicate;
    while ((duplicate = dup_nid_postfix(descriptor)) == -1 && Interrupted()) {
    }
    if (duplicate == -1) return -1;
    if (!SetCloseOnExec(duplicate)) {
        CloseWithRetry(duplicate);
        return -1;
    }
    return duplicate;
}
