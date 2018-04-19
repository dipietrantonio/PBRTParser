#include "utils.h"

//
// read_file
// Load a text file as a string.
//
std::string read_file(std::string filename) {
	std::fstream inputFile;
	inputFile.open(filename, std::ios::in);
	std::stringstream ss;
	std::string line;
	while (std::getline(inputFile, line)) {
		ss << line << "\n";
	}
	// set filename and path properties
	auto textToParse = ss.str();
	inputFile.close();
	return textToParse;
}

//
// split
// splits a string according to one or more separator characters
//
std::vector<std::string> split(std::string text, std::string sep) {
	std::vector<std::string> results;
	std::stringstream ss;

	for (char c : text) {
		if (sep.find(c) != std::string::npos) {
			auto str = ss.str();
			if (str.size() > 0) {
				results.push_back(str);
				ss = std::stringstream();
			}
		}
		else {
			ss << c;
		}
	}
	auto str = ss.str();
	if (str.length() > 0)
		results.push_back(str);
	return results;
}

//
// standardize_path_separator
// replaces '\' with '/' in a path.
//
std::string standardize_path_separator(std::string path) {
	std::stringstream ss;
	for (char ch : path) {
		if (ch == '\\')
			ss << '/';
		else
			ss << ch;
	}
	return ss.str();
}

//
// get_path_and_filename
// Split a string representing an absolute or relative path in two parts,
// filename and path.
//
std::pair<std::string, std::string> get_path_and_filename(std::string file) {
	file = standardize_path_separator(file);
	// now divide path and filename
	auto p1 = file.find_last_of('/');
	std::string path, filename;
	if (p1 == std::string::npos) {
		path = ".";
		filename = file;
	}
	else {
		path = file.substr(0, p1);
		filename = file.substr(p1 + 1);
	}
	return std::make_pair(path, filename);
}

//
// concatenate_paths
//
std::string concatenate_paths(std::string position, std::string path) {
	if (path.length() == 0)
		return "";

	path = standardize_path_separator(path);
	if (path[0] == '/' || (path.length() > 3 && path[1] == ':' && path[2] == '/')) {
		return path;
	}
	else {
		// relative path
		std::stringstream builtPath;
		builtPath << position << "/" << path;
		return builtPath.str();
	}
}