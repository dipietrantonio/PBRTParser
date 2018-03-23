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
#include <cstdio>
#include <sstream>
#include <exception>
#include <locale>
#include <unordered_map>
#define YGL_IMAGEIO_IMPLEMENTATION 1
#define YGL_OPENGL 0
#include "../yocto/yocto_gl.h"
#include "PBRTLexer.h"
#include "PLYParser.h"
#include "utils.h"

// ===========================================================================================
//                                         PBRTParser
// ===========================================================================================


struct PBRTParameter{
    std::string type;
    std::string name;
    void *value;
};

struct AreaLightInfo {
	bool active = false;
	ygl::vec3f L = { 1, 1, 1 };
	bool twosided = false;
};

// not a fan of the following
struct ParsedMaterialInfo {
	bool b_type = false;
	std::string type;
	bool b_kd = false;
	ygl::vec3f kd;
	ygl::texture * textureKd = nullptr;
	bool b_ks = false;
	ygl::vec3f ks;
	ygl::texture* textureKs = nullptr;
	bool b_kr = false;
	ygl::vec3f kr;
	ygl::texture* textureKr = nullptr;
	bool b_kt = false;
	ygl::vec3f kt;
	ygl::texture* textureKt = nullptr;
	bool b_ke = false;
	ygl::vec3f ke;
	ygl::texture* textureKe = nullptr;
	bool b_eta = false;
	ygl::vec3f eta;
	ygl::texture* textureETA = nullptr;
	bool b_k = false;
	ygl::vec3f k;
	ygl::texture* textureK = nullptr;
	bool b_rs = false;
	float rs;
	ygl::texture* textureRs = nullptr;
	bool b_amount = false;
	float amount;
	bool b_namedMaterial1 = false;
	std::string namedMaterial1;
	bool b_namedMaterial2 = false;
	std::string namedMaterial2;
	bool b_bump = false;
	ygl::texture* bump = nullptr;
};

struct GraphicState {
	// Current Transformation Matrix
	ygl::mat4f CTM;
	// AreaLight directive state
	AreaLightInfo ALInfo;
	// Current Material
	ygl::material *mat = nullptr;

	// mappings to memorize data (textures, objects, ..) by name and use
	// them in different times during parsing
	std::unordered_map<std::string, ygl::texture*> nameToTexture{};
	std::unordered_map<std::string, ygl::material*> nameToMaterial{};
};

struct ShapeData {
	bool referenced = false;
	ygl::shape_group *sg;
	ygl::mat4f CTM;

	ShapeData(ygl::shape_group *s, ygl::mat4f ctm) : sg(s), CTM(ctm) {};
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
	std::unordered_map < std::string, ShapeData> nameToObject{}; // instancing

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

	// The following methods are used to parse parameter values (i.e. normals, vertices, positions)
	template<typename T, int N>
	ygl::vec<T, N> parse_vec() {
		ygl::vec<T, N> v;
		for (int i = 0; i < N; i++) {
			if (this->current_token().type != LexemeType::NUMBER) {
				throw_syntax_error("The parsed value is not a number.");
			}
			if (T == int) {
				v[i] = atoi(this->current_token().value);
			}
			else {
				v[i] = atof(this->current_token().value);
			}
			this->advance();
		}
		return v;
	}

	// parse a single parameter type, name and associated value
	bool parse_parameter(PBRTParameter &par, std::string type = "");

	template<typename T1, typename T2>
	void parse_value_array(int groupSize, std::vector<T1> *vals, bool(*checkValue)(Lexeme &lexm),
		T1(*convert)(T2*, int), T2(*convert2)(std::string)) {
		this->advance();
		// it can be a single value or multiple values
		if (checkValue(this->current_token()) && groupSize == 1) {
			// single value
			T2 v = convert2(this->current_token().value);
			vals->push_back(convert(&v, 1));
		}
		else if (this->current_token().type == LexemeType::SINGLETON && this->current_token().value == "[") {
			// start array of value(s)
			bool stopped = false;
			T2 *value = new T2[groupSize];
			while (!stopped) {
				for (int i = 0; i < groupSize; i++) {
					this->advance();
					if (this->current_token().type == LexemeType::SINGLETON && this->current_token().value == "]") {
						if (i == 0) {
							stopped = true;
							break; // finished to read the array
						}
						else {
							throw_syntax_error("Too few values specified.");
						}
					}
					if (!checkValue(this->current_token()))
						throw_syntax_error("One of the values differs from the expected type.");
					value[i] = convert2(this->current_token().value);
				}
				if (!stopped)
					vals->push_back(convert(value, groupSize));
			}
			delete[] value;
		}
		else {
			throw_syntax_error("Value differ from expected type.");
		}
		this->advance(); // remove ] or single value
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

	// Attributes
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
	void PBRTParser::parse_material_properties(ParsedMaterialInfo &pmi);
	void parse_material_matte(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_uber(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_plastic(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_metal(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_translucent(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_mirror(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_mix(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);

	void execute_Texture();

	void parse_preworld_directives();
	void parse_world_directives();

	void parse_world_directive();

	// Error and format compatibility handling methods.

	void ignore_current_directive();

	inline void throw_syntax_error(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (file: " << this->current_file() << ", line " << this->lexers.at(0)->get_line() <<\
			", column " << this->lexers.at(0)->get_column() << "): " << msg;
        throw std::exception(ss.str().c_str());
    };

	inline void check_type() {

	}
    public:
    
	// public methods

	// Build a parser for the scene pointed by "filename"
	PBRTParser(std::string filename){
		this->lexers.push_back(new PBRTLexer(filename));
		this->scn = new ygl::scene();
    };

	// start the parsing.
    ygl::scene *parse();

};
#endif