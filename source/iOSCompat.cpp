// iOSCompat.cpp — iOS 16 系统 libc++ 缺失符号兼容实现
//
// 背景：CI 用 Xcode 26.6（LLVM 20 时代）头文件编译，生成的代码引用新版 libc++
// 导出符号（带新 .llvm.<hash> ABI 版本）。iPhone 8 的 iOS 16.7 系统 libc++
// （LLVM 15 时代）没有这些符号 → dyld 加载 dylib 时 "missing symbol"。
// -D_LIBCPP_DISABLE_AVAILABILITY 只压编译期检查，运行期符号仍缺。
//
// 本文件提供 13 个缺失符号的等价实现：
//   std::to_chars 浮点系列 9 个（float/double/long double × 无格式/chars_format/带精度）
//   std::exception_ptr 4 个（C1 拷贝构造 / D1 析构 / aS 赋值 / __from_native_exception_pointer）
//
// 布局说明：新版 libc++（LLVM 18+）exception_ptr = 单个 void*（libc++abi 的
// primary exception 句柄，refcount 由 __cxa_increment/decrement_exception_refcount
// 管理）。libMaaCore/libMaaUtils 内所有 exception_ptr 操作（std::function 异常
// 存储、terminate handler 的 current_exception 等）均走本实现 + 新版头文件
// inline 路径，内部一致。系统库的 future/async 异常路径全程在系统库内部
// （老布局），不与本实现交互。
//
// to_chars 用 snprintf 实现：未指定精度时用 %.17g / %.9g（保证 round-trip，
// 输出可能比最短形式略长，MaaCore 用于日志/JSON，无碍）。

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

// libc++abi 公共 C API（iOS 16 系统库均导出）
extern "C" void* __cxa_current_primary_exception() noexcept;
extern "C" void __cxa_increment_exception_refcount(void*) noexcept;
extern "C" void __cxa_decrement_exception_refcount(void*) noexcept;

// ===================== std::exception_ptr（新版 ABI） =====================
// 符号名：__ZNSt13exception_ptrC1ERKS_ / D1Ev / aSERKS_ / 31__from_native_exception_pointerEPv
// 布局：单指针（native exception 句柄）。

extern "C" void __ZNSt13exception_ptrC1ERKS_(void* self, const void* other) noexcept
{
    void* p = *static_cast<void* const*>(other);
    if (p) {
        __cxa_increment_exception_refcount(p);
    }
    *static_cast<void**>(self) = p;
}

extern "C" void __ZNSt13exception_ptrD1Ev(void* self) noexcept
{
    void* p = *static_cast<void**>(self);
    if (p) {
        __cxa_decrement_exception_refcount(p);
    }
}

extern "C" void* __ZNSt13exception_ptraSERKS_(void* self, const void* other) noexcept
{
    void* np = *static_cast<void* const*>(other);
    if (np) {
        __cxa_increment_exception_refcount(np);
    }
    void* old = *static_cast<void**>(self);
    if (old) {
        __cxa_decrement_exception_refcount(old);
    }
    *static_cast<void**>(self) = np;
    return self;
}

// __from_native_exception_pointer(void* __e)：__e 来自 __cxa_current_primary_exception()
// （已带 1 个 refcount），直接持有。
extern "C" void __ZNSt13exception_ptr31__from_native_exception_pointerEPv(void* self, void* p) noexcept
{
    *static_cast<void**>(self) = p;
}

// ===================== std::to_chars 浮点系列 =====================
// 返回 to_chars_result { char* ptr; std::errc ec; } —— arm64 按值返回 (x0, x1)
// 符号：__ZNSt3__18to_charsEPcS0_{d,f,e}[NS_12chars_formatE[i]]
// chars_format 底层 int：scientific=1 fixed=2 hex=4 general=3(scientific|fixed)

enum iOSCompatCharsFormat : int
{
    iOSCompatScientific = 1,
    iOSCompatFixed = 2,
    iOSCompatHex = 4,
    iOSCompatGeneral = 3,
};

namespace
{
struct to_chars_out
{
    char* ptr;
    int ec;
};

constexpr int kValueTooLarge = 34; // std::errc::value_too_large == ERANGE

template <typename T>
to_chars_out ios_to_chars_general(char* first, char* last, T value, int digits)
{
    int n = std::snprintf(first, static_cast<size_t>(last - first), "%.*g", digits, static_cast<double>(value));
    if (n < 0 || static_cast<size_t>(n) >= static_cast<size_t>(last - first)) {
        return { last, kValueTooLarge };
    }
    return { first + n, 0 };
}

template <typename T>
to_chars_out ios_to_chars_fmt(char* first, char* last, T value, int fmt, int precision)
{
    int n = -1;
    switch (fmt) {
    case iOSCompatScientific:
        n = precision >= 0
                ? std::snprintf(first, static_cast<size_t>(last - first), "%.*e", precision, static_cast<double>(value))
                : std::snprintf(first, static_cast<size_t>(last - first), "%.*e", 17, static_cast<double>(value));
        break;
    case iOSCompatFixed:
        n = precision >= 0
                ? std::snprintf(first, static_cast<size_t>(last - first), "%.*f", precision, static_cast<double>(value))
                : std::snprintf(first, static_cast<size_t>(last - first), "%.*f", 17, static_cast<double>(value));
        break;
    case iOSCompatHex:
        n = std::snprintf(first, static_cast<size_t>(last - first), "%a", static_cast<double>(value));
        break;
    default: // general
        n = precision >= 0
                ? std::snprintf(first, static_cast<size_t>(last - first), "%.*g", precision, static_cast<double>(value))
                : std::snprintf(first, static_cast<size_t>(last - first), "%.*g", 17, static_cast<double>(value));
        break;
    }
    if (n < 0 || static_cast<size_t>(n) >= static_cast<size_t>(last - first)) {
        return { last, kValueTooLarge };
    }
    return { first + n, 0 };
}
} // namespace

// ---- double (d) ----
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_d(char* first, char* last, double value) noexcept
{
    return ios_to_chars_general(first, last, value, 17);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_dNS_12chars_formatE(char* first, char* last, double value, int fmt) noexcept
{
    return ios_to_chars_fmt(first, last, value, fmt, -1);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_dNS_12chars_formatEi(char* first, char* last, double value, int fmt, int precision) noexcept
{
    return ios_to_chars_fmt(first, last, value, fmt, precision);
}

// ---- float (f) ----
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_f(char* first, char* last, float value) noexcept
{
    return ios_to_chars_general(first, last, value, 9);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_fNS_12chars_formatE(char* first, char* last, float value, int fmt) noexcept
{
    return ios_to_chars_fmt(first, last, value, fmt, -1);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_fNS_12chars_formatEi(char* first, char* last, float value, int fmt, int precision) noexcept
{
    return ios_to_chars_fmt(first, last, value, fmt, precision);
}

// ---- long double (e) —— arm64 iOS 上 long double 即 double ----
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_e(char* first, char* last, long double value) noexcept
{
    return ios_to_chars_general(first, last, static_cast<double>(value), 17);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_eNS_12chars_formatE(char* first, char* last, long double value, int fmt) noexcept
{
    return ios_to_chars_fmt(first, last, static_cast<double>(value), fmt, -1);
}
extern "C" to_chars_out __ZNSt3__18to_charsEPcS0_eNS_12chars_formatEi(char* first, char* last, long double value, int fmt, int precision) noexcept
{
    return ios_to_chars_fmt(first, last, static_cast<double>(value), fmt, precision);
}
