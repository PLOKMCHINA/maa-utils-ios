find_path(ONNXRuntime_INCLUDE_DIR NAMES onnxruntime/onnxruntime_c_api.h)

find_library(ONNXRuntime_LIBRARY_IMP NAMES onnxruntime)

# IAA: iOS 上 vcpkg 的 onnxruntime 可能是 framework 或配置不同，
# fallback 到 vcpkg 提供的 CMake config
if(IOS AND NOT ONNXRuntime_LIBRARY_IMP)
    find_package(onnxruntime CONFIG QUIET)
    if(TARGET onnxruntime::onnxruntime)
        if(NOT ONNXRuntime_INCLUDE_DIR)
            get_target_property(_ort_inc onnxruntime::onnxruntime INTERFACE_INCLUDE_DIRECTORIES)
            if(_ort_inc)
                set(ONNXRuntime_INCLUDE_DIR "${_ort_inc}")
            endif()
        endif()
        set(ONNXRuntime_FOUND TRUE)
        set(ONNXRuntime_INCLUDE_DIRS ${ONNXRuntime_INCLUDE_DIR})
        if(NOT TARGET ONNXRuntime::ONNXRuntime)
            add_library(ONNXRuntime::ONNXRuntime INTERFACE IMPORTED)
            set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
                INTERFACE_LINK_LIBRARIES onnxruntime::onnxruntime
                INTERFACE_INCLUDE_DIRECTORIES "${ONNXRuntime_INCLUDE_DIR}")
        endif()
        return()
    endif()
endif()

if (WIN32)
    get_filename_component(ONNXRuntime_PATH_LIB ${ONNXRuntime_LIBRARY_IMP} DIRECTORY)
    find_file(ONNXRuntime_LIBRARY NAMES onnxruntime_maa.dll PATHS "${ONNXRuntime_PATH_LIB}/../bin")

    find_file(ONNXRuntime_LIBRARY_IMP_DEBUG NAMES onnxruntime.lib PATHS "${ONNXRuntime_PATH_LIB}/../debug/lib")
    find_file(ONNXRuntime_LIBRARY_DEBUG NAMES onnxruntime_maa.dll PATHS "${ONNXRuntime_PATH_LIB}/../debug/bin")
else ()
    set(ONNXRuntime_LIBRARY ${ONNXRuntime_LIBRARY_IMP})
endif (WIN32)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    ONNXRuntime
    REQUIRED_VARS ONNXRuntime_LIBRARY_IMP ONNXRuntime_INCLUDE_DIR
)

if(ONNXRuntime_FOUND)
    set(ONNXRuntime_INCLUDE_DIRS ${ONNXRuntime_INCLUDE_DIR})
    if(NOT TARGET ONNXRuntime::ONNXRuntime)
        add_library(ONNXRuntime::ONNXRuntime SHARED IMPORTED)
        set_property(TARGET ONNXRuntime::ONNXRuntime APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        if (WIN32)
            set_property(TARGET ONNXRuntime::ONNXRuntime APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
                IMPORTED_IMPLIB_RELEASE "${ONNXRuntime_LIBRARY_IMP}"
            )
            set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
                IMPORTED_IMPLIB_DEBUG "${ONNXRuntime_LIBRARY_IMP_DEBUG}"
                IMPORTED_LOCATION_DEBUG "${ONNXRuntime_LIBRARY_DEBUG}"
            )
        endif (WIN32)
        set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
            IMPORTED_LOCATION_RELEASE "${ONNXRuntime_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRuntime_INCLUDE_DIR}"
        )
    endif()
endif()
