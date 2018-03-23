#include "PLYParser.h"
//
// parse_ply
// Parse a PLY file format and returns a shape.
// Note: works for binary and ascii, but only when faces and vertex elements are present.
// TODO: a less ugly implementation (maybe is better a third party lib).
//
bool parse_ply(std::string filename, ygl::shape **shape) {

	auto shp = new ygl::shape();
	// first, parse the header
	std::ifstream plyFile;
	plyFile.open(filename, std::ios::in | std::ios::binary);
	// current line read
	std::string line;

	// we are interested in vertices and faces (for now)
	// number of vertices (equivalent to shape.pos)
	int n_vertices = 0;
	// per vertex properties name
	std::vector<std::string> vertex_prop{};
	// faces (triangles)
	int n_faces = 0;
	int n_vert_per_face = 0;
	// list of elements present (should be only vertices and faces)
	std::vector<std::string> elements;
	bool is_asc = false;
	auto errMsgStart = "[File: " + filename + "]: ";

	std::getline(plyFile, line);
	
	while (true) {
		// skip comments
		if (ygl::startswith(line, "end_header"))
			break;
		if (ygl::startswith(line, "format") && ygl::contains(line, "ascii")) {
			is_asc = true;
			std::getline(plyFile, line);
			continue;
		}
		if (ygl::startswith(line, "element")) {
			auto tokens = split(line, " \r\n");
			// get name
			if (tokens[1] == "vertex") {
				elements.push_back("vertex");
				n_vertices = atoi(tokens[2].c_str());
				// read properties
				std::getline(plyFile, line);
				while (ygl::startswith(line, "property")) {
					auto prop_tokens = split(line, " \r\n");
					if (prop_tokens[1] != "float") {
						std::cerr << errMsgStart << "unexpected type (!= float) for vertex property.\n";
						return false;
					}
					// memorize name of property
					vertex_prop.push_back(prop_tokens[2]);
					std::getline(plyFile, line);
				}
			}
			else if (tokens[1] == "face") {
				elements.push_back("face");
				n_faces = atoi(tokens[2].c_str());
				// read properties
				std::getline(plyFile, line);
				while (ygl::startswith(line, "property")) {
					auto prop_tokens = split(line, " \r\n");
					if (prop_tokens[2] != "uint8" && prop_tokens[2] != "uchar") {
						std::cerr << errMsgStart << "expected type uint8 or uchar for list of vertex indexes' size, but got " << prop_tokens[2] << ".\n";
						return false;
					}
					if (prop_tokens[3] != "int") {
						std::cerr << errMsgStart << "Expected type int for vertex indexes\n";
						return false;
					}
					if (prop_tokens[4] != "vertex_indices") {
						std::cerr << errMsgStart << "Expected vertex_indices property, got " << prop_tokens[4] << " instead.\n";
						return false;
					}
					std::getline(plyFile, line);
				}
			}
			else {
				std::cerr << errMsgStart << "Element " << tokens[1] << " not known.\n";
				return false;
			}
		}
		else {
			std::getline(plyFile, line);
		}
	}
	// After the header, now parse the values
	for (auto elem : elements) {
		if (elem == "vertex") {
			for (int v = 0; v < n_vertices; v++) {
				// boolean values that tells whether those properties are 
				// found in the list of properties for a vertex
				bool bpos = false;
				bool buv = false;
				bool bnorm = false;

				ygl::vec3f pos, norm;
				ygl::vec2f uv;

				if (is_asc) {
					std::getline(plyFile, line);
					auto vals = split(line, " \r\n");
					int count = 0;
					for (auto prop : vertex_prop) {
						if (prop == "x") {
							bpos = true;
							pos.x = atof(vals[count++].c_str());
						}
						else if (prop == "y") {
							pos.y = atof(vals[count++].c_str());
						}
						else if (prop == "z") {
							pos.z = atof(vals[count++].c_str());
						}
						else if (prop == "nx") {
							norm.x = atof(vals[count++].c_str());
						}
						else if (prop == "ny") {
							norm.y = atof(vals[count++].c_str());
						}
						else if (prop == "nz") {
							norm.z = atof(vals[count++].c_str());
						}
						else if (prop == "u") {
							uv.x = atof(vals[count++].c_str());
						}
						else if (prop == "v") {
							uv.y = atof(vals[count++].c_str());
						}
						else {
							std::cerr << "Value " << prop << " is not a recognized property of vertex.\n";
							return false;
						}
					}
				}
				else {
					// read binary
					for (auto prop : vertex_prop) {
						if (prop == "x") {
							char buff[4];
							bpos = true;
							plyFile.read(buff, 4);
							pos.x = *((float *)buff);
						}
						else if (prop == "y") {
							char buff[4];
							plyFile.read(buff, 4);
							pos.y = *((float *)buff);
						}
						else if (prop == "z") {
							char buff[4];
							plyFile.read(buff, 4);
							pos.z = *((float *)buff);
						}
						else if (prop == "nx") {
							char buff[4];
							bnorm = true;
							plyFile.read(buff, 4);
							norm.x = *((float *)buff);
						}
						else if (prop == "ny") {
							char buff[4];
							plyFile.read(buff, 4);
							norm.y = *((float *)buff);
						}
						else if (prop == "nz") {
							char buff[4];
							plyFile.read(buff, 4);
							norm.z = *((float *)buff);
						}
						else if (prop == "u") {
							char buff[4];
							buv = true;
							plyFile.read(buff, 4);
							uv.x = *((float *)buff);
						}
						else if (prop == "v") {
							char buff[4];
							plyFile.read(buff, 4);
							uv.y = *((float *)buff);
						}
						else {
							std::cerr << "Value " << prop << " is not a recognized property of vertex.\n";
							return false;
						}
					}
				}
				if (!bpos) {
					std::cerr << errMsgStart << "No vertex positions\n";
					return false;
				}
				shp->pos.push_back(pos);
				if (bnorm)
					shp->norm.push_back(norm);
				if (buv)
					shp->texcoord.push_back(uv);
			}
		}
		else if (elem == "face") {
			for (int f = 0; f < n_faces; f++) {
				if (is_asc) {
					std::getline(plyFile, line);
					auto vals = split(line, " \r\n");
					if (vals[0] != "3") {
						// only triangles for now
						std::cerr << errMsgStart << "There must be only three vertices per face. Got " << vals[0] << " instead.\n";
						return false;
					}
					ygl::vec3i triangle = { atoi(vals[1].c_str()), atoi(vals[2].c_str()), atoi(vals[3].c_str()) };
					shp->triangles.push_back(triangle);
				}
				else {
					ygl::vec3i triangle;

					char buff1[1];
					plyFile.read(buff1, 1);
					unsigned char n_v = (unsigned char)buff1[0];
					if (n_v != 3) {
						std::cerr << errMsgStart << "There must be only three vertices per face. Got " << n_v << " instead.\n";
						return false;
					}
					char buff[4];
					plyFile.read(buff, 4);
					triangle.x = *((int *)buff);

					plyFile.read(buff, 4);
					triangle.y = *((int *)buff);

					plyFile.read(buff, 4);
					triangle.z = *((int *)buff);

					shp->triangles.push_back(triangle);
				}
			}
		}
		else {
			std::cerr << errMsgStart << "Element '" << elem << "' not recognized.\n";
			return false;
		}

	}
	(*shape) = shp;
	plyFile.close();
	return true;
}
