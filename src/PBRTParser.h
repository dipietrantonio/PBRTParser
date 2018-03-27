/*
 * PBRTParser
 * 
 * Cristian Di Pietrantonio (cristiandipietrantonio@gmail.com)
 * 
 * This library implements a parser for the PBRT scene format (v3).
 * The implementation follows the specifications that can be found at 
 * the following web page:
 * 
 * http://www.pbrt.org/fileformat-v3.html
 * 
 */
#ifndef __PBRTPARSER__
#define __PBRTPARSER__
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <unordered_map>
#define YGL_IMAGEIO 1
#define YGL_OPENGL 0
#include "../yocto/yocto_gl.h"
#include "PBRTLexer.h"
#include "PLYParser.h"
#include "utils.h"

// A general directive parsed parameter has type, name and value
struct PBRTParameter{
    std::string type;
    std::string name;
    void *value;
};

// All the shapes declared while AreaLightMode is active become lights
struct AreaLightMode {
	bool active = false;
	ygl::vec3f L = { 1, 1, 1 };
	bool twosided = false;
};

struct GraphicState {
	// Current Transformation Matrix
	ygl::mat4f CTM;
	// AreaLight directive state
	AreaLightMode ALInfo;
	// Current Material
	ygl::material *mat = nullptr;

	// mappings to memorize data by name and use them in different times during parsing
	std::unordered_map<std::string, ygl::texture*> nameToTexture{};
	std::unordered_map<std::string, ygl::material*> nameToMaterial{};
};

struct DeclaredObject {
	bool referenced = false;
	ygl::shape_group *sg;
	ygl::mat4f CTM;

	DeclaredObject(ygl::shape_group *s, ygl::mat4f ctm) : sg(s), CTM(ctm) {};
};

// This structure will simplify the code later, allowing to work at the same time with
// both HDR and LDR images.
class TextureSupport {
public:
	ygl::texture *txt;
	int height;
	int width;
	bool is_hdr;

	ygl::vec4f at(int i, int j) {
		if(is_hdr)
			return txt->hdr.at(i, j);
		else
			return ygl::byte_to_float(txt->ldr.at(i, j));
	};

	TextureSupport(ygl::texture *t) {
		txt = t;
		if (!t)
			return;
		if (t->ldr.empty()) {
			is_hdr = true;
			height = t->hdr.height();
			width = t->hdr.width();
		}
		else {
			is_hdr = false;
			height = t->ldr.height();
			width = t->ldr.width();
		}	
	};
};

class PBRTParser {

    private:

	// PRIVATE ATTRIBUTES
	// stack of lexical analyzers (useful for Inlcude directives)
	std::vector<PBRTLexer *> lexers{};
	
	// stack of CTMs
	std::vector<ygl::mat4f> CTMStack{};

	// Stack of Graphic States
	std::vector<GraphicState> stateStack{};

	// What follows are some variables that need to be shared among
	// parsing statements
	
	// pointer to the newly created yocto scene
	ygl::scene *scn = nullptr;

	// The following information must be gloabally accessible during parsing.

	// aspect ratio can be set in Camera or Film directive
	float defaultAspect = 16.0 / 9.0;
	float defaultFocus = 1;

	// "execute_Shape" call needs to know this information.
	bool inObjectDefinition = false;
	// When the parser is inside an Onject environment, it needs to keep
	// track of all the shapes that define the object. While "inObjectDefinition"
	// is true, every shape parsed will be put in this vector. Since we can parse
	// one Object at time, using a single vector is fine.
	ygl::shape_group *shapesInObject = nullptr;

	// Defines the current graphics properties active and to apply to the scene objects.
	GraphicState gState{ ygl::identity_mat4f, {}, nullptr};
	// name to pair (list_of_shapes, CTM)
	std::unordered_map < std::string, DeclaredObject> nameToObject{}; // instancing

	// the following items are used to assign unique names to elements.
	unsigned int shapeCounter = 0;
	unsigned int shapeGroupCounter = 0;
	unsigned int instanceCounter = 0;
	unsigned int materialCounter = 0;
	unsigned int textureCounter = 0;
	unsigned int envCounter = 0;
	enum CounterID {shape, shape_group, instance, material, texture, environment};

	// The following mapping specifies the legal types for each possible parameter
	std::unordered_map<std::string, std::vector<std::string>> parameterToType{};

	// PRIVATE METHODS
	
	// Read the next token
	void advance();
	// Get the current token
	Lexeme& current_token() {
		return this->lexers.at(0)->currentLexeme;
	};
	// current position of parser in the filesystem
	std::string& current_path() {
		return this->lexers.at(0)->path;
	}

	std::string current_file() {
		return this->lexers.at(0)->path + "/" + this->lexers.at(0)->filename;
	}

	// The following private methods correspond to Directives in pbrt format.
	// execute_* are a direct corrispondence to the pbrt directives. Sometimes they use and share some
	// helper functions, which have the prefix "parse_*".
	 
	void execute_Include();
	// Scene wide rendering options
	void execute_Camera();
	void execute_Film();
	// Transformation
	void execute_Translate();
	void execute_Scale();
	void execute_Rotate();
	void execute_LookAt();
	void execute_Transform();
	void execute_ConcatTransform();

	void execute_AttributeBegin();
	void execute_AttributeEnd();
	void execute_TransformBegin();
	void execute_TransformEnd();

	void execute_Shape();
	void parse_curve(ygl::shape *shp);
	void parse_trianglemesh(ygl::shape *shp);
	// DEBUG method
	void parse_cube(ygl::shape *shp);

	void execute_ObjectBlock();
	void execute_ObjectInstance();

	void execute_LightSource();
	void parse_InfiniteLight();
	void parse_PointLight();
	void execute_AreaLightSource();

	void execute_MakeNamedMaterial();
	void execute_NamedMaterial();
	void execute_Material();
	
	void parse_material_matte(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_uber(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_plastic(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_metal(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_translucent(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_mirror(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_mix(ygl::material *mat, std::vector<PBRTParameter> &params);
	void parse_material_glass(ygl::material *mat, std::vector<PBRTParameter> &params);

	// TEXTURES
	ygl::texture* blend_textures(ygl::texture *txt1, ygl::texture *txt2, float amount);
	void parse_imagemap_texture(ygl::texture *txt);
	void parse_constant_texture(ygl::texture *txt);
	void parse_checkerboard_texture(ygl::texture *txt);
	void parse_fbm_texture(ygl::texture *txt);
	void execute_Texture();

	void execute_preworld_directives();
	void execute_world_directives();
	void execute_world_directive();

	// Error and format compatibility handling methods.
	void ignore_current_directive();

	inline void throw_syntax_error(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (file: " << this->current_file() << ", line " << this->lexers.at(0)->get_line() <<\
			", column " << this->lexers.at(0)->get_column() << "): " << msg;
        throw std::exception(ss.str().c_str());
    };
	// get an id for shape, instance, ..
	std::string get_unique_id(CounterID id);
	void fill_parameter_to_type_mapping();

	// check if the parameter par has been given a legal type
	bool check_param_type(std::string par, std::string parsedType);
	
	// some types are synonyms, transform them to default.
	std::string check_synonyms(std::string s);

	// parse a single parameter type, name and associated value
	void parse_parameter(PBRTParameter &par);
	
	// parse all the parameters of the current directive
	void parse_parameters(std::vector<PBRTParameter> &pars);

	//
	// get_single_value
	// get the first value of the parsed array of values
	//
	template <typename T>
	T get_single_value(PBRTParameter &par) {
		std::vector<T> *vals = (std::vector<T> *)par.value;
		T v = vals->at(0);
		delete vals;
		return v;
	};

	//
	// find_param
	// search for a parameter by name in a vector of parsed parameters.
	// Returns the index of the searched parameter in the vector if found, -1 otherwise.
	//
	int find_param(std::string name, std::vector<PBRTParameter> &vec);

	//
	// parse_value
	// Parse an array of values (or a single value).
	//
	template <typename T, int LT, typename _CONVERTER>
	void parse_value(std::vector<T> *vals, _CONVERTER converter) {

		bool isArray = false;
		if (this->current_token().value == "[") {
			this->advance();
			isArray = true;
		}

		while (this->current_token().type == LT) {
			vals->push_back(converter(this->current_token().value));
			this->advance();
			if (!isArray)
				break;
		}
		if (isArray) {
			if (this->current_token().value == "]")
				this->advance();
			else
				throw_syntax_error("Expected closing ']'.");
		}
		if (vals->size() == 0)
			throw_syntax_error("The array parsed is empty.");
	};

	// Convenience functions

	//
	// set_k_property
	// Convenience function to set kd, ks, kt, kr from parsed parameter
	//
	void set_k_property(PBRTParameter &par, ygl::vec3f &k, ygl::texture **txt);

	//
	// make_constant_image
	//
	ygl::image4b PBRTParser::make_constant_image(float v);
	ygl::image4b PBRTParser::make_constant_image(ygl::vec3f v);

	// load_texture image from file
	void load_texture(ygl::texture *txt, std::string &filename);

    public:
    
	// Build a parser for the scene pointed by "filename"
	PBRTParser(std::string filename){
		this->lexers.push_back(new PBRTLexer(filename));
		this->scn = new ygl::scene();
		this->fill_parameter_to_type_mapping();
    };

	// start the parsing.
    ygl::scene *parse();

};
#endif