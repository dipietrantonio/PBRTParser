# PBRTParser

A parser written to convert scene from pbrt (v3) format to [yocto](https://github.com/xelatihy/yocto-gl/tree/master/yocto) format (obj).

<img src="images/ecosys.png"/>

##Introduction

Computer Graphics' major concern is to render 3D scenes. Given a set of 3D objects with several 
properties (material, position, etc..), the goal is to produce an image which is how the set of objects is seen
by a camera positioned somewhere in the scene. The software that solve this task is called renderer, and the major 
## How to use
Once compiled using cmake, run
```
parse <file_to_parse> <output_obj>
```

## TODO
In order of importance

- Mix material: texture blending might have some transparency problem (check landscape/view-0 as an example).
- Colors in ecosys scene are a bit different.
- Shapes material's properties overriding and overall testing of materials.
- Checkerboard texture produce different colors (maybe related to solved env map problem).
- [pavilion-night] check the transparency issue of the left part of image, check why lights are much stronger than pbrt rendering.
- Test illumination, implement some hack for distant light.
- Uber material has index property that will use to create a constant texture for "eta" (seen in code)

## Rendering examples

You can find some rendering examples in the folder "images"-

## Credits
This software is intended to work with and is built upon the yocto library, which is developed at the following [repository](https://github.com/xelatihy/yocto-gl).
The yocto version used by this parser is [this](https://github.com/xelatihy/yocto-gl/tree/c06dd8014ca2de68911a16e744d4dd18c637a1bf).

This software uses in part some adapted code from pbrt path tracer.