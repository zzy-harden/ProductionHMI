cmake_minimum_required(VERSION 3.20)
project(ProductionHMI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(ProductionHMI_core STATIC
    src/core/config/ProjectConfigLoader.cpp
    src/communication/modbus/ModbusCodec.cpp
)

target_include_directories(ProductionHMI_core PUBLIC src)
target_link_libraries(ProductionHMI_core PUBLIC Qt6::Core)

add_executable(ProductionHMI
    src/app/main.cpp
)

target_link_libraries(ProductionHMI PRIVATE ProductionHMI_core Qt6::Widgets)

if(MSVC)
    target_compile_options(ProductionHMI_core PRIVATE /W4 /permissive-)
    target_compile_options(ProductionHMI PRIVATE /W4 /permissive-)
else()
    target_compile_options(ProductionHMI_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ProductionHMI PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_ProjectConfigLoader
        tests/core/config/test_ProjectConfigLoader.cpp
    )
    target_link_libraries(test_ProjectConfigLoader PRIVATE ProductionHMI_core Qt6::Test)

    add_test(NAME ProjectConfigLoader COMMAND test_ProjectConfigLoader)

    add_executable(test_ModbusCodecEncode
        tests/communication/modbus/test_ModbusCodecEncode.cpp
    )
    target_link_libraries(test_ModbusCodecEncode PRIVATE ProductionHMI_core Qt6::Test)

    add_test(NAME ModbusCodecEncode COMMAND test_ModbusCodecEncode)
endif()
