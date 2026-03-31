cmake_minimum_required(VERSION 3.20)

project(OmniSimEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(omnisim_math STATIC
    math/src/vector2.cpp
)
target_include_directories(omnisim_math PUBLIC
    math/include
)

add_library(omnisim_simulation_interface STATIC
    simulation/src/simulation_factory.cpp
)
target_include_directories(omnisim_simulation_interface PUBLIC
    simulation/include
)

add_library(omnisim_engine STATIC
    engine/src/engine_loop.cpp
)
target_include_directories(omnisim_engine PUBLIC
    engine/include
)
target_link_libraries(omnisim_engine PUBLIC
    omnisim_simulation_interface
)

add_library(omnisim_projectile STATIC
    simulation/projectile/src/projectile_simulation.cpp
    simulation/projectile/src/terrain_profile.cpp
)
target_include_directories(omnisim_projectile PUBLIC
    simulation/projectile/include
)
target_link_libraries(omnisim_projectile PUBLIC
    omnisim_simulation_interface
    omnisim_math
)

add_executable(omnisim_cli
    app/main.cpp
)
target_link_libraries(omnisim_cli PRIVATE
    omnisim_engine
    omnisim_projectile
)

if(WIN32)
    add_executable(omnisim_projectile_2d WIN32
        app/projectile_2d_main.cpp
    )
    target_link_libraries(omnisim_projectile_2d PRIVATE
        omnisim_projectile
        gdi32
        user32
        psapi
    )
endif()
