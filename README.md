# rcoaster

![License](https://img.shields.io/github/license/fonzcastellanos/rcoaster)

A simulated roller coaster experience built with OpenGL.

![Demo](demo.gif)

## Description 

- The track model and camera path are generated from using recursive subdivision to evaluate Catmull-Rom splines, each defined by the control points in its corresponding input spline file.
- The rails have T-shaped cross sections.
- The crossties are placed between the rails at a regular distance interval.
- Textures are mapped to the models for the sky, ground, and crossties.
    - The image files corresponding to the textures are designated via command-line parameters.
- The camera follows a continuous path and orientation.
- The scene was displayed at 60 frames per second on a Retina display of a 13-inch Macbook Pro from early 2015.
    - Vertical synchronization seemed to be enabled by default.

## Build Requirements

- Operating system: macOS or Linux
- C++ compiler supporting at least C++17
- CMake >= 3.2
- OpenGL >= 3.3

My development environment and tooling:
- Hardware: MacBook Pro (Retina, 13-inch, Early 2015)
- Operating system: macOS Mojave == 10.14.16
- C++ compiler: Clang == 10.0.01
- CMake == 3.25.1
- OpenGL == 4.1

## Build Steps

The build system must be generated before the project can be built. From the project directory, generate the build system:
```sh
mkdir build
cd build
cmake ..
```
The `-G` option is omitted in `cmake ..`, so CMake will choose a default build system generator type based on your platform. To learn more about generators, see the [CMake docs](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

After having generated the build system, go back to the project directory:
```sh
cd ..
```

Build *all* the targets of the project:
```sh
cmake --build build --config Release
```

The built targets are placed in the directory `build`. There is only one executable target: `rcoaster`.

## Usage

```sh
./build/rcoaster <track_file> <ground_texture> <sky_texture> <crossbar_texture>
```

### Example

```sh
./build/rcoaster track.txt textures/grass.jpg textures/sky.jpg textures/wood.jpg
```

### Track File

The track file lists the spline files to load to create the track. The first line is the number of spline files to load. Each subsequent line is a path to a spline file. Paths may be absolute or relative. Relative paths are relative to the current working directory of the `rcoaster` process.

An example track file is [`track.txt`](track.txt). 

### Spline Files

A spline file contains the control points of a spline. The first line consists of two space-separated fields: the number of control points and a number indicating the type of spline. Each subsequent line is a control point.

The only supported spline type are Catmull-Rom splines. Use `0` in the spline file to indicate a Catmull-Rom spline.

Example spline files are in the directory `splines`. I tailored the spline file [`custom.sp`](splines/custom.sp) to produce a track optimized for my hard-coded scene size, ground size, sky box size, and camera movement speed. The other spline files were for initial testing testing and unfortunately produce tracks that are too small for my current configuration.
 
### Texture Files

Textures are provided as either JPEG or PNG files. Example texture files are in the `textures` directory.