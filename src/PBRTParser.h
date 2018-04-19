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
#include "spectrum.h"

// A general directive parsed parameter has type, name and value.
class PBRTParameter {
    public:
	bool used = false;
	std::string type;
    std::string name;
	// value is actually a pointer to an array of values. Different
	// parameters might have different types (hence void).
	void *value = nullptr;

	//
	// get_first_value
	// get the first value of the parsed array of values
	//
	template <typename T>
	T get_first_value() {
		std::vector<T> *vals = (std::vector<T> *)value;
		T v = vals->at(0);
		return v;
	};

	~PBRTParameter(){
		if(value == nullptr)
			return;
		if (type == "string" || type == "texture" || type == "bool") {
			delete ((std::vector<std::string> *) value);
		}
		else if (type == "float") {
			delete ((std::vector<float> *) value);
		}
		else if (type == "integer") {
			delete ((std::vector<int> *) value);
		}
		else if (type == "point3" || type == "normal3"|| type == "rgb") {
			delete ((std::vector<ygl::vec3f> *) value);
		}
		value = nullptr;	
	};
};

// DeclaredTexture, DeclaredMaterial, DeclaredObject are structures
// used to handle, as their name says, resources declared in the source pbrt file
// and that might be used (e.g. instantiated) during the next step of parsing.
// Among other things, we introduce these support structures to avoid saving resources
// that are not used (even if declared).

struct DeclaredTexture {
	ygl::texture *txt = nullptr;
	float uscale = 1;
	float vscale = 1;
	bool addedInScene = false;

	DeclaredTexture() {};
	DeclaredTexture(ygl::texture *t, float uscale, float vscale) : txt(t), uscale(uscale), vscale(vscale) {};
	
	~DeclaredTexture() {
		if (!addedInScene && txt)
			delete txt;
	};
};

struct DeclaredMaterial {
	ygl::material *mat = nullptr;
	bool addedInScene = false;
	
	DeclaredMaterial() {};
	DeclaredMaterial(ygl::material *m): mat(m) {};
	
	~DeclaredMaterial() {
		if (!addedInScene && mat)
			delete mat;
	};
};

struct DeclaredObject {
	bool addedInScene = false;
	std::vector<ygl::shape_group *> sg;
	ygl::mat4f CTM;

	DeclaredObject(std::vector<ygl::shape_group *> &s, ygl::mat4f ctm) : sg(s), CTM(ctm) {};

	~DeclaredObject() {
		if (!addedInScene)
			for (auto s : sg)
				delete s;
	};
};


struct GraphicsState {
	// Current Transformation Matrix
	ygl::mat4f CTM;
	// AreaLight directive state
	struct {
		// All the shapes declared while AreaLightMode is active become lights
		bool active = false;
		ygl::vec3f L = { 1, 1, 1 };
		bool twosided = false;
	} areaLight;

	// Current Material
	std::shared_ptr<DeclaredMaterial> mat = nullptr;
	// hack to apply uv scaling factor to shapes texture coordinates
	float uscale = 1;
	float vscale = 1;
	// mappings to memorize data by name and use them in different times during parsing
	// the following two belongs to the graphics 
	std::unordered_map<std::string, std::shared_ptr<DeclaredTexture>> nameToTexture{};
	std::unordered_map<std::string, std::shared_ptr<DeclaredMaterial>> nameToMaterial{};
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
	std::vector<std::shared_ptr<PBRTLexer>> lexers{};
	
	// stack of CTMs
	std::vector<ygl::mat4f> CTMStack{};

	// Stack of Graphic States
	std::vector<GraphicsState> stateStack{};

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
	std::vector<ygl::shape_group *> shapesInObject {};

	// Defines the current graphics properties active and to apply to the scene objects.
	GraphicsState gState{ ygl::identity_mat4f, {}, nullptr};
	// name to pair (list_of_shapes, CTM)
	std::unordered_map < std::string, std::shared_ptr<DeclaredObject>> nameToObject{}; // instancing

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

	// Resource folders
	std::string textureSavePath = ".";

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
	void parse_trianglemesh(ygl::shape *shp);
	// DEBUG method
	void parse_cube(ygl::shape *shp);

	void execute_ObjectBlock();
	void execute_ObjectInstance();

	void execute_LightSource();
	void parse_InfiniteLight();
	void parse_PointLight();
	void execute_AreaLightSource();
	void execute_Material(bool makeNamedMaterial);
	void execute_NamedMaterial();

	void parse_material_matte(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_uber(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_plastic(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_metal(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_translucent(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_mirror(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_mix(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_glass(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	void parse_material_substrate(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params);
	
	void load_texture(ygl::texture *txt, std::string &filename, bool flip = true);
	ygl::texture* blend_textures(ygl::texture *txt1, ygl::texture *txt2, float amount);
	void parse_imagemap_texture(std::shared_ptr<DeclaredTexture> &dt);
	void parse_constant_texture(std::shared_ptr<DeclaredTexture> &dt);
	void parse_scale_texture(std::shared_ptr<DeclaredTexture> &dt);
	void parse_checkerboard_texture(std::shared_ptr<DeclaredTexture> &dt);
	void execute_Texture();

	void execute_preworld_directives();
	void execute_world_directives();
	void execute_world_directive();

	// Error and format compatibility handling methods.
	void ignore_current_directive();

	inline void throw_syntax_exception(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (" << this->current_file() << ":" << this->lexers.at(0)->get_line() <<\
			"," << this->lexers.at(0)->get_column() << "): " << msg;
		//delete scn;
        throw  PBRTException(ss.str());
    };

	inline void warning_message(std::string msg) {
		std::stringstream ss;
		ss << "WARNING: ("  << this->current_file() << ":" << this->lexers.at(0)->get_line() << \
			"," << this->lexers.at(0)->get_column() << "): " << msg;
		std::cout << ss.str() << "\n";
	};

	// get an id for shape, instance, ..
	std::string get_unique_id(CounterID id);

	void fill_parameter_to_type_mapping();

	// check if the parameter par has been given a legal type
	bool check_param_type(std::string par, std::string parsedType);
	
	// some types are synonyms, transform them to default.
	std::string check_synonyms(std::string s);

	// parse a single parameter type, name and associated value
	std::shared_ptr<PBRTParameter>  parse_parameter();
	
	// parse all the parameters of the current directive
	void PBRTParser::parse_parameters(std::vector<std::shared_ptr<PBRTParameter>> &pars);

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
				throw_syntax_exception("Expected closing ']'.");
		}
		if (vals->size() == 0)
			throw_syntax_exception("The array parsed is empty.");
	};

	//
	// set_k_property
	// Convenience function to set kd, ks, kt, kr from parsed parameter
	//
	void set_k_property(std::shared_ptr<PBRTParameter> &par, ygl::vec3f &k, ygl::texture **txt);

	//
	// texture lookup.
	//
	std::shared_ptr<DeclaredTexture> texture_lookup(std::string &name, bool markAsAddedInScene) {
		auto it = gState.nameToTexture.find(name);
		if (it == gState.nameToTexture.end())
			throw_syntax_exception("Texture '" + name + "' was not found among declared textures.");
		if (markAsAddedInScene && it->second->addedInScene == false) {
			scn->textures.push_back(it->second->txt);
			it->second->addedInScene = true;
		}
		return it->second;
	}

	//
	// material lookup.
	//
	std::shared_ptr<DeclaredMaterial> material_lookup(std::string &name, bool markAsAddedInScene) {
		auto it = gState.nameToMaterial.find(name);
		if (it == gState.nameToMaterial.end())
			throw_syntax_exception("Named material '" + name + "' was not found among declared named materials.");
		if (markAsAddedInScene && it->second->addedInScene == false) {
			scn->materials.push_back(it->second->mat);
			it->second->addedInScene = true;
		}
		return it->second;
	}

	public:
	// Build a parser for the scene pointed by "filename"
	PBRTParser(std::string filename);
	// start the parsing.
    ygl::scene *parse();

};

// ==========================================================================================
//                                    AUXILIARY FUNCTIONS
// ==========================================================================================

//
// find_param
// search for a parameter by name in a vector of parsed parameters.
// Returns the index of the searched parameter in the vector if found, -1 otherwise.
//
int find_param(std::string name, std::vector<std::shared_ptr<PBRTParameter>> &vec);

//
// make_constant_image
//
ygl::image4b make_constant_image(float v);
ygl::image4b make_constant_image(ygl::vec3f v);

//
// flip_image
// flip an image on the y axis.
//
template <typename T>
ygl::image<T> flip_image(ygl::image<T> in) {
	ygl::image<T> nI(in.width(), in.height());

	for (int j = 0; j < in.height(); j++) {
		for (int i = 0; i < in.width(); i++) {
			nI.at(i, j) = in.at(i, in.height() - j - 1);
		}
	}
	return nI;
}

//
// my_compute_normals
// because pbrt computes it differently
// TODO: must be removed in future.
//
void my_compute_normals(const std::vector<ygl::vec3i>& triangles,
	const std::vector<ygl::vec3f>& pos, std::vector<ygl::vec3f>& norm, bool weighted);
#endif
