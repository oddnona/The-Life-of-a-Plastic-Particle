# The Life of a Plastic Particle

This project simulates and visualizes the movement of plastic particles in the ocean using trajectory data. 
## Requirements

- Windows (this project is currently configured for windows, not portable cross-platform)
- Visual Studio (MSVC, C++17 support)
- CMake 3.10 or newer
- OpenGL
- OpenSceneGraph (osg, osgDB, osgGA, osgViewer, osgText, osgUtil, osgShadow, OpenThreads)
- NetCDF library
- NetCDF C++4
- HDF5
- zlib
- FFTW
- libpng

The project uses some manually installed libraries and hardcoded paths in CMakeLists.txt. These paths must be adjusted to match the local installation on another machine before building the project.
## Build Instructions

1. Clone or download the repository

2. Open a terminal (or use CLion/Visual Studio) 

3. Run CMake to configure the project:    
   cmake -S . -B build  
   cmake --build . --config Release

## Running the Project

After building, run:

OceanGrid.exe

Make sure the required data files (sa_100fragscale.nc, meshply.ply, tubes_7.ply) are present in these directories:
- OceanGrid/src/trajectories/sa_100fragscale.nc
- OceanGrid/src/coordinates/meshply.ply
- OceanGrid/src/coordinates/tubes_7.ply
## Project Structure

- OceanGrid/            Ocean data, simulation data, and grid handling
- src/                  Main application source code
- src/cameras           Cameras and Event sequence manager
- src/dataloaders       Handler for data loading
- src/globe             Globe animations (zoom out)
- src/intro             Intro frame
- src/scene             Bathy animations (zoom in)
- main.cpp              Entry point of the app

## Notes

- The project requires the included .nc data files to run correctly.
- File paths are currently relative and assume executable is run from the executable's directory.
- Certain animations will appear faster or slower depending on the device you are running it on. Animations like the underwater cutaway are fixed to play for 1 minute exactly, other animations like the single particle travel on the globe will be relative to the device, which can reflect in the HUD scale indicator mismatching the actual viewed scale. 
- The current program is in MONO mode (viewable in 2D only). For STEREO:

    - main.cpp:211:     true for stereo  

    - RenderCompositeSystem.cpp:60:      uncomment for stereo and - comment line 61

    - RenderCompositeSystem.cpp:99:      uncomment for stereo and comment line 100
## Author

Nona Bulubasa  
Bachelor Thesis Project