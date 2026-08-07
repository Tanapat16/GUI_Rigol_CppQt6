cmake_minimum_required(VERSION 3.16)
project(RigolScopeGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --------------------------------------------------------------------------
# Qt6 Path Setup
# หาก CMake หา Qt6 ไม่เจอ ให้ปลดคอมเมนต์บรรทัดด้านล่างแล้วใส่ Path ของ Qt6 ในเครื่อง
# set(CMAKE_PREFIX_PATH "C:/Qt/6.x.x/msvc2019_64")
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# --------------------------------------------------------------------------
# NI-VISA Configuration
# --------------------------------------------------------------------------
option(USE_NIVISA "Enable real instrument connection via NI-VISA" ON)

if(USE_NIVISA)
    if(WIN32)
        set(NIVISA_INCLUDE_DIR "C:/Program Files/IVI Foundation/VISA/Win64/Include"
            CACHE PATH "Path to NI-VISA visa.h")
        set(NIVISA_LIB_DIR "C:/Program Files/IVI Foundation/VISA/Win64/Lib_x64/msvc"
            CACHE PATH "Path to NI-VISA visa64.lib")
        set(NIVISA_LIB_NAME "visa64")
    else()
        set(NIVISA_INCLUDE_DIR "/usr/include" CACHE PATH "Path to VISA headers")
        set(NIVISA_LIB_DIR "/usr/lib" CACHE PATH "Path to VISA lib")
        set(NIVISA_LIB_NAME "visa")
    endif()

    find_path(NIVISA_HEADER visa.h PATHS ${NIVISA_INCLUDE_DIR})
    find_library(NIVISA_LIB ${NIVISA_LIB_NAME} PATHS ${NIVISA_LIB_DIR})

    if(NIVISA_HEADER AND NIVISA_LIB)
        message(STATUS "NI-VISA found: header=${NIVISA_HEADER} lib=${NIVISA_LIB}")
        set(HAVE_NIVISA TRUE)
    else()
        message(WARNING "NI-VISA not found. Building with simulation-only mode.")
        set(HAVE_NIVISA FALSE)
    endif()
else()
    set(HAVE_NIVISA FALSE)
endif()

# --------------------------------------------------------------------------
# Sources (แก้เป็น main.cpp ไฟล์เดียวตามโครงสร้างใหม่)
# --------------------------------------------------------------------------
set(SOURCES
    main.cpp
)

add_executable(RigolScopeGUI ${SOURCES})

target_link_libraries(RigolScopeGUI PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

if(HAVE_NIVISA)
    target_include_directories(RigolScopeGUI PRIVATE ${NIVISA_INCLUDE_DIR})
    target_link_libraries(RigolScopeGUI PRIVATE ${NIVISA_LIB})
    target_compile_definitions(RigolScopeGUI PRIVATE HAVE_NIVISA=1)
else()
    target_compile_definitions(RigolScopeGUI PRIVATE HAVE_NIVISA=0)
endif()

if(WIN32)
    # หากต้องการปิดหน้าต่าง Console สีดำเมื่อรันโปรแกรม ให้เปิดใช้บรรทัดนี้
    # set_target_properties(RigolScopeGUI PROPERTIES WIN32_EXECUTABLE TRUE)
endif()