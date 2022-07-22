cmake_minimum_required(VERSION 3.14)
project(PluginFinder)

sofa_find_package(SofaFramework REQUIRED)
sofa_find_package(SofaSimulation REQUIRED)
sofa_find_package(SofaGuiCommon REQUIRED)
sofa_find_package(SofaBase REQUIRED)

set(SOFABENCHMARK_SRC "src")

set(HEADER_FILES "")
set(SOURCE_FILES ${SOFABENCHMARK_SRC}/Main.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC SofaFramework SofaSimulation SofaGuiCommon SofaBase)
target_include_directories(${PROJECT_NAME} PUBLIC ${SOFABENCHMARK_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE extlibs/cxxopts-2.2.1/include)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Applications)
