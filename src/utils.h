#ifndef __MYUTILS__
#define __MYUTILS__
#include <vector>
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>

//
// read_file
// Load a text file as a string.
//
std::string read_file(std::string filename);

//
// split
// splits a string according to one or more separator characters
//
std::vector<std::string> split(std::string text, std::string sep = " ");

//
// get_path_and_filename
// Split a string representing an absolute or relative path in two parts,
// filename and path.
//
std::pair<std::string, std::string> get_path_and_filename(std::string file);

//
// standardize_path_separator
// replaces '\' with '/' in a path.
//
std::string standardize_path_separator(std::string path);

//
// concatenate_paths
//
std::string concatenate_paths(std::string position, std::string path);

#endif