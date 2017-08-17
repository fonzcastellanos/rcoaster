# Roller Coaster

Simulates a roller coaster using Catmull-Rom splines and OpenGL texture mapping

## Features
- Models the track with Catmull-Rom splines calculated from user-provided spline files
- Draws the splines using recursive division
- Renders a double rail with T-shaped cross sections and crossbars
- Allows user to provide the ground, sky, and crossbar textures
- Renders camera at a reasonable speed in a continuous path/orientation (~60-90 fps on early 2015 Macbook Pro)

## Build
- Execute `make`
- Compiles for macOS and Linux

## Usage
- `rcoaster <trackfile> <groundtexture> <skytexture> <crossbartexture>`
- Sample splines files are in the `splines` directory
- Sample texture files are in the `textures` directory
- Modify track.txt with spline files of your choice

## Built With
- [OpenGL](https://www.opengl.org/)
- [OpenGL Mathematics (GLM)](https://glm.g-truc.net)
- [Vega FEM's imageIO Library](http://run.usc.edu/vega/)

## Author
Alfonso Castellanos

## License
MIT @ [Alfonso Castellanos](https://github.com/TrulyFonz)

## Acknowledgements
Professor Jernej Barbic for his [computer graphics class](http://www-bcf.usc.edu/~jbarbic/cs420-s17/)
