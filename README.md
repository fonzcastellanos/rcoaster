# rcoaster

![License](https://img.shields.io/github/license/fonzcastellanos/rcoaster)

An OpenGL-powered roller coaster simulator with configurable spline-based tracks.

![Demo](demo.gif)

## Description 

- Catmull-Rom splines are evaluated via recursive subdivision to determine the camera pose and track model along the spline.
    - Each spline is defined by the control points found in the input spline file.
    - The camera moves in a continuous path and orientation.
- The track model consists of a pair of rails and a sequence of crossties.
    - A rail has a T-shaped cross section.
    - All neighboring pairs of crossties are separated by the same distance.
- Textures are mapped to the models for the sky, ground, and crossties.
    - The image files corresponding to the textures are designated via command-line parameters.
- The scene was displayed at 60 frames per second on the Retina display of a 13-inch Macbook Pro from early 2015.
    - Vertical synchronization seemed to be enabled by default on the Macbook Pro.

## Build Requirements

- Operating system: macOS or Linux
- C++ compiler supporting at least C++17
- CMake >= 3.2
- OpenGL >= 3.2

My development environment and tooling:
- Hardware: MacBook Pro (Retina, 13-inch, Early 2015)
- Operating system: macOS Mojave == 10.14.16
- C++ compiler: Clang == 10.0.01
- CMake == 3.25.1
- OpenGL == 4.1

I built `rcoaster` on a Linux machine as well:
- Operating system: Debian == 11.7
- C++ compiler: gcc == 10.2.1
- CMake == 3.26.3
- OpenGL == 4.5

Make sure to install the following packages on Linux:
- libglew-dev
- freeglut3-dev

## Build Steps

The build system must be generated before the project can be built. From the project directory, generate the build system:
```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE:STRING=Release ..
```
The `-G` option is omitted in `cmake -DCMAKE_BUILD_TYPE:STRING=Release ..`, so CMake will choose a default build system generator type based on your platform. To learn more about generators, see the [CMake docs](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

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

```
./build/rcoaster [options...] <track-file> <ground-texture> <sky-texture> <crossbar-texture>
```

Options:
- `--max-spline-segment-len <length>`
    - The maximum length of each spline segment generated from recursive subdivision.
    - The default option argument is 0.5.
- `--camera-speed <speed>`
    - The camera movement rate in spline segments per second.
    - The default option argument is 100.
- `--screenshot-filename-prefix <prefix>`
    - The filename prefix of any screenshots generated.
    - Screenshot filenames follow the format `<prefix>_<count>.jpg`, where `count` is formatted as a 3 digit integer.
        - For example, if `prefix` is "screenshot" and `count` is 17, then the filename would be "screenshot_017.jpg"
    - The default option argument is "screenshot".
- `--screenshot-directory-path <path>`
    - The directory path where any screenshots taken will be saved.
    - The default option argument is ".", which is the current working directory.
- `--verbose <verbose_output>`
    - An option argument of 1 enables verbose output to `stdout`, and an option argument of 0 disables it. 
    - The default option argument is 0.

Any screenshots taken are saved as JPEG files. Video is a series of screenshots. To take a single screenshot, press `i`. To start recording video, press `v`. To stop recording video, press `v` again.

To exit the program, have the window in focus and press `ESC`. You can also terminate the program by pressing `CTRL + C` in the terminal.

### Example

```sh
./build/rcoaster track.txt textures/grass.jpg textures/sky.jpg textures/wood.jpg
```

### Track File

A track file lists the spline files to load to create the track. The first line is the number of spline files to load. Each subsequent line is a path to a spline file. Paths may be absolute or relative. Relative paths are relative to the current working directory of the `rcoaster` process.

Currently only the first spline file listed is used.

An example track file is [`track.txt`](track.txt). 

### Spline Files

A spline file contains the control points of a spline. The first line consists of two space-separated fields: the number of control points and a number indicating the type of spline. Each subsequent line is a control point.

Currently the only supported spline type are Catmull-Rom splines. Use `0` in the spline file to indicate a Catmull-Rom spline.

Example spline files are in the directory `splines`. I tailored the spline file [`custom.sp`](splines/custom.sp) to produce a track optimized for my hard-coded scene size, ground size, and sky box size. The other spline files were for initial testing testing and unfortunately produce tracks that are too small for my current configuration.
 
### Texture Files

Textures are provided as either JPEG or PNG files. Example texture files are in the `textures` directory.

## References
- Calculation of Reference Frames along a Space Curve
    - By Jules Bloomenthal
    - Bloomenthal gives credit to Ken Sloan for the technique I implemented to propagate reference frames along the spline.
