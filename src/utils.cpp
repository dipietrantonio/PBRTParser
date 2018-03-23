#include "utils.h"
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
// get_path_and_filename
// Split a string representing an absolute or relative path in two parts,
// filename and path.
//
std::pair<std::string, std::string> get_path_and_filename(std::string file) {
	// first, replace all '\' with '/', if any
	std::stringstream ss;
	for (char ch : file) {
		if (ch == '\\')
			ss << '/';
		else
			ss << ch;
	}
	file = ss.str();
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
// join_string_int
// merge a string and an integer (used to create unique names)
//
std::string join_string_int(char *pref, int size) {
	char buff[100];
	sprintf(buff, "%s_%d", pref, size);
	return std::string(buff);
}
