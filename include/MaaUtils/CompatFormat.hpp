#pragma once

// std::format 兼容层
// iOS：系统 libc++（iOS 16）无 std::format 导出符号（新版头文件生成的符号带
// .llvm.<hash> ABI 版本后缀，老系统没有）→ 用 fmt 库（header/静态库）实现。
// 其他平台保持标准 <format>。
#if defined(__APPLE__) && !defined(__MAC_OS_X_VERSION_MIN_REQUIRED)

#include <fmt/format.h>

#include <string>
#include <utility>

namespace std
{
template <class... Args>
std::string format(fmt::format_string<Args...> __fmt, Args&&... __args)
{
    return fmt::format(__fmt, std::forward<Args>(__args)...);
}
} // namespace std

#else

#include <format>

#endif
