cmake_minimum_required(VERSION 3.27)
project(3dhead)

set(CMAKE_CXX_STANDARD 17)
set(SFML_DIR "C:/Korysne/naukma/docs/sfml_lib/SFML/lib/cmake/SFML")

find_package(SFML 2.6 COMPONENTS graphics audio REQUIRED)

add_executable(3dhead main.cpp
        tgaimage.h
        tgaimage.cpp
        model.h
        geometry.h
        model.cpp
        geometry.cpp)

target_link_libraries(3dhead sfml-graphics sfml-audio)