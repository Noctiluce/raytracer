<p align="center"><h1>Project Ray Tracer</h1></p>
<p align="right">26/05/2020</p>

Ray tracing project allowing to visualize a 3D scene and to calculate an image based on the rules of physics

## Features
- phong sphere, square, triangular lighting
- intersection sphere, square, triangle
- normal interpolation
- add 3d models
- add glass
- adding reflection
- added anti-aliazing effect
- adding soap bubble material
- addition of Bounding Volume Hierarchy

## Building
#### On Linux
**Prerequisite**: CMake

To build this program, download the source code using ``git clone https://github.com/Vulpinii/raytracer`` or directly the zip archive.

Then following these commands:
```shell script
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
./raytracer
```

#### On Windows
[instructions coming soon]

## Navigation
- KEY 1 : visualize spheres
- KEY 2 : visualize quad
- KEY 3 : visualize basic scene
- KEY 4 : visualize bubble scene
- KEY 5 : visualize 'fun' scene
    
- R : generate ray tracing
- mouse : manipulate the camera
	
- ECHAP : quit the program

## Gallery
#### Preview
<p align="center"><img src="https://github.com/Noctiluce/raytracer/blob/master/results/5.4_bubblessoftshadow_4338.png" alt="bubble" width="75%" /></p>
<p align="center"><img src="https://github.com/Noctiluce/raytracer/blob/master/results/6.1_rendufinal_7835.png" alt="fun" width="75%" /></p>
