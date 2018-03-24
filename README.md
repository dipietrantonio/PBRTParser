# PBRTParser

A parser written to convert pbrt scenes to yocto scenes (obj).

## How to use
Once compiled using cmake, run
```
parse <file_to_parse> <output_obj>
```

## TODO

- Fix memory leaks using shared pointers.
- Implement spectrum type (convert it to rgb)
- Better way to parse material parameters
- Textures: detect if there are textures that are not used
- Testures: implement mix, scale textures.
- Distant lightsource
- Metal material


## Credits
This software is intended to work with the yocto library, which is developed at the following repository: https://github.com/xelatihy/yocto-gl.