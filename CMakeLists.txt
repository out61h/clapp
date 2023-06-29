cmake_minimum_required( VERSION 3.11 )
project( clapp VERSION 0.1.0 LANGUAGES C CXX )

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_LIST_DIR}/bin)

option(RTL_ENABLE_ASSERT "Turn on assertions cheks with RTL_ASSERT macro." OFF)
option(RTL_ENABLE_LOG "Enable logging to debug output with RTL_LOG macro." OFF)
option(RTL_ENABLE_RUNTIME_CHECKS "Enable checking the result of system API's calls." OFF)
option(RTL_ENABLE_RUNTIME_TESTS "Enable runtime tests execution at program startup." OFF)

find_package(OpenCL REQUIRED)
find_package(rtl REQUIRED)

aux_source_directory(src/ SOURCES)
aux_source_directory(src/clapp SOURCES)

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/res/clapp.rc
    ${SOURCES}
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/res/cl)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        RTL_ENABLE_APP ON
        RTL_ENABLE_APP_AUDIO ON
        RTL_ENABLE_APP_CLOCK ON
        RTL_ENABLE_APP_CURSOR ON
        RTL_ENABLE_APP_KEYS ON
        RTL_ENABLE_APP_OPENGL ON
        RTL_ENABLE_APP_OPENGL_VSYNC ON
        RTL_ENABLE_APP_RESET ON
        RTL_ENABLE_APP_RESIZE ON
        RTL_ENABLE_APP_RESOURCES ON
        RTL_ENABLE_APP_SINGLETON ON
        RTL_ENABLE_CHRONO_CLOCK ON
        RTL_ENABLE_HEAP ON
        RTL_ENABLE_OPENCL ON

        RTL_ENABLE_ASSERT ${RTL_ENABLE_ASSERT}
        RTL_ENABLE_LOG ${RTL_ENABLE_LOG}
        RTL_ENABLE_RUNTIME_CHECKS ${RTL_ENABLE_RUNTIME_CHECKS}
        RTL_ENABLE_RUNTIME_TESTS ${RTL_ENABLE_RUNTIME_TESTS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${OpenCL_LIBRARIES}
        rtl::rtl
        winmm
        opengl32
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
        res
        ${OpenCL_INCLUDE_DIRS}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        CLAPP_ENABLE_ARCHITECT_MODE=0
)

if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
