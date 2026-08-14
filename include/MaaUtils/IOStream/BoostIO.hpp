#pragma once

#define BOOST_PROCESS_USE_STD_FS 1

#include <boost/asio.hpp>
// IAA: iOS 沙盒无子进程支持，boost::process（依赖 wordexp）不可用，
// 且 ChildPipeIOStream（唯一使用者）已在 iOS 构建中排除
#ifndef MAA_UTILS_IOS
#include <boost/process.hpp>
#endif
#ifdef _WIN32
#include <boost/process/extend.hpp>
#include <boost/process/windows.hpp>
#endif
