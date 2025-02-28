cmake_minimum_required(VERSION 3.17)

option(MV_UNITY_BUILD "Combine target source files into batches for faster compilation" OFF)

# -----------------------------------------------------------------------------
# DualView Plugin
# -----------------------------------------------------------------------------
PROJECT("DualViewPlugin")

# -----------------------------------------------------------------------------
# CMake Options
# -----------------------------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWIN32 /EHsc /MP /permissive- /Zc:__cplusplus")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
endif(MSVC)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Widgets WebEngineWidgets OpenGL OpenGLWidgets REQUIRED)

find_package(ManiVault COMPONENTS Core PointData ClusterData CONFIG)

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
# Define the plugin sources
set(PLUGIN_SOURCES
    src/DualViewPlugin.h
    src/DualViewPlugin.cpp
    src/ChartWidget.h
    src/ChartWidget.cpp
    src/DualViewPlugin.json
	src/ScatterPlotWidget.h
    src/ScatterPlotWidget.cpp
	src/EmbeddingLinesWidget.h
	src/EmbeddingLinesWidget.cpp
	src/MyShader.h
	src/MyShader.cpp
)

set(Actions
    src/Actions/SettingsAction.h
    src/Actions/SettingsAction.cpp
    src/Actions/EmbeddingAPointPlotAction.h
    src/Actions/EmbeddingAPointPlotAction.cpp
	src/Actions/EmbeddingBPointPlotAction.h
    src/Actions/EmbeddingBPointPlotAction.cpp
	src/Actions/ColoringActionB.h
    src/Actions/ColoringActionB.cpp
	src/Actions/ColoringActionA.h
    src/Actions/ColoringActionA.cpp
	src/Actions/SelectionAction.h
	src/Actions/SelectionAction.cpp
	src/Actions/LoadedDatasetsAction.h
	src/Actions/LoadedDatasetsAction.cpp
	src/Actions/GenePickerAction.h
	src/Actions/GenePickerAction.cpp
	src/Actions/DimensionSelectionAction.h
	src/Actions/DimensionSelectionAction.cpp
	src/Actions/MyOptionAction.h
    src/Actions/MyOptionAction.cpp
	src/Actions/ScalarAction.h
    src/Actions/ScalarAction.cpp
    src/Actions/ScalarSourceAction.h
    src/Actions/ScalarSourceAction.cpp
	src/Actions/PointPlotAction.h
    src/Actions/PointPlotAction.cpp
	src/Actions/PointPlotActionB.h
    src/Actions/PointPlotActionB.cpp
	src/Actions/NavigationAction.h
    src/Actions/NavigationAction.cpp
	src/Actions/EnrichmentAction.h
    src/Actions/EnrichmentAction.cpp
)

set(Models
    src/Models/ScalarSourceModel.h
    src/Models/ScalarSourceModel.cpp
)

set(Compute
    src/Compute/EnrichmentAnalysis.h
	src/Compute/EnrichmentAnalysis.cpp
)

set(PLUGIN_MOC_HEADERS
    src/DualViewPlugin.h
)

set(WEB
    res/dual_chart/test_bipartite.html
    res/dual_chart/bipartite_chart.tools.js
    res/dual_chart/d3-bipartite.js
    res/js_libs/qwebchannel.tools.js
)

set(SHADERS
    res/shaders/EmbeddingLines.frag
    res/shaders/EmbeddingLines.vert
	res/shaders/EmbeddingLines.geom
)

set(AUX
    res/dual_chart.qrc
)

qt6_add_resources(RESOURCE_FILES res/dual_chart.qrc)

source_group( Plugin FILES ${PLUGIN_SOURCES})
source_group( Actions FILES ${Actions})
source_group( Models FILES ${Models})
source_group( Compute FILES ${Compute})
source_group( Web FILES ${WEB})
source_group( Shaders FILES ${SHADERS})
source_group( Aux FILES ${AUX})


# -----------------------------------------------------------------------------
# CMake Target
# -----------------------------------------------------------------------------
# Create dynamic library for the plugin
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES} ${Actions} ${Models} ${Compute} ${SHADERS} ${AUX} ${WEB} ${RESOURCE_FILES})

# -----------------------------------------------------------------------------
# Target include directories
# -----------------------------------------------------------------------------
# Include ManiVault headers, including system data plugins
target_include_directories(${PROJECT_NAME} PRIVATE "${ManiVault_INCLUDE_DIR}")

# -----------------------------------------------------------------------------
# Target properties
# -----------------------------------------------------------------------------
# Request C++17
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Enable unity build
if(MV_UNITY_BUILD)
    set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

# -----------------------------------------------------------------------------
# Target library linking
# -----------------------------------------------------------------------------
# Link to Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::WebEngineWidgets)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::OpenGL)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::OpenGLWidgets)

# Link to ManiVault and data plugins
target_link_libraries(${PROJECT_NAME} PRIVATE ManiVault::Core)
target_link_libraries(${PROJECT_NAME} PRIVATE ManiVault::PointData)
target_link_libraries(${PROJECT_NAME} PRIVATE ManiVault::ClusterData)

# -----------------------------------------------------------------------------
# Target installation
# -----------------------------------------------------------------------------
# Install the shared plugin libary to the "Plugins" folder in the ManiVault install directory
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION Plugins COMPONENT PLUGINS # Windows .dll
    LIBRARY DESTINATION Plugins COMPONENT PLUGINS # Linux/Mac .so
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        --install ${CMAKE_CURRENT_BINARY_DIR}
        --config $<CONFIGURATION>
        --prefix ${ManiVault_INSTALL_DIR}/$<CONFIGURATION>
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    FOLDER ViewPlugins
)

# -----------------------------------------------------------------------------
# Miscellaneous
# -----------------------------------------------------------------------------
# Automatically set the debug environment (command + working directory) for MSVC
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<IF:$<CONFIG:DEBUG>,${ManiVault_INSTALL_DIR}/debug,${ManiVault_INSTALL_DIR}/release>)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_COMMAND $<IF:$<CONFIG:DEBUG>,${ManiVault_INSTALL_DIR}/debug/ManiVault\ Studio.exe,${ManiVault_INSTALL_DIR}/release/ManiVault\ Studio.exe>)
endif()
