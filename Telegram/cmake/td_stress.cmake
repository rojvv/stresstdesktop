set(TDESKTOP_STRESSPP_ROOT "$ENV{HOME}/Projects/stresspp" CACHE PATH
    "Local stresspp source checkout")
set(TDESKTOP_ONNXRUNTIME_ROOT "${PROJECT_SOURCE_DIR}/.stress-build/onnxruntime-static"
    CACHE PATH "Static ONNX Runtime installation")

set(stress_runtime_library "${TDESKTOP_ONNXRUNTIME_ROOT}/lib/libonnxruntime.a")
if (NOT EXISTS "${stress_runtime_library}")
    message(FATAL_ERROR "Build static ONNX Runtime first; see docs/stress-marking.md")
endif()
file(READ "${stress_runtime_library}" stress_runtime_magic LIMIT 8 HEX)
if (NOT stress_runtime_magic STREQUAL "213c617263683e0a")
    message(FATAL_ERROR "ONNX Runtime must be a static archive, not a dynamic library")
endif()

add_library(stress_onnxruntime STATIC IMPORTED)
set_target_properties(stress_onnxruntime PROPERTIES
    IMPORTED_LOCATION "${stress_runtime_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${TDESKTOP_ONNXRUNTIME_ROOT}/include"
)

add_library(stresspp_core STATIC)
target_compile_features(stresspp_core PRIVATE cxx_std_17)
foreach (source constants exception_dictionary_wrapper exception_dictionary model predict utils)
    target_sources(stresspp_core PRIVATE "${TDESKTOP_STRESSPP_ROOT}/${source}.cc")
endforeach()
target_sources(stresspp_core PRIVATE "${src_loc}/history/history_stress_session.cpp")
target_include_directories(stresspp_core PUBLIC "${TDESKTOP_STRESSPP_ROOT}")
target_link_libraries(stresspp_core PUBLIC stress_onnxruntime)
if (APPLE)
    target_link_libraries(stresspp_core PUBLIC
        "-framework Foundation"
        "-framework CoreFoundation"
        "-framework CoreML"
        "-framework Accelerate"
    )
endif()
if (DESKTOP_APP_MAC_ARCH)
    set_target_properties(stresspp_core PROPERTIES OSX_ARCHITECTURES "${DESKTOP_APP_MAC_ARCH}")
endif()

add_library(td_stress STATIC)
init_non_host_target(td_stress)
target_sources(td_stress PRIVATE
    "${src_loc}/history/history_stress_marking.cpp"
    "${src_loc}/history/history_stress_marking.h"
)
target_include_directories(td_stress PUBLIC "${src_loc}")
target_link_libraries(td_stress PRIVATE stresspp_core PUBLIC desktop-app::lib_ui)
add_library(tdesktop::td_stress ALIAS td_stress)

if (TDESKTOP_STRESS_TESTS)
    add_executable(stress_marking_tests "${PROJECT_SOURCE_DIR}/tools/stress-tests/marking_tests.cpp")
    init_non_host_target(stress_marking_tests)
    target_link_libraries(stress_marking_tests PRIVATE td_stress)
    add_executable(stresspp_regression_tests "${TDESKTOP_STRESSPP_ROOT}/tests/stress_test.cc")
    target_link_libraries(stresspp_regression_tests PRIVATE stresspp_core)
    target_compile_features(stresspp_regression_tests PRIVATE cxx_std_17)
    add_test(NAME stress_marking COMMAND stress_marking_tests)
    add_test(NAME stresspp_regressions COMMAND stresspp_regression_tests)
endif()
