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

macro(RemoveDebugCXXFlag flag)
    string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endmacro()

macro(RemoveCXXFlag flag)
    string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endmacro()

find_package(OpenCL REQUIRED)
find_package(rtl REQUIRED)

aux_source_directory(src/ SOURCES)
aux_source_directory(src/clapp SOURCES)

add_executable(${PROJECT_NAME} 
    ${CMAKE_CURRENT_LIST_DIR}/res/cl/program.i
    ${CMAKE_CURRENT_LIST_DIR}/res/clapp.rc
    ${SOURCES}
)

target_link_libraries(${PROJECT_NAME} 
        ${OpenCL_LIBRARIES} 
        rtl::rtl
        winmm 
        opengl32)

target_include_directories(${PROJECT_NAME} 
    PRIVATE
        src
        res
        ${OpenCL_INCLUDE_DIRS}
)

if( MSVC )
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_LIST_DIR}/res/cl/program.i
        COMMAND
            CL /P /C /D PREPROCESS program.c
        DEPENDS 
            ${CMAKE_CURRENT_LIST_DIR}/res/clapp.rc
        WORKING_DIRECTORY 
            ${CMAKE_CURRENT_LIST_DIR}/res/cl
        COMMENT
            "Preprocessing OpenCL program"
    )  

    RemoveDebugCXXFlag("/RTC1")
    RemoveCXXFlag("/EHsc")
    message("MSVC flags: ${CMAKE_CXX_FLAGS}")

    # MSVC compiler inserts call to 'memset' function when generates the code.
    # But it's impossible to define custom 'memset' without disabling whole program 
    # optimization. So, have to link with VC runtime to give linker 'memset' implementation.
    target_link_libraries(${PROJECT_NAME} 
         libvcruntime
         libcmt
    )

    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            RTL_ENABLE_ASSERT=$<BOOL:${RTL_ENABLE_ASSERT}>
            RTL_ENABLE_LOG=$<BOOL:${RTL_ENABLE_LOG}>
            RTL_ENABLE_RUNTIME_CHECKS=$<BOOL:${RTL_ENABLE_RUNTIME_CHECKS}>
            RTL_ENABLE_RUNTIME_TESTS=$<BOOL:${RTL_ENABLE_RUNTIME_TESTS}>

            RTL_ENABLE_APP=1
            RTL_ENABLE_APP_AUDIO=1
            RTL_ENABLE_APP_CLOCK=1
            RTL_ENABLE_APP_CURSOR=1
            RTL_ENABLE_APP_FULLSCREEN=0
            RTL_ENABLE_APP_KEYS=1            
            RTL_ENABLE_APP_OSD=0
            RTL_ENABLE_APP_OPENGL=1
            RTL_ENABLE_APP_OPENGL_VSYNC=1
            RTL_ENABLE_APP_RESIZE=1
            RTL_ENABLE_APP_RESOURCES=1
            RTL_ENABLE_APP_SCREEN_BUFFER=0
            RTL_ENABLE_APP_SINGLETON=1
            RTL_ENABLE_HEAP=1
            RTL_ENABLE_MEMSET=0
            RTL_ENABLE_OPENCL=1

            CLAPP_ENABLE_ARCHITECT_MODE=0

            UNICODE
        )

    target_compile_options(${PROJECT_NAME}
        PRIVATE
            /EHa-       # Disable standard C++ stack unwinding. 
            /EHs-       # Disable C++ exception handling (no SEH exceptions).
            /FA         # Creates a listing file containing assembler code.
            /GF         # Enables string pooling
            /GL         # Enables whole program optimization.
            /GR-        # Disables run-time type information (RTTI).
            /Gs32768    # The number of bytes that local variables can occupy before a stack probe is initiated
            /GS-        # Disables checks buffer security.
        #   /Gy-        # Disables function-level linking
            /Gw         # Enables whole-program global data optimization.
            /O1         # Creates small code
        #   /Ob2        # Controls inline expansion
            /Oi-        # Generates intrinsic functions
            /Os         # Favors small code.
            /Oy         # Omits frame pointer. (x86 only)
            /Wall       # Enable all warnings, including warnings that are disabled by default.
            /wd4505     # 'function' : unreferenced local function has been removed
            /wd4514     # 'function' : unreferenced inline function has been removed
            /wd4625     # 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
            /wd4626     # 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted          
            /wd4577     # 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
            /wd4710     # 'function' : function not inlined
            /wd4711     # function 'function' selected for inline expansion
            /wd5039     # pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception.
            /wd5045     # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
            /WX         # Treat warnings as errors.

    )

    target_link_options(${PROJECT_NAME}
        PRIVATE       
            /DYNAMICBASE:NO                                 # Specifies whether to generate an executable image that's rebased at load time by using the address space layout randomization (ASLR) feature.
            /FIXED                                          # Creates a program that can be loaded only at its preferred base address.
            /INCREMENTAL:NO                                 # Controls incremental linking.
            /LTCG                                           # Specifies link-time code generation.
            /MANIFEST:NO                                    # Creates a side-by-side manifest file and optionally embeds it in the binary.
            /MAP                                            # Creates a mapfile.
            /NOCOFFGRPINFO
            /NODEFAULTLIB                                   # Ignores all (or the specified) default libraries when external references are resolved.
            /SAFESEH:NO                                     # Specifies that the image will contain a table of safe exception handlers
            /SUBSYSTEM:WINDOWS
            /STUB:${CMAKE_CURRENT_LIST_DIR}/lib/stub.bin    # Attaches an MS-DOS stub program to a Win32 program.
    )
endif()
