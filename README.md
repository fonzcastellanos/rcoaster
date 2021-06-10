# Roller Coaster

Simulates a roller coaster using Catmull-Rom splines and OpenGL texture mapping

![](demo.gif)

## Features
- Models the track with Catmull-Rom splines calculated from user-provided spline files
- Draws the splines using recursive division
- Renders a double rail with T-shaped cross sections and crossbars
- Allows user to provide the ground, sky, and crossbar textures
- Renders camera at a reasonable speed in a continuous path/orientation (~60-90 fps on early 2015 Macbook Pro)

## Prerequisites
- OS: macOS, Linux
- Programs: g++, make

## Build
```console
cd src/
make
```
Produces `rcoaster` executable in `src/`.
## Usage
```console
rcoaster <track_file> <ground_texture> <sky_texture> <crossbar_texture>
```
### Example
```console
./rcoaster track.txt textures/grass.jpg textures/sky.jpg textures/wood.jpg
```

### Track File
The track file allows you to designate which spline files should be loaded to create the track. 

The example track file is `src/track.txt`, which looks like this (except without the comments)
 ```console
1 <--- number of spline files to load in the order listed
splines/custom.sp <-- path to a spline file
```
Sample splines files are in `src/splines/`. I tailored the spline file `custom.sp` to produce a track of reasonable size for my camera movement speed and thus it provides the best experience. The other spline files were provided to all students for testing and unfortunately produce tracks that are too small for my configuration.
 
 ### Texture Files
The texture files are JPG files. Sample texture files are in `src/textures/`.


## Built With
- [OpenGL](https://www.opengl.org/)
- [OpenGL Mathematics (GLM)](https://glm.g-truc.net)
- [Vega FEM's imageIO Library](http://barbic.usc.edu/vega/)

## Author
Alfonso Castellanos

## License
MIT @ [Alfonso Castellanos](https://github.com/fonzcastellanos)

## Acknowledgements
Professor Jernej Barbic for his [computer graphics class](http://barbic.usc.edu/cs420-s17/)