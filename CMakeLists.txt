cmake_minimum_required(VERSION 3.10)

project(vld VERSION 1.0)

# Set the output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/setup)

# Add the main vld library
add_library(vld SHARED
    src/callstack.cpp
    src/dllspatches.cpp
    src/ntapi.cpp
    src/stdafx.cpp
    src/utility.cpp
    src/vld.cpp
    src/vldapi.cpp
    src/vldheap.cpp
    src/vld_hooks.cpp
)

# Set C++ standard to C++17
target_compile_features(vld PUBLIC cxx_std_17)

# Define Unicode for the project
target_compile_definitions(vld PRIVATE UNICODE _UNICODE)

# Add precompiled header
target_precompile_headers(vld PRIVATE src/stdafx.h)

# Set the configurations
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Compiler and linker settings
foreach(config IN ITEMS Debug Release)
    foreach(arch IN ITEMS Win32 x64)
        set_target_properties(vld PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config}_${arch} ${CMAKE_BINARY_DIR}/bin/${arch}/${config}
            LIBRARY_OUTPUT_DIRECTORY_${config}_${arch} ${CMAKE_BINARY_DIR}/lib/${arch}/${config}
            ARCHIVE_OUTPUT_DIRECTORY_${config}_${arch} ${CMAKE_BINARY_DIR}/lib/${arch}/${config}
        )
    endforeach()
endforeach()

# Compiler options
target_compile_options(vld PRIVATE
    $<$<CONFIG:Debug>:/Od /D_DEBUG /MTd /W4 /std:c++17 /WX>
    $<$<CONFIG:Release>:/O2 /DNDEBUG /MT /W4 /std:c++17 /WX>
)

# Linker options
target_link_options(vld PRIVATE
    $<$<CONFIG:Debug>:/DEBUG /SUBSYSTEM:WINDOWS>
    $<$<CONFIG:Release>:/OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS>
)

# Additional dependencies
target_link_libraries(vld PRIVATE Onecore.lib psapi.lib)

# Post-build event
add_custom_command(TARGET vld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vld> ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/$<TARGET_FILE_NAME:vld>.lib
)
