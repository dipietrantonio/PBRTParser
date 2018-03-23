#ifndef __MYUTILS__
#define __MYUTILS__
#include <vector>
#include <string>
#include <sstream>
// ======================================================================================
//                                UTILITIES
// ======================================================================================

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
// join_string_int
// merge a string and an integer (used to create unique names)
//
std::string join_string_int(char *pref, int size);

#endif