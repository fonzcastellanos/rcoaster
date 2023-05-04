# Roller Coaster

![License](https://img.shields.io/github/license/fonzcastellanos/roller_coaster)

Simulates a roller coaster using Catmull-Rom splines and texture mapping.

![](demo.gif)

## Features
- Models the track with Catmull-Rom splines calculated from user-provided spline files
- Draws the splines using recursive division
- Renders a double rail with T-shaped cross sections and crossbars
- Allows user to provide the ground, sky, and crossbar textures
- Renders camera at a reasonable speed in a continuous path/orientation (~60-90 fps on early 2015 Macbook Pro)

## Build Requirements
- Operating system: macOS or Linux
- C++ compiler
    - Must support at least C++17
- CMake >= 3.2

My development environment and tooling:
- Hardware: MacBook Pro (Retina, 13-inch, Early 2015)
- Operating system: macOS Mojave (version 10.14.16)
- C++ compiler: Clang (version 10.0.01)
- CMake (version 3.25.1)


## Build Steps
The build system must be generated before the project can be built. From the project directory, generate the build system.
```sh
mkdir build
cd build
cmake ..
```
The `-G` option is omitted in `cmake ..`, so CMake will choose a default build system generator type based on your platform. To learn more about generators, see the [CMake docs](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

After having generated the build system, go back to the project directory
```sh
cd ..
```
and build *all* the targets of the project. 
```sh
cmake --build build --config Release
```

The built targets are placed in the directory `build`.

There is only one executable target: `rcoaster`.

## Usage
```sh
./build/rcoaster <track_file> <ground_texture> <sky_texture> <crossbar_texture>
```
### Example
```sh
./build/rcoaster track.txt textures/grass.jpg textures/sky.jpg textures/wood.jpg
```

### Track File
The track file allows you to designate which spline files should be loaded to create the track. 

The example track file is [`track.txt`](track.txt), which looks like this (except without the comments)
 ```
1 <--- number of spline files to load in the order listed
splines/custom.sp <-- path to a spline file
```
Sample splines files are in the `splines` directory. I tailored the spline file [`custom.sp`](splines/custom.sp) to produce a track of reasonable size for my camera movement speed and thus it provides the best experience. The other spline files were provided to all students for testing and unfortunately produce tracks that are too small for my configuration.
 
 ### Texture Files
The texture files are JPEG files. Sample texture files are in the `textures` directory.

## Built With
- [OpenGL](https://www.opengl.org/)
- [OpenGL Mathematics (GLM)](https://glm.g-truc.net)
- [stb](https://github.com/nothings/stb)
    - stb_image.h
    - stb_image_write.h

## Acknowledgements
Professor Jernej Barbic for his [computer graphics class](https://viterbi-web.usc.edu/~jbarbic/cs420-s17)