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

#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <exception>
#include <locale>
#include "yocto_gl.h"
#include <unordered_map>

#define YGL_IMAGEIO 1

class InputEndedException : public std::exception {

    virtual const char *what() const throw(){
        return "Input has ended.";
    }
};

class LexicalErrorException : public std::exception{

    private:
    std::string message;

    public:
    LexicalErrorException(std::string msg){
        this->message = msg;
    }
	virtual char const * what() {
		return message.c_str();
	}

};

class SyntaxErrorException : public std::exception{

    private:
    std::string message;

    public:
    SyntaxErrorException(std::string msg){
        this->message = msg;
    }

	virtual char const * what() {
		return message.c_str();
	}
	
};

enum LexemeType {INDENTIFIER, NUMBER, STRING, SINGLETON};

class Lexeme {
    private:
    std::string value;
    LexemeType type;

    public:
    Lexeme() {};
    Lexeme(LexemeType type, std::string value){
        this->type = type;
        this->value = value;
    }
    std::string get_value(){
        return this->value;
    }

    LexemeType get_type(){
        return this->type;
    }
};

class PBRTLexer{

    private:
    int line, column;
    int currentPos;
    std::string text;
    bool inputEnded;
    int lastPos;
    // Private methods

    void remove_blanks();
    void advance();

    inline char look(int i = 0){
        return this->text[currentPos + i];
    }

    bool read_indentifier();
    bool read_number();
    bool read_string();

    public:
	Lexeme currentLexeme;
    PBRTLexer(std::string text);
    bool next_lexeme();
    int get_column(){ return this->column;};
    int get_line(){ return this->line; };
};

PBRTLexer::PBRTLexer(std::string text){
    this->line = 1;
    this->column = 1;
    this->text = text;
    this->lastPos = text.length() - 1;
    this->currentPos = 0;

    if(text.length() == 0)
        this->inputEnded = true;
    else
        this->inputEnded = false;
}

//
// parse
//
bool PBRTLexer::next_lexeme(){
	this->remove_blanks();

	if (this->read_indentifier())
		return true;
	if (this->read_string())
		return true;
	if (this->read_number())
		return true;

	char c = this->look();
	if (c == '[' || c == ']') {
		std::stringstream ss = std::stringstream();
		ss.put(c);
		this->currentLexeme = Lexeme(LexemeType::SINGLETON, ss.str());
		this->advance();
		return true;
	}

	std::stringstream errStr;
	errStr << "Lexical error (line " << this->line << ", column " << this->column << ") input not recognized.";
	throw LexicalErrorException(errStr.str());
}

bool PBRTLexer::read_indentifier(){
    char c = this->look();
    
    std::stringstream ss;
    if(!std::isalpha(c))
        return false;
    do{
        ss << c;
        this->advance();
    }while(std::isalpha(c = this->look()));

   this->currentLexeme = Lexeme(LexemeType::INDENTIFIER, ss.str());
   return true;
}

bool PBRTLexer::read_string(){
    char c = this->look();
    std::stringstream ss;
    // remove "
    if(!(c == '"'))
        return false;
    this->advance();
    while((c = this->look()) != '"'){
        ss << c;
        this->advance();
    }
    this->advance();
   this->currentLexeme = Lexeme(LexemeType::STRING, ss.str());
   return true;
}

bool PBRTLexer::read_number(){
    char c = this->look();
    if( ! (c == '+' || c == '-' || c == '.' || std::isdigit(c)))
        return false;
    std::stringstream ss;
    bool point_seen = false;
    /*
     * status = 0: a digit, `+`, `-` or `.` expected
     * status = 1: waiting for one mandatory digit
     * status = 2: waiting for more digits or `.` (if not seen before) [final state]
     * status = 3: waiting for zero or ore digits after point [final state]
     * status = 4: waiting for `-`, `+` or a digit after "E" (exponential)
     * status = 5: waiting for mandatory digit
     * status = 6: waiting for zero or more additional digits [final state]
     */
    int status = 0;

    while(true){
        if(status == 0 && (c == '-' || c == '+' || c == '.')){
            if (c == '.')
                point_seen = true;
            status = 1; // at least one digit needed
        }else if(status == 0 && std::isdigit(c)){
            status = 2;
        }else if(status == 1 && std::isdigit(c)){
            if(point_seen)
                status = 3;
            else
                status = 2;
        }else if(status == 2 && c == '.' && !point_seen){
            point_seen = true;
            status = 3;
        }else if(status == 2 && std::isdigit(c)){
            status = 2;
        }else if(status == 2 && (std::tolower(c) == 'e')){
            status = 4;
        }else if(status == 3 && std::isdigit(c)){
            status = 3;
        }else if(status == 3 && (std::tolower(c) == 'e')){
            status = 4;
        }else if(status == 4 && (c == '-' || c == '+')){
            status = 5;
        }else if(status == 4 && std::isdigit(c)){
            status = 6;
        }else if(status == 5 && std::isdigit(c)){
            status = 6;
        }else if(status == 6 && std::isdigit(c)){
            status = 6;
        }else if(status == 2 || status == 3 || status == 6){
            // legal point of exit
            break;
        }else{
            std::stringstream errstr;
            errstr << "Lexical error (line: " << this->line << ", column " << this->column << "): wrong litteral specification.";
            throw LexicalErrorException(errstr.str());
        }
        ss << c;
        this->advance();
        c = this->look();
    }
    this->currentLexeme = Lexeme(LexemeType::NUMBER, ss.str());
    return true;
}

//
// advance
// Moves the lexer to the next character in the string.
//
void PBRTLexer::advance(){
	if (this->currentPos < this->lastPos) {
		this->currentPos++;
		this->column++;
	}
    else if(this->inputEnded)
        throw InputEndedException();
    else{
        // a trick to avoid to lose the last lexeme
        this->inputEnded = true;
        this->currentPos = 0;
        this->text = "  ";
        this->lastPos = 1;
    }
}

//
// Remove blanks
// Removes all the characters that can be ignored in the input from the current
// position till the next meaningful character. That is, remove all the characters
// that are not part of the alphabet.
//
void PBRTLexer::remove_blanks(){ 
    while(true){
        char tmp = this->look();
        if(tmp == ' ' || tmp == '\t' || tmp == '\r' || tmp == '\n'){
			if (tmp == '\n') {
				this->line++;
				this->column = 0;
			}
            this->advance();
            continue;
        }else if(tmp == '#'){
            // comment
            while(tmp != '\n'){
                this->advance();
                tmp = this->look();
            }
			this->column = 0;
			this->advance();
            this->line++;
            continue;
        }else{
            // meaningful part of the input starts, exit the routine.
            return;
        }
    }
}

/* ===========================================================================================
 *                                          PBRTParser
 * Parses a stream of lexemes.
 * ===========================================================================================
 */

// -------------------------------------------------------------------------------------------
// UTILS
// -------------------------------------------------------------------------------------------

//
// split
// split a string according to space character.
//
std::vector<std::string> split(std::string text) {
	std::vector<std::string> results;
	std::stringstream ss;

	for (char c : text) {
		if (c == ' ') {
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

struct GraphicState {
	// Current Transformation Matrix
	ygl::mat4f CTM;
	// AreaLight directive status
	AreaLightInfo alInfo;
	// Current Material
	ygl::material mat;

};

class PBRTParser {

    private:

	// PRIVATE ATTRIBUTES
    PBRTLexer *lexer;
	// end of the token stream reached?
	bool inputEnded = false;

	// What follows are some variables that need to be shared among
	// parsing statements
	
	// pointer to the newly created yocto scene
	ygl::scene *scn;
	// aspect ratio can be set in Camera or Film directive
	float defaultAspect = 0;
	GraphicState gState{ ygl::identity_mat4f, {}, {} };

	// mappings to memorize data (textures, objects, ..) by name and use
	// them in different times duding parsing (eg for instancing).
	std::unordered_map<std::string, ygl::texture*> nameToTexture{};
	std::unordered_map<std::string, ygl::material*> nameToMaterial{};
	std::unordered_map<std::string, ygl::shape*> nameToObject{}; // instancing

	// PRIVATE METHODS
	void advance();
	Lexeme& current_token() {return this->lexer->currentLexeme;};
	bool parse_preworld_statements();
    bool parse_world_statements();
	bool parse_parameter(PBRTParameter &par, std::string type = "");

	template<typename T1, typename T2>
	void parse_value_array(int groupSize, std::vector<T1> *vals, bool(*checkValue)(Lexeme &lexm),\
		T1(*convert)(T2*, int), T2(*convert2)(std::string));


	// The following private methods correspond to Directives in pbrt format.
	 
	// Scene wide rendering options
	void execute_Camera();
	void execute_Film();
	// Transformation
	void execute_Translate();
	void execute_Scale();
	void execute_Rotate();
	void execute_LookAt();
	// Attributes
	void execute_AttributeBlock();
	void parse_block_content();
	void execute_TransformBlock();

	void execute_Shape();
	void parse_curve();
	void parse_trianglemesh();

	void load_texture_from_file(std::string filename, ygl::texture_info &txtinfo);

	void execute_Object();
	void execute_ObjectInstance();

	void execute_LightSource();
	void parse_InfiniteLight();
	void parse_PointLight();

	void execute_AreaLightSource();

	void execute_MakeNamedMaterial();
	void execute_NamedMaterial();
	void execute_Material();
	void parse_material_matte();
	void parse_material_plastic();
	void parse_material_metal();
	void parse_material_mirror();
	void parse_material_mix();

	void execute_Texture();

	inline void throw_syntax_error(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (line " << this->lexer->get_line() << ", column " << this->lexer->get_column() << "): " << msg;
        throw SyntaxErrorException(ss.str());
    };

    public:
    
	// PUBLIC METHODS

	PBRTParser(std::string text){
        this->lexer = new PBRTLexer(text);
		this->scn = new ygl::scene();
    };

    ygl::scene *parse();

};

//
// advance
// Fetches the next lexeme (token)
//
void PBRTParser::advance() {
	try {
		this->lexer->next_lexeme();
	}
	catch (InputEndedException ex) {
		inputEnded = true;
	}
};

//
// parse
//
ygl::scene *PBRTParser::parse() {
	ygl::mat4f CTM = ygl::identity_mat4f;
	this->advance();

	while (!inputEnded) {
		try{
			PBRTParameter par;
			this->parse_parameter(par);
			if (!par.type.compare("string")) {
				std::cout << "String parameter of name " << par.name << "\nValues: ";
				auto vals = (std::vector<std::string> *)par.value;
				for (std::string s : *vals) {
					std::cout << s << ", ";
				}
				std::cout << "\n";
			}
			else if (!par.type.compare("normal") || !par.type.compare("rgb")) {
				std::cout << "Normal parameter of name " << par.name << "\nValues: ";
				auto vals = (std::vector<ygl::vec3f> *)par.value;
				for (ygl::vec3f s : *vals) {
					std::cout << s << ", ";
				}
				std::cout << "\n";
			}
		}
		catch (SyntaxErrorException ex) {
			std::cout << ex.what() << std::endl;
			return nullptr;
		}
		catch (LexicalErrorException ex) {
			std::cout << "ERRR2\n";
			std::cout << ex.what() << std::endl;
			return nullptr;
		}
	}
}


bool PBRTParser::parse_preworld_statements() {
    // When this method starts executing, the first token must be an Identifier of
	// a directive.
	bool checkCamera = false;

	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().get_type() == LexemeType::INDENTIFIER &&
		!this->current_token().get_value().compare("WorldBegin"))) {

		if (this->current_token().get_type() != LexemeType::INDENTIFIER)
			throw_syntax_error("Identifier expected.");

		// Scene-Wide rendering options
		else if (!this->current_token().get_value().compare("Camera")) {
			this->execute_Camera();
			checkCamera = true;
		}
		else if (!this->current_token().get_value().compare("Film")) {
			this->execute_Film();
		}

		// Transformation
		else if (!this->current_token().get_value().compare("Translate")) {
			this->execute_Translate();
		}
		else if (!this->current_token().get_value().compare("Scale")) {
			this->execute_Scale();
		}
		else if (!this->current_token().get_value().compare("Rotate")) {
			this->execute_Rotate();
		}
		else if (!this->current_token().get_value().compare("LookAt")) {
			this->execute_LookAt();
		}
	}
	return checkCamera;
}

bool PBRTParser::parse_world_statements() {
	// TODO
	this->gState.CTM = ygl::identity_mat4f;
	return false;
}

bool check_type_existence(std::string &val){
	std::vector<std::string> varTypes{ "integer", "float", "point2", "vector2", "point3", "vector3",\
		"normal3", "spectrum", "bool", "string", "rgb", "color", "point", "vector", "normal" };

	for (auto s : varTypes)
		if (!s.compare(val))
			return true;
    return false;
}

template<typename T1, typename T2>
void PBRTParser::parse_value_array(int groupSize, std::vector<T1> *vals, bool(*checkValue)(Lexeme &lexm), 
	T1(*convert)(T2*, int), T2(*convert2)(std::string)) {
	this->advance();
	// it can be a single value or multiple values
	if (checkValue(this->current_token()) && groupSize == 1) {
		// single value
		T2 v = convert2(this->current_token().get_value());
		vals->push_back(convert(&v, 1));
	}
	else if (this->current_token().get_type() == LexemeType::SINGLETON && !this->current_token().get_value().compare("[")) {
		// start array of value(s)
		bool stopped = false;
		T2 *value = new T2[groupSize];
		while (!stopped) {
			for (int i = 0; i < groupSize; i++) {
				this->advance();
				if (this->current_token().get_type() == LexemeType::SINGLETON && !this->current_token().get_value().compare("]")) {
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
				value[i] = convert2(this->current_token().get_value());
			}
			if(!stopped)
				vals->push_back(convert(value, groupSize));
		}
		delete[] value;
	}
	else {
		throw_syntax_error("Value differ from expected type.");
	}
	this->advance(); // remove ] or single value
}

bool PBRTParser::parse_parameter(PBRTParameter &par, std::string type){
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected a string with type and name of a parameter.");
    auto tokens = split(this->current_token().get_value());
    // handle type
    if(!check_type_existence(tokens[0]))
        throw_syntax_error("Unrecognized type.");
    if(!type.compare("") && !(type.compare(tokens[0]))){
        std::stringstream ss;
        ss << "Expected parameter of type `" << type << "` but got `" << tokens[0] << "`.";
        throw_syntax_error(ss.str());
    }
    par.type = tokens[0];
	par.name = tokens[1];

	// now, according to type, we parse the value
	if (!par.type.compare("string")) {
		std::vector<std::string> *vals = new std::vector<std::string>();
		parse_value_array<std::string, std::string>( 1, vals, \
			[](Lexeme &lex)->bool {return lex.get_type() == LexemeType::STRING; },\
			[](std::string *x, int g)->std::string {return *x; },
			[](std::string x) -> std::string {return x; }
		);
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	else if (!par.type.compare("float")) {
		std::vector<float> *vals = new std::vector<float>();
		parse_value_array<float, float>( 1, vals, \
			[](Lexeme &lex)->bool {return lex.get_type() == LexemeType::NUMBER; },\
			[](float *x, int g)->float {return *x; },
			[](std::string x) -> float {return atof(x.c_str()); }
		);
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	else if (!par.type.compare("integer")) {
		std::vector<int> *vals = new std::vector<int>();
		parse_value_array<int, int>( 1, vals, \
			[](Lexeme &lex)->bool {return lex.get_type() == LexemeType::NUMBER; }, \
			[](int *x, int g)->int {return *x; },
			[](std::string x) -> int {return atoi(x.c_str()); }
		);
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	else if (!par.type.compare("bool")) {
		std::vector<bool> *vals = new std::vector<bool>();
		parse_value_array<bool, bool>( 1, vals, \
			[](Lexeme &lex)->bool {return lex.get_type() == LexemeType::STRING && \
				(!lex.get_value().compare("true") || !lex.get_value().compare("false")); }, \
			[](bool *x, int g)->bool {return *x; },
			[](std::string x) -> bool { return (!x.compare("true")) ? true : false; }
		);
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	// now we come at a special case of arrays
	else if (!par.type.compare("point") || !par.type.compare("point3") || !par.type.compare("normal") 
		|| !par.type.compare("normal3") || !par.type.compare("rgb") || !par.type.compare("color")) {
		std::vector<ygl::vec3f> *vals = new std::vector<ygl::vec3f>();
		parse_value_array<ygl::vec3f, float>( 3, vals, \
			[](Lexeme &lex)->bool {return lex.get_type() == LexemeType::NUMBER; }, \
			[](float *x, int g)->ygl::vec3f {
				ygl::vec3f r; 
				for (int k = 0; k < g; k++) 
					r[k] = x[k]; 
				return r; },
			[](std::string x) -> float { return atof(x.c_str()); }
		);
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	return true;
}

bool is_camera_type(std::string type) {
	std::vector<std::string> camTypes { "environment", "ortographic", "perspective", "realistic" };
	for (auto s : camTypes)
		if (!s.compare(type))
			return true;
	return false;
}

//
// TRANSFORMATIONS
//

void PBRTParser::execute_Translate() {
	float x, y, z;

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Translate directive.");
	x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Translate directive.");
	y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Translate directive.");
	z = atof(this->current_token().get_value().c_str());
	
	const ygl::vec3f transl_vec { x, y, z };
	auto transl_mat = ygl::translation_mat4f(transl_vec);
	this->gState.CTM = this->gState.CTM * transl_mat;
	this->advance();
}

void PBRTParser::execute_Scale(){
	float x, y, z;

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Scale directive.");
	x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Scale directive.");
	y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Scale directive.");
	z = atof(this->current_token().get_value().c_str());

	const ygl::vec3f scale_vec{ x, y, z };
	auto scale_mat = ygl::scaling_mat4f(scale_vec);
	this->gState.CTM = this->gState.CTM * scale_mat;
	this->advance();
}
void PBRTParser::execute_Rotate() {
	float angle, x, y, z;

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'angle' parameter of Rotate directive.");
	angle = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Rotate directive.");
	x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Rotate directive.");
	y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Rotate directive.");
	z = atof(this->current_token().get_value().c_str());

	const ygl::vec3f rot_vec{ x, y, z };
	auto rot_mat = ygl::rotation_mat4f(rot_vec, angle);
	this->gState.CTM = this->gState.CTM * rot_mat;
	this->advance();
}

void PBRTParser::execute_LookAt(){
	float eye_x, eye_y, eye_z, look_x, look_y, look_z, up_x, up_y, up_z;
	
	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_x' parameter of LookAt directive.");
	eye_x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_y' parameter of LookAt directive.");
	eye_y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_z' parameter of LookAt directive.");
	eye_z = atof(this->current_token().get_value().c_str());

	const ygl::vec3f eye{ eye_x, eye_y, eye_z };

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_x' parameter of LookAt directive.");
	look_x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_y' parameter of LookAt directive.");
	look_y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_z' parameter of LookAt directive.");
	look_z = atof(this->current_token().get_value().c_str());

	const ygl::vec3f look { look_x, look_y, look_z };

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_x' parameter of LookAt directive.");
	up_x = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_y' parameter of LookAt directive.");
	up_y = atof(this->current_token().get_value().c_str());

	this->advance();
	if (this->current_token().get_type() != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_z' parameter of LookAt directive.");
	up_z = atof(this->current_token().get_value().c_str());

	const ygl::vec3f up{ up_x, up_y, up_z };

	auto mm = ygl::lookat_mat4f(eye, look, up);
	this->gState.CTM = this->gState.CTM * mm;
	this->advance();
}

//
// SCENE-WIDE RENDERING OPTIONS
//


//
// execute_Camera
// Parse camera information.
// NOTE: only support perspective camera for now. And only one camera
//
void PBRTParser::execute_Camera() {
	ygl::camera *cam = new ygl::camera;
	cam->aspect = defaultAspect;
	cam->aperture = 0.0f;
	cam->yfov = 90.0f;
	cam->focus = powf(10, 30);

	// TODO: check if this is right
	// CTM defines world to camera transformation
	cam->frame = ygl::to_frame3f(this->gState.CTM);

	// Parse the camera parameters

	// First parameter is the type
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string camType = this->current_token().get_value();
	this->advance();

	// RESTRICTION: only perspective camera is supported
	if (camType.compare("perspective"))
		throw_syntax_error("Only perspective camera type supported.");
	// END-RESTRICTION

	// read parameters
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		// get aspect ratio
		if (!par.name.compare("frameaspectratio")) {
			if (par.type.compare("float"))
				throw_syntax_error("'floataspectratio' must have type float.");
			if (camType.compare("perspective") && camType.compare("orthographic") && camType.compare("environment"))
				throw_syntax_error("'floataspectratio' is not a parameter for the current camera type.");
			auto data = (std::vector<float> *) par.value;
			cam->aspect = data->at(0);
			delete data;
		}
		else if (!par.name.compare("lensradius")) {
			if (par.type.compare("float"))
				throw_syntax_error("'lensradius' must have type float.");
			if (camType.compare("perspective") && camType.compare("orthographic"))
				throw_syntax_error("'lensradius' is not a parameter for the current camera type.");
			auto data = (std::vector<float> *) par.value;
			cam->aperture = data->at(0);
			delete data;
		}
		else if (!par.name.compare("focaldistance")) {
			if (par.type.compare("float"))
				throw_syntax_error("'focaldistance' must have type float.");
			if (camType.compare("perspective") && camType.compare("orthographic"))
				throw_syntax_error("'focaldistance' is not a parameter for the current camera type.");
			auto data = (std::vector<float> *) par.value;
			cam->focus = data->at(0);
			delete data;
		}
		else if (!par.name.compare("fov")) {
			if (par.type.compare("float"))
				throw_syntax_error("'fov' must have type float.");
			if (camType.compare("perspective"))
				throw_syntax_error("'fov' is not a parameter for the current camera type.");
			auto data = (std::vector<float> *) par.value;
			// NOTE: this is true if the image is vertical
			cam->yfov = data->at(0);
			delete data;
		}
	}

	scn->cameras = std::vector<ygl::camera *>();
	scn->cameras.push_back(cam);
}

//
// execute_Film
//
void PBRTParser::execute_Film() {
	
	// First parameter is the type
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string filmType = this->current_token().get_value();
	this->advance();

	if (filmType.compare("image"))
		throw_syntax_error("Only image \"film\" is supported.");

	int xres = 0;
	int yres = 0;
	// read parameters
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("xresolution")) {
			if (par.type.compare("integer"))
				throw_syntax_error("'xresolution' must have integer type.");

			std::vector<int> *data = (std::vector<int> *)par.value;
			xres = data->at(0);
			delete data;
		}
		else if (!par.name.compare("yresolution")) {
			if (par.type.compare("integer"))
				throw_syntax_error("'yresolution' must have integer type.");

			std::vector<int> *data = (std::vector<int> *)par.value;
			yres = data->at(0);
			delete data;
		}
	}

	if (xres && yres) {
		this->defaultAspect = ((float)xres) / ((float)yres);
	}
}

//
// DESCRIBING THE SCENE
//

//
// Attributes
//

void PBRTParser::execute_AttributeBlock() {
	this->advance();
	// save the current state
	GraphicState oldState = this->gState;

	this->parse_block_content();

	this->gState = oldState;
	this->advance();
}

void PBRTParser::parse_block_content() {
	// if (attributeEnd || TransformEnd) return
}

void PBRTParser::execute_TransformBlock() {
	this->advance();
	// save the current state
	ygl::mat4f oldCTM = this->gState.CTM;

	this->parse_block_content();

	this->gState.CTM = oldCTM;
	this->advance();
}

void PBRTParser::parse_curve() {

	std::vector<ygl::vec3f> *p = nullptr; // max 4 points
	int degree = 3; // only legal options 2 and 3
	std::string type = "";
	std::vector<ygl::vec3f> *N = nullptr;
	int splitDepth = 3;
	int width = 1;

	ygl::shape *curve = new ygl::shape;

	// read parameters
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("p")) {
			if (par.type.compare("point3") && par.type.compare("point"))
				throw_syntax_error("Parameter 'p' must be of point type.");
			p = (std::vector<ygl::vec3f> *)par.value;
		}
		else if (!par.name.compare("degree")) {
			if (par.type.compare("integer"))
				throw_syntax_error("Parameter 'degree' must be of integer type.");
			std::vector<int> *data = (std::vector<int> *)par.value;
			degree = data->at(0);
			delete data;
		}
		else if (!par.name.compare("type")) {
			if (par.type.compare("string"))
				throw_syntax_error("Parameter 'type' must be of string type.");
			std::vector<std::string> *data = (std::vector<std::string> *)par.value;
			type = data->at(0);
			delete data;
		}
		else if (!par.name.compare("N")) {
			if (par.type.compare("normal3") && par.type.compare("normal"))
				throw_syntax_error("Parameter 'N' must be of normal type.");
			N = (std::vector<ygl::vec3f> *)par.value;
		}
		else if (!par.name.compare("splitdepth")) {
			if (par.type.compare("integer"))
				throw_syntax_error("Parameter 'splitdepth' must be of normal type.");
			auto data = (std::vector<int> *)par.value;
			splitDepth = data->at(0);
			delete data;
		}
		else if (!par.name.compare("width")) {
			if (par.type.compare("float"))
				throw_syntax_error("Parameter 'width' must be of float type.");
			auto data = (std::vector<float> *)par.value;
			width = data->at(0);
			delete data;
		}
	}

	if (degree == 3 && type.compare("bezier")) {
		
		// add vertex to the shape
		for (auto v : *p) {
			curve->pos.push_back(v);
			curve->radius.push_back(width);
		}
		curve->beziers.push_back({ 0, 1, 2, 3 });
		for (int i = 0; i < splitDepth; i++) {
			std::vector<int> verts;
			std::vector<ygl::vec4i> segments;
			tie(curve->beziers, verts, segments) =
				ygl::subdivide_bezier_recursive(curve->beziers, (int)curve->pos.size());
			curve->pos = ygl::subdivide_vert_bezier(curve->pos, verts, segments);
			curve->norm = ygl::subdivide_vert_bezier(curve->norm, verts, segments);
			curve->texcoord = ygl::subdivide_vert_bezier(curve->texcoord, verts, segments);
			curve->color = ygl::subdivide_vert_bezier(curve->color, verts, segments);
			curve->radius = ygl::subdivide_vert_bezier(curve->radius, verts, segments);
		}

		// TODO: add shape to scn
	}
	else {
		throw_syntax_error("Other types of curve than cubic bezier are not supported.");
	}
}

void PBRTParser::parse_trianglemesh() {
	
	ygl::shape *triangleMesh = new ygl::shape;

	bool indicesCheck = false;
	bool PCheck = false;

	// read parameters
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("indices")) {
			if (par.type.compare("integer"))
				throw_syntax_error("'indices' parameter must have integer type.");
			std::vector<int> *data = (std::vector<int> *) par.value;
			if (data->size() % 3 != 0)
				throw_syntax_error("'indices' parameter has a value array with wrong size (must be multiple of 3).");
			int j = 0;
			while (j < data->size()) {
				ygl::vec3i triangle;
				for (int i = 0; i < 3; i++, j++) {
					triangle[i] = data->at(j);
				}
				triangleMesh->triangles.push_back(triangle);
			}
			indicesCheck = true;
			delete data;
		}
		else if (!par.name.compare("P")) {
			if (par.type.compare("point") && par.type.compare("point3"))
				throw_syntax_error("'P' parameter must be of point3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				triangleMesh->pos.push_back(p);
			}
			delete data;
			PCheck = true;
		}
		else if (!par.name.compare("N")) {
			if (par.type.compare("normal") && par.type.compare("normal3"))
				throw_syntax_error("'N' parameter must be of normal3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				triangleMesh->norm.push_back(p);
			}
			delete data;
		}
		else if (!par.name.compare("uv")) {
			if (par.type.compare("float"))
				throw_syntax_error("'uv' parameter must be of float type.");
			std::vector<float> *data = (std::vector<float> *)par.value;
			int j = 0;
			while (j < data->size()) {
				ygl::vec2f uv;
				uv[0] = data->at(j++);
				uv[1] = data->at(j++);
				triangleMesh->texcoord.push_back(uv);
			}
			delete data;
		}
	}

	if (!(indicesCheck && PCheck))
		throw_syntax_error("Missing indices or positions in triangle mesh specification.");

	// TODO: add the shape to scene
	// TODO: material (kd, ks..)
}

void PBRTParser::execute_Shape() {
	this->advance();

	// parse the shape name
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected shape name.");
	// TODO: check if the type is correct
	std::string shapeName = this->current_token().get_value();
	this->advance();
	// current transformation matrix is used to set the object to world transformation for the shape.

	if (!shapeName.compare("curve"))
		this->parse_curve();
	else if (!shapeName.compare("trianglemesh"))
		this->parse_trianglemesh();
}

void PBRTParser::execute_Object() {
	this->advance();

	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//object space to instance's coordinate space
	std::string objName = this->current_token().get_value();
	this->advance();

	// TODO: parse list of shapes

	this->advance();
}

void PBRTParser::execute_ObjectInstance() {
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//instance space to world coordinate space
	std::string objName = this->current_token().get_value();
	this->advance();
	//TODO: create instance

}

void PBRTParser::execute_LightSource() {
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");
	// NOTE: when a light source is defined, the CTM is uded to define
	// the light-to-world transformation. 
	std::string lightType = this->current_token().get_value();
	this->advance();

	if (!lightType.compare("point"))
		this->parse_PointLight();
	else if (!lightType.compare("infinite"))
		this->parse_InfiniteLight();
	else
		throw_syntax_error("Light type not supported.");
}

void PBRTParser::parse_InfiniteLight() {
	
	ygl::vec3f scale { 1, 1, 1 };
	ygl::vec3f L { 1, 1, 1 };
	std::string mapname;

	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("scale")) {
			if (par.type.compare("spectrum"))
				throw_syntax_error("'scale' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (!par.name.compare("L")) {
			if (par.type.compare("specturm"))
				throw_syntax_error("'L' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			L = data->at(0);
			delete data;
		}
		else if (!par.name.compare("mapname")) {
			if (par.type.compare("string"))
				throw_syntax_error("'mapname' parameter expects a string type.");
			auto data = (std::vector<std::string>*)par.value;
			mapname = data->at(0);
			delete data;
		}
	}

	ygl::light *lgt = new ygl::light;
	ygl::environment *env = new ygl::environment;
	lgt->env = env;
	env->ke = scale * L;
	if (mapname.length() > 0) {
		// TODO: better manage the textures
		this->load_texture_from_file(mapname, env->ke_txt);
	}

	scn->environments.push_back(env);
	scn->lights.push_back(lgt);
}

std::string process_filename(std::string filename) {
	// TODO: implement handling of relative paths, if needed
	return filename;
}

void PBRTParser::load_texture_from_file(std::string filename, ygl::texture_info &txtinfo) {	
	std::string fname = process_filename(filename);
	ygl::texture *txt = new ygl::texture;
	//TODO: maybe some bug in yocto
	//txt->hdr = ygl::load_image4f(filename); // TODO: verify format
	txtinfo.txt = txt;
}

void PBRTParser::parse_PointLight() {
	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f I{ 1, 1, 1 };
	ygl::vec3f point;

	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("scale")) {
			if (par.type.compare("spectrum"))
				throw_syntax_error("'scale' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (!par.name.compare("I")) {
			if (par.type.compare("specturm"))
				throw_syntax_error("'I' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			I = data->at(0);
			delete data;
		}
		else if (!par.name.compare("from")) {
			if (par.type.compare("point3") && par.type.compare("point"))
				throw_syntax_error("'from' parameter expects a point type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			point = data->at(0);
			delete data;
		}
	}

	ygl::shape *lgtShape = new ygl::shape;
	lgtShape->pos.push_back(point);
	lgtShape->points.push_back(0);
	lgtShape->mat->ke = I * scale;
	scn->shapes.push_back(lgtShape);
	ygl::instance *inst = new ygl::instance;
	inst->shp = lgtShape;
	//TODO: frame!! (maybe light can have fixed position, caution)
	scn->instances.push_back(inst);
	ygl::light *lgt = new ygl::light;
	lgt->ist = inst;
	scn->lights.push_back(lgt);
}

void PBRTParser::execute_AreaLightSource() {
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");
	
	std::string areaLightType = this->current_token().get_value();
	// TODO: check type
	this->advance();

	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f L{ 1, 1, 1 };
	bool twosided = false;

	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("scale")) {
			if (par.type.compare("spectrum"))
				throw_syntax_error("'scale' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (!par.name.compare("L")) {
			if (par.type.compare("specturm"))
				throw_syntax_error("'L' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			L = data->at(0);
			delete data;
		}
		else if (!par.name.compare("twosided")) {
			if (par.type.compare("bool"))
				throw_syntax_error("'twosided' parameter expects a bool type.");
			auto data = (std::vector<bool>*)par.value;
			twosided = data->at(0);
			delete data;
		}
	}

	this->gState.alInfo.active = true;
	this->gState.alInfo.L = L;
	this->gState.alInfo.twosided = twosided;
}

void PBRTParser::execute_Material() {
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");

	std::string materialType = this->current_token().get_value();
	// TODO: check material type
	// For now, implement matte, plastic, metal, mirror
	// NOTE: shapes can override material parameters
	// TODO: implement shape materials (in shape call)
	
	if (!materialType.compare("matte"))
		this->parse_material_matte();
	else if (!materialType.compare("metal"))
		this->parse_material_metal();
	else if (!materialType.compare("mix"))
		this->parse_material_mix();
	else if (!materialType.compare("plastic"))
		this->parse_material_plastic();
	else if (!materialType.compare("mirror"))
		this->parse_material_mirror();
}

void PBRTParser::parse_material_matte() {
	ygl::vec3f kd{ 0.5, 0.5, 0.5 };
	std::string textureKd = "";
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kd")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				kd = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureKd = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'Kd' parameter must be a spectrum or a texture.");
			}
		}
	}

	this->gState.mat = {};
	if (textureKd.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		this->gState.mat.kd = { 1, 1, 1 };
		this->gState.mat.kd_txt = {};
		auto it = nameToTexture.find(textureKd);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kd' parameter was not found.");
		this->gState.mat.kd_txt.txt = it->second;
	}
	else {
		this->gState.mat.kd = kd;
	}
}

void PBRTParser::parse_material_metal() {
	// TODO: default values = copper
	ygl::vec3f eta{ 0.5, 0.5, 0.5 };
	std::string textureETA = "";
	
	ygl::vec3f k { 0.5, 0.5, 0.5 };
	std::string textureK = "";

	float rs = 0.01;
	std::string textureRS = "";

	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("eta")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				eta = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureETA = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'eta' parameter must be a spectrum or a texture.");
			}
		}else if(!par.name.compare("k")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				k = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureK = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'k' parameter must be a spectrum or a texture.");
			}
		}
		else if (!par.name.compare("roughness")) {
			if (!par.type.compare("float")) {
				auto data = (std::vector<float> *)par.value;
				rs = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureRS = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'roughness' parameter must be a float or a texture.");
			}
		}
	}

	this->gState.mat = {};
	/*
	if (textureKd.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		this->gState.mat.kd = { 1, 1, 1 };
		this->gState.mat.kd_txt = {};
		auto it = nameToTexture.find(textureKd);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kd' parameter was not found.");
		this->gState.mat.kd_txt.txt = it->second;
	}
	else {
		this->gState.mat.kd = kd;
	}*/
}

void PBRTParser::parse_material_mirror() {
	ygl::vec3f kr{ 0.9, 0.9, 0.9 };
	std::string textureKr = "";
	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kr")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				kr = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureKr = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'Kr' parameter must be a spectrum or a texture.");
			}
		}
	}

	this->gState.mat = {};
	if (textureKr.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		this->gState.mat.kr = { 1, 1, 1 };
		this->gState.mat.kr_txt = {};
		auto it = nameToTexture.find(textureKr);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kr' parameter was not found.");
		this->gState.mat.kr_txt.txt = it->second;
	}
	else {
		this->gState.mat.kd = kr;
	}
}

void PBRTParser::parse_material_mix() {
	// TODO
}

void PBRTParser::parse_material_plastic() {
	ygl::vec3f kd{ 0.25, 0.25, 0.25 };
	std::string textureKd = "";
	ygl::vec3f ks{ 0.25, 0.25, 0.25 };
	std::string textureKs = "";
	float rs = 0.1;
	std::string textureRS = "";

	while (this->current_token().get_type() != LexemeType::INDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kd")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				kd = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureKd = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'Kd' parameter must be a spectrum or a texture.");
			}
		}
		else if (!par.name.compare("Ks")) {
			if (!par.type.compare("spactrum") || !par.type.compare("rbg")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				ks = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureKs = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'Ks' parameter must be a spectrum or a texture.");
			}
		}
		else if (!par.name.compare("roughness")) {
			if (!par.type.compare("float")) {
				auto data = (std::vector<float> *)par.value;
				rs = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureRS = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'roughness' parameter must be a float or a texture.");
			}
		}
	}

	this->gState.mat = {};
	if (textureKd.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		this->gState.mat.kd = { 1, 1, 1 };
		this->gState.mat.kd_txt = {};
		auto it = nameToTexture.find(textureKd);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kd' parameter was not found.");
		this->gState.mat.kd_txt.txt = it->second;
	}
	else {
		this->gState.mat.kd = kd;
	}

	if (textureKs.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		this->gState.mat.ks = { 1, 1, 1 };
		this->gState.mat.ks_txt = {};
		auto it = nameToTexture.find(textureKs);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'ks' parameter was not found.");
		this->gState.mat.ks_txt.txt = it->second;
	}
	else {
		this->gState.mat.ks = ks;
	}
	if (textureRS.compare("")) {
		// TODO: check how yocto mixes kd and kdtxt
		// TODO: texture of only one float??
	}
	else {
		this->gState.mat.rs = rs;
	}
}

void PBRTParser::execute_MakeNamedMaterial() {
	this->advance();

	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected material name as string.");

	std::string materialName = this->current_token().get_value();
	this->advance();

	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected material name as string.");


}

void PBRTParser::execute_NamedMaterial() {

}
