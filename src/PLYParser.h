#ifndef __PLY_PARSER__
#define __PLY_PARSER__
#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <exception>
#include <locale>
#include <vector>
#include "utils.h"

#define YGL_IMAGEIO_IMPLEMENTATION 1
#define YGL_OPENGL 0
#include "../yocto/yocto_gl.h"

//
// parse_ply
// Parse a PLY file format and fills a shape structure (must be altready allocated).
// Note: works for binary and ascii, but only when faces and vertex elements are present.
// TODO: a less ugly implementation (maybe is better a third party lib).
//
bool parse_ply(std::string filename, ygl::shape *shape);
#endif