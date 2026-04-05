cmake_minimum_required(VERSION 3.10)
project(PlasticParticleVE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)

# Library root paths
set(OSG_BUILD_ROOT "C:/Libraries/openscenegraph/build")
set(OSG_INCLUDE_ROOT "C:/Libraries/openscenegraph/install/include")
set(NetCDF_ROOT "C:/Libraries/netcdf/install")
set(NetCDF_CXX_ROOT "C:/Libraries/netcdf-cxx/install")
set(HDF5_ROOT "C:/Libraries/hdf5/install")
set(ZLIB_ROOT "C:/Libraries/zlib/install")
set(FFTW_ROOT "C:/Libraries/fftw/installed")
set(PNG_ROOT "C:/Libraries/libpng/installed")

# Include directories
include_directories(
        ${OSG_INCLUDE_ROOT}
        ${NetCDF_ROOT}/include
        ${NetCDF_CXX_ROOT}/include
        ${HDF5_ROOT}/include
        ${ZLIB_ROOT}/include
)

# Library directories
link_directories(
        ${OSG_BUILD_ROOT}/lib
        ${NetCDF_ROOT}/lib
        ${NetCDF_CXX_ROOT}/lib
        ${HDF5_ROOT}/lib
        ${ZLIB_ROOT}/lib
)

# Source files
add_executable(OceanGrid
        src/main.cpp
        src/dataloaders/LoadNC.cpp
        src/dataloaders/LoadNC.h
        src/dataloaders/DataConverter.cpp
        src/dataloaders/DataConverter.h
        src/scene/SurfacePlane.cpp
        src/scene/SurfacePlane.h
        src/scene/BathymetryMesh.cpp
        src/scene/BathymetryMesh.h
        src/scene/FogController.h
        src/scene/DynamicBackgroundController.h
        src/scene/Particle.cpp
        src/scene/Particle.h
        src/scene/FollowCameraManipulator.h
        src/scene/TogglePlaybackHandler.h
        src/scene/Currents.cpp
        src/scene/Currents.h
        src/intro/Intro.cpp
        src/intro/Intro.h
        src/intro/AdvanceIntroHandler.h
        src/globe/globe.cpp
        src/globe/globe.h
        src/globe/trajectories.cpp
        src/globe/trajectories.h
        src/globe/density.cpp
        src/globe/density.h
        src/globe/travel.cpp
        src/globe/travel.h
        src/globe/controller.cpp
        src/globe/controller.h
        src/globe/utils_netcdf.h
        src/globe/utils_geo_color.h
        src/globe/ZoomTransitionController.cpp
        src/globe/ZoomTransitionController.h
        src/globe/timeCounter.cpp
        src/globe/timeCounter.h
        src/globe/many.cpp
        src/globe/many.h
        src/globe/dynamicScale.cpp
        src/globe/dynamicScale.h
        src/globe/spin.cpp
        src/globe/spin.h
        src/cameras/RenderCompositeSystem.cpp
        src/cameras/RenderCompositeSystem.h
        src/cameras/GlobeSequence.cpp
        src/cameras/GlobeSequence.h
        src/cameras/MainScene.cpp
        src/cameras/MainScene.h
        src/cameras/PhaseCoordinator.cpp
        src/cameras/PhaseCoordinator.h
        src/cameras/HandOffs.cpp
        src/cameras/HandOffs.h
)

target_include_directories(OceanGrid PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(OceanGrid
        osg
        osgDB
        osgGA
        osgViewer
        osgText
        osgUtil
        osgShadow
        OpenThreads
        netcdf-cxx4
        netcdf
        hdf5
        hdf5_hl
        zlib
        opengl32
)

# Helpful output
message(STATUS "Using OpenSceneGraph from: ${OSG_BUILD_ROOT}")
message(STATUS "Using NetCDF from: ${NetCDF_ROOT}")
message(STATUS "Using NetCDF-CXX from: ${NetCDF_CXX_ROOT}")
message(STATUS "Using HDF5 from: ${HDF5_ROOT}")
message(STATUS "Using ZLIB from: ${ZLIB_ROOT}")
message(STATUS "Using FFTW from: ${FFTW_ROOT}")
message(STATUS "Using PNG from: ${PNG_ROOT}")

# Copy required DLLs next to the executable
set(DLLS_TO_COPY
        "${NetCDF_ROOT}/bin/netcdf.dll"
        "${NetCDF_CXX_ROOT}/bin/netcdf-cxx4.dll"
        "${HDF5_ROOT}/bin/hdf5.dll"
        "${HDF5_ROOT}/bin/hdf5_hl.dll"
        "${ZLIB_ROOT}/bin/zlib1.dll"
        "${FFTW_ROOT}/bin/fftw3.dll"
        "${PNG_ROOT}/bin/libpng16.dll"
)

# OSG DLLs
file(GLOB OSG_DLLS "${OSG_BUILD_ROOT}/bin/*.dll")
list(APPEND DLLS_TO_COPY ${OSG_DLLS})

foreach(DLL ${DLLS_TO_COPY})
    if(EXISTS "${DLL}")
        add_custom_command(TARGET OceanGrid POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" $<TARGET_FILE_DIR:OceanGrid>
        )
    else()
        message(WARNING "DLL not found, skipping copy: ${DLL}")
    endif()
endforeach()

# Copy OSG plugin folder next to executable
set(OSG_PLUGINS_DIR "${OSG_BUILD_ROOT}/bin/osgPlugins-3.6.5")

if(EXISTS "${OSG_PLUGINS_DIR}")
    add_custom_command(TARGET OceanGrid POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OSG_PLUGINS_DIR}"
            "$<TARGET_FILE_DIR:OceanGrid>/osgPlugins-3.6.5"
    )
else()
    message(WARNING "OSG plugin folder not found: ${OSG_PLUGINS_DIR}")
endif()

message(STATUS "Configured post-build steps to copy DLLs, plugins, and resources.")