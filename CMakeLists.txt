cmake_minimum_required(VERSION 3.14)
project(PluginFinder)

sofa_find_package(Sofa.Core REQUIRED)
sofa_find_package(Sofa.GUI.Common REQUIRED)
sofa_find_package(Sofa.Simulation.Graph REQUIRED)

set(PLUGINFINDER_SRC "src")

set(HEADER_FILES ${PLUGINFINDER_SRC}/ErrorCountingMessageHandler.h)
set(SOURCE_FILES ${PLUGINFINDER_SRC}/Main.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC Sofa.Core Sofa.GUI.Common Sofa.Simulation.Graph)
target_include_directories(${PROJECT_NAME} PUBLIC ${PLUGINFINDER_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE extlibs/cxxopts-2.2.1/include)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Applications)
