cmake_minimum_required(VERSION 3.21)
project(PixelGuard VERSION 0.2.0 LANGUAGES CXX)

# This machine has a Chinese-only MSVC resource pack. CMake's detected prefix
# is misdecoded; the local preset supplies the UTF-8 prefix used by Ninja.
if(MSVC AND DEFINED PIXELGUARD_MSVC_INCLUDES_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${PIXELGUARD_MSVC_INCLUDES_PREFIX}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8 /W4)
endif()

find_package(OpenCV 4.6 REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets)

add_library(pixelguard_core src/core/DiffDetector.cpp)
target_include_directories(pixelguard_core PUBLIC src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(pixelguard_core PUBLIC ${OpenCV_LIBS})

# Windows Unicode paths are handled at the Qt boundary, not inside the core.
add_library(pixelguard_imageio src/ui/ImageIO.cpp)
target_link_libraries(pixelguard_imageio PUBLIC pixelguard_core Qt6::Gui)

add_library(pixelguard_widgets src/ui/MainWindow.cpp src/ui/ImagePreview.cpp src/ui/Theme.cpp)
target_link_libraries(pixelguard_widgets PUBLIC pixelguard_imageio Qt6::Widgets)
qt_add_resources(pixelguard_widgets pixelguard_theme PREFIX "/" FILES resources/theme.qss)

add_executable(PixelGuard WIN32 src/main.cpp)
target_link_libraries(PixelGuard PRIVATE pixelguard_widgets)

# Copy OpenCV beside executables so tests do not depend on its PATH entry.
if(WIN32 AND TARGET opencv_world)
    add_custom_command(TARGET PixelGuard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:opencv_world> $<TARGET_FILE_DIR:PixelGuard>)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Test)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE pixelguard_widgets Qt6::Test)
    add_test(NAME widgets_interaction COMMAND ui_tests)
    add_executable(real_screenshot_tests tests/real_screenshot_tests.cpp)
    target_link_libraries(real_screenshot_tests PRIVATE pixelguard_imageio)
    add_test(NAME real_screenshots COMMAND real_screenshot_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/test_images" "${CMAKE_BINARY_DIR}/evidence/real_cases")
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE pixelguard_core)
    add_test(NAME core_detection COMMAND core_tests)

    add_executable(imageio_tests tests/imageio_tests.cpp)
    target_link_libraries(imageio_tests PRIVATE pixelguard_imageio)
    add_test(NAME unicode_image_io COMMAND imageio_tests "${CMAKE_CURRENT_SOURCE_DIR}/test_images")

    # Only the test process receives these search paths; system PATH is untouched.
    get_target_property(QT_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE}" DIRECTORY)
    foreach(test_target core_tests imageio_tests ui_tests real_screenshot_tests)
        if(WIN32 AND TARGET opencv_world)
            add_custom_command(TARGET ${test_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:opencv_world> $<TARGET_FILE_DIR:${test_target}>)
        endif()
    endforeach()
    add_test(NAME widgets_startup COMMAND PixelGuard --smoke-test)
    set_tests_properties(unicode_image_io widgets_startup widgets_interaction real_screenshots PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}")
    set_tests_properties(widgets_startup PROPERTIES TIMEOUT 15)
    set_tests_properties(widgets_interaction PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()
