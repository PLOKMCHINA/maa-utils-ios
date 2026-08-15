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
// ⚠️ 两个关键坑（都踩过）：
// 1. Mach-O 下 extern "C" 函数名自动加前导下划线（foo → _foo），直接把 mangled
//    名写 extern "C" 函数名会变成 ___ZNSt13...（三个下划线），与 C++ 引用的
//    __ZNSt13...（两个）对不上。必须用 asm labels 语法精确指定符号名。
// 2. Apple Clang 不认 __attribute__((asm("...")))（报 unknown attribute），
//    必须用 GCC 风格的声明后缀 __asm__("符号名")，声明和定义都要带。

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

// libc++abi 公共 C API（iOS 16 系统库均导出）
extern "C" void* __cxa_current_primary_exception() noexcept;
extern "C" void __cxa_increment_exception_refcount(void*) noexcept;
extern "C" void __cxa_decrement_exception_refcount(void*) noexcept;

// ===================== std::exception_ptr（新版 ABI） =====================
// 布局：单指针（native exception 句柄）。

extern "C" void ep_copy_ctor(void* self, const void* other) noexcept
    __asm__("__ZNSt13exception_ptrC1ERKS_");
extern "C" void ep_copy_ctor(void* self, const void* other) noexcept
    {

    void* p = *static_cast<void* const*>(other);
    if (p) {
        __cxa_increment_exception_refcount(p);
    }
    *static_cast<void**>(self) = p;
}

extern "C" void ep_dtor(void* self) noexcept __asm__("__ZNSt13exception_ptrD1Ev");
extern "C" void ep_dtor(void* self) noexcept {

    void* p = *static_cast<void**>(self);
    if (p) {
        __cxa_decrement_exception_refcount(p);
    }
}

extern "C" void* ep_assign(void* self, const void* other) noexcept
    __asm__("__ZNSt13exception_ptraSERKS_");
extern "C" void* ep_assign(void* self, const void* other) noexcept
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
extern "C" void ep_from_native(void* self, void* p) noexcept
    __asm__("__ZNSt13exception_ptr31__from_native_exception_pointerEPv");
extern "C" void ep_from_native(void* self, void* p) noexcept
    {

    *static_cast<void**>(self) = p;
}

// ===================== std::__hash_memory =====================
// LLVM 18+ libc++ 新增（unordered 容器辅助），iOS 16 系统库没有。
// 语义：确定性字节散列（libc++ 用 FNV-1a），返回 size_t。
extern "C" size_t ios_hash_memory(const void* ptr, size_t size) noexcept
    __asm__("__ZNSt3__113__hash_memoryEPKvm");
extern "C" size_t ios_hash_memory(const void* ptr, size_t size) noexcept
{
    const unsigned char* p = static_cast<const unsigned char*>(ptr);
    size_t h = 1469598103934665603ULL; // FNV offset basis
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 1099511628211ULL; // FNV prime
    }
    return h;
}

// ===================== std::to_chars 浮点系列 =====================
// 返回 to_chars_result { char* ptr; std::errc ec; } —— arm64 按值返回 (x0, x1)
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

// 宏：声明带 __asm__ 精确符号名（定义处不带——asm label 只允许在声明）
// 注意：asm label 不支持重载，每个重载必须用不同 C 函数名（name 参数）
#define IOS_COMPAT_TO_CHARS(name, suffix, param_list, asm_suffix) \
    extern "C" to_chars_out name param_list noexcept \
        __asm__("__ZNSt3__18to_charsEPcS0_" #suffix asm_suffix); \
    extern "C" to_chars_out name param_list noexcept

// ---- double (d) ----
IOS_COMPAT_TO_CHARS(ios_tc_d, d, (char* first, char* last, double value), "")
{
    return ios_to_chars_general(first, last, value, 17);
}
IOS_COMPAT_TO_CHARS(ios_tc_d_fmt, d, (char* first, char* last, double value, int fmt), "NS_12chars_formatE")
{
    return ios_to_chars_fmt(first, last, value, fmt, -1);
}
IOS_COMPAT_TO_CHARS(ios_tc_d_fmtp, d, (char* first, char* last, double value, int fmt, int precision), "NS_12chars_formatEi")
{
    return ios_to_chars_fmt(first, last, value, fmt, precision);
}

// ---- float (f) ----
IOS_COMPAT_TO_CHARS(ios_tc_f, f, (char* first, char* last, float value), "")
{
    return ios_to_chars_general(first, last, value, 9);
}
IOS_COMPAT_TO_CHARS(ios_tc_f_fmt, f, (char* first, char* last, float value, int fmt), "NS_12chars_formatE")
{
    return ios_to_chars_fmt(first, last, value, fmt, -1);
}
IOS_COMPAT_TO_CHARS(ios_tc_f_fmtp, f, (char* first, char* last, float value, int fmt, int precision), "NS_12chars_formatEi")
{
    return ios_to_chars_fmt(first, last, value, fmt, precision);
}

// ---- long double (e) —— arm64 iOS 上 long double 即 double ----
IOS_COMPAT_TO_CHARS(ios_tc_e, e, (char* first, char* last, long double value), "")
{
    return ios_to_chars_general(first, last, static_cast<double>(value), 17);
}
IOS_COMPAT_TO_CHARS(ios_tc_e_fmt, e, (char* first, char* last, long double value, int fmt), "NS_12chars_formatE")
{
    return ios_to_chars_fmt(first, last, static_cast<double>(value), fmt, -1);
}
IOS_COMPAT_TO_CHARS(ios_tc_e_fmtp, e, (char* first, char* last, long double value, int fmt, int precision), "NS_12chars_formatEi")
{
    return ios_to_chars_fmt(first, last, static_cast<double>(value), fmt, precision);
}

#undef IOS_COMPAT_TO_CHARS
