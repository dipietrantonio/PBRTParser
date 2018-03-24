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
#include <typeinfo>
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

// This structure will simplify the code later, allowing to use
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
	std::unordered_map < std::string, ShapeData> nameToObject{}; // instancing

	// the following items are used to assign unique names to elements.
	unsigned int shapeCounter = 0;
	unsigned int shapeGroupCounter = 0;
	unsigned int instanceCounter = 0;
	unsigned int materialCounter = 0;
	unsigned int textureCounter = 0;
	unsigned int envCounter = 0;
	enum CounterID {shape, shape_group, instance, material, texture, environment};

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

	// The following functions parse array of values
	void parse_value_int(std::vector<int> *vals);
	void parse_value_float(std::vector<float> *vals);
	void parse_value_string(std::vector<std::string> *vals);
	// parse a single parameter type, name and associated value
	void parse_parameter(PBRTParameter &par);

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
	void PBRTParser::parse_material_properties(ParsedMaterialInfo &pmi);
	void parse_material_matte(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_uber(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_plastic(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_metal(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_translucent(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_mirror(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_mix(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);
	void parse_material_glass(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed = false);

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

	inline std::string get_unique_id(CounterID id) {
		char buff[300];
		unsigned int val;
		char *st = "";

		if (id == CounterID::shape)
			st = "s_", val = shapeCounter++;
		else if (id == CounterID::shape_group)
			st = "sg_", val = shapeGroupCounter++;
		else if (id == CounterID::instance)
			st = "i_", instanceCounter++;
		else if (id == CounterID::material)
			st = "m_", val = materialCounter++;
		else if (id == CounterID::environment)
			st = "e_", val = envCounter++;
		else
			st = "t_", val = textureCounter++;
		
		sprintf(buff, "%s%u", st, val);
		return std::string(buff);
	}

    public:
    
	// Build a parser for the scene pointed by "filename"
	PBRTParser(std::string filename){
		this->lexers.push_back(new PBRTLexer(filename));
		this->scn = new ygl::scene();
    };

	// start the parsing.
    ygl::scene *parse();

};
#endif