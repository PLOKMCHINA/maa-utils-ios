file(GLOB_RECURSE maa_utils_src *.h *.hpp *.cpp)
file(GLOB_RECURSE maa_utils_header ${MAAUTILS_DIR}/include/MaaUtils/*)

# IAA: iOS 沙盒禁止创建子进程，ChildPipeIOStream（依赖 boost::process）不参与编译
# IAA: PlatformMacOS.cpp 用 macOS 专属 libproc.h，iOS 无此头；list_processes/get_process_path 无调用方，直接排除
if(IOS)
    list(FILTER maa_utils_src EXCLUDE REGEX "ChildPipeIOStream\\.cpp$")
    list(FILTER maa_utils_src EXCLUDE REGEX "PlatformMacOS\\.cpp$")
endif()
add_library(MaaUtils SHARED ${maa_utils_src} ${maa_utils_header})
target_include_directories(MaaUtils PRIVATE ${MAAUTILS_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MaaUtils PUBLIC Boost::regex PRIVATE Boost::system ${OpenCV_LIBS})
if(IOS)
    # IAA: iOS 16 系统 libc++ 缺新版符号（to_chars 浮点/exception_ptr），
    # source/iOSCompat.cpp 提供等价实现；std::format 由 fmt 库提供（CompatFormat.hpp）
    find_package(fmt CONFIG REQUIRED)
    target_link_libraries(MaaUtils PRIVATE fmt::fmt)
endif()

if(WIN32)
    target_link_libraries(MaaUtils PRIVATE d3d12 dxgi Cfgmgr32)
endif()

if(LINUX)
    target_link_libraries(MaaUtils PRIVATE pthread dl)
endif()

target_compile_definitions(MaaUtils PRIVATE MAA_UTILS_EXPORTS)
# IAA: iOS 平台标记，PUBLIC 传播给 MaaCore（其源码也会 include BoostIO.hpp 链）
if(IOS)
    target_compile_definitions(MaaUtils PUBLIC MAA_UTILS_IOS)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${maa_utils_src})
