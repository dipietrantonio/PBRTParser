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
#define YGL_IMAGEIO_IMPLEMENTATION 1
#define YGL_OPENGL 0
#include "yocto_gl.h"
#include <unordered_map>


// ======================================================================================
//                                UTILITIES
// ======================================================================================

//
// split
// split a string according to sep character
//
std::vector<std::string> split(std::string text, std::string sep = " ") {
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

enum LexemeType {IDENTIFIER, NUMBER, STRING, SINGLETON};

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
	std::string read_file(std::string filename);

    inline char look(int i = 0){
        return this->text[currentPos + i];
	}

    bool read_indentifier();
    bool read_number();
    bool read_string();

    public:
	std::string filename;
	std::string path;
	Lexeme currentLexeme;
    PBRTLexer(std::string text);
    bool next_lexeme();
    int get_column(){ return this->column;};
    int get_line(){ return this->line; };
};


//
// constructor
//
PBRTLexer::PBRTLexer(std::string filename){
    this->line = 1;
    this->column = 0;
	this->text = read_file(filename);
    this->lastPos = text.length() - 1;
    this->currentPos = 0;

    if(text.length() == 0)
        this->inputEnded = true;
    else
        this->inputEnded = false;

	auto path_and_name = get_path_and_filename(filename);
	this->path = path_and_name.first;
	this->filename = path_and_name.second;
}


//
// read_file
// Load a text file as a string.
//
std::string PBRTLexer::read_file(std::string filename) {
	std::fstream inputFile;
	inputFile.open(filename, std::ios::in);
	std::stringstream ss;
	std::string line;
	while (std::getline(inputFile, line)) {
		ss << line << "\n";
	}
	
	// set filename and path properties
	return ss.str();
}


//
// next_lexeme
// get the next lexeme in the file.
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
	errStr << "Lexical error (file: " << this->filename << ", line " << this->line << ", column " << this->column << ") input not recognized.";
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

   this->currentLexeme = Lexeme(LexemeType::IDENTIFIER, ss.str());
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
		char tmp = this->look();
		if (tmp == '\n') {
			this->line++;
			this->column = 0;
		}
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
            this->advance();
            continue;
        }else if(tmp == '#'){
            // comment
            while(tmp != '\n'){
                this->advance();
                tmp = this->look();
            }
			this->advance();
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
	ygl::material *mat;

};

inline std::string join_string_int(char *pref, int size) {
	char buff[100];
	sprintf(buff, "%s_%d", pref, size);
	return std::string(buff);
}

class PBRTParser {

    private:

	// PRIVATE ATTRIBUTES
	// stack of lexical analyzers
	std::vector<PBRTLexer *> lexers{};
	
	// What follows are some variables that need to be shared among
	// parsing statements
	
	// pointer to the newly created yocto scene
	ygl::scene *scn;
	// aspect ratio can be set in Camera or Film directive
	float defaultAspect = 16.0 / 9.0;
	float defaultFocus = 1;
	// The parser "head" is currently inside an object definition.
	// "execute_Shape" call needs to know this information.
	bool inObjectDefinition = false;
	// when the parser is inside an Onject environment, it needs to keep
	// track of all the shapes that define the object. While "inObjectDefinition"
	// is true, every shape parsed will be put in this vector. Since we can parse
	// one Object at time, using a single vector is fine.
	std::vector<ygl::shape*> shapesInObject = {};

	GraphicState gState{ ygl::identity_mat4f, {}, new ygl::material};

	// mappings to memorize data (textures, objects, ..) by name and use
	// them in different times duding parsing (eg for instancing).
	std::unordered_map<std::string, ygl::texture*> nameToTexture{};
	std::unordered_map<std::string, ygl::material*> nameToMaterial{};
	// name to pair (list_of_shapes, CTM)
	std::unordered_map < std::string, std::pair<std::vector<ygl::shape*>, ygl::mat4f>> nameToObject{}; // instancing

	// PRIVATE METHODS
	void advance();
	Lexeme& current_token() {
		return this->lexers.at(0)->currentLexeme;
	};

	std::string& current_path() {
		return this->lexers.at(0)->path;
	}

	std::string& current_file() {
		return this->lexers.at(0)->path;
	}
	// parse a single parameter type, name and associated value
	bool parse_parameter(PBRTParameter &par, std::string type = "");

	template<typename T1, typename T2>
	void parse_value_array(int groupSize, std::vector<T1> *vals, bool(*checkValue)(Lexeme &lexm),\
		T1(*convert)(T2*, int), T2(*convert2)(std::string));


	// The following private methods correspond to Directives in pbrt format.
	 
	void execute_Include();
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
	void execute_TransformBlock();

	void execute_Shape();
	void parse_curve(ygl::shape *shp);
	void parse_trianglemesh(ygl::shape *shp);
	// DEBUG methods
	void parse_cube(ygl::shape *shp);

	// TODO: implement Inclode directive, note that it requires 
	// to modify the stream of characters in input to the lexer 

	void load_texture_from_file(std::string filename, ygl::texture_info &txtinfo);

	void execute_ObjectBlock();
	void execute_ObjectInstance();

	void execute_LightSource();
	void parse_InfiniteLight();
	void parse_PointLight();
	void execute_AreaLightSource();

	void execute_MakeNamedMaterial();
	void execute_NamedMaterial();
	void execute_Material();
	void parse_material_matte(ygl::material *mat);
	void parse_material_plastic(ygl::material *mat);
	void parse_material_metal(ygl::material *mat);
	void parse_material_mirror(ygl::material *mat);
	void parse_material_mix(ygl::material *mat);

	void execute_Texture();

	bool parse_preworld_directives();
	bool parse_world_directives();

	void parse_world_directive();
	void parse_object_directive();

	void ignore_current_directive();

	inline void throw_syntax_error(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (file: " << this->current_file() << ", line " << this->lexers.at(0)->get_line() <<\
			", column " << this->lexers.at(0)->get_column() << "): " << msg;
        throw SyntaxErrorException(ss.str());
    };

    public:
    
	// public methods

	PBRTParser(std::string filename){
		this->lexers.push_back(new PBRTLexer(filename));
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
		this->lexers.at(0)->next_lexeme();
	}
	catch (InputEndedException ex) {
		this->lexers.erase(this->lexers.begin());
		if (lexers.size() == 0) {
			throw InputEndedException();
		}
		// after being restored, we still need to flush away the included filename.
		// See execute_Include() for more info.
		this->advance();
	}
	
};

//
// parse
//
ygl::scene *PBRTParser::parse() {
	ygl::mat4f CTM = ygl::identity_mat4f;
	this->advance();
	this->parse_preworld_directives();
	this->parse_world_directives();
	return scn;
}

void PBRTParser::ignore_current_directive() {
	this->advance();
	while (this->current_token().get_type() != LexemeType::IDENTIFIER)
		this->advance();
}

void PBRTParser::execute_Include() {
	this->advance();
	// now read the file to include
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected the name of the file to be included.");

	std::string fileToBeIncl = this->current_token().get_value();

	// call advance here is dangerous. It could end the parsing too soon.
	// better call it in advance() method, directly on the lexter after being
	// restored.

	if (fileToBeIncl.length() == 0)
		throw_syntax_error("Empty filename.");
	//distinguish if it is relative or absolute path
	std::stringstream ss;
	for (char ch : fileToBeIncl) {
		if (ch == '\\')
			ss << '/';
		else
			ss << ch;
	}
	fileToBeIncl = ss.str();

	if (fileToBeIncl[0] == '/' || (fileToBeIncl.length() > 3 && fileToBeIncl[1] == ':' && fileToBeIncl[2] == '/')) {
		// absolute path
		this->lexers.insert(this->lexers.begin(), new PBRTLexer(fileToBeIncl));
	}
	else {
		// relative path
		std::stringstream builtPath;
		builtPath << this->current_path() << "/" << fileToBeIncl;
		this->lexers.insert(this->lexers.begin(), new PBRTLexer(builtPath.str()));
	}
	this->advance();
}

bool PBRTParser::parse_preworld_directives() {
    // When this method starts executing, the first token must be an Identifier of
	// a directive.
	bool checkCamera = false;

	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().get_type() == LexemeType::IDENTIFIER &&
		!this->current_token().get_value().compare("WorldBegin"))) {

		if (this->current_token().get_type() != LexemeType::IDENTIFIER)
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
		else {
			this->ignore_current_directive();
		}
	}
	return checkCamera;
}



bool PBRTParser::parse_world_directives() {
	this->gState.CTM = ygl::identity_mat4f;
	this->advance();
	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().get_type() == LexemeType::IDENTIFIER &&
		!this->current_token().get_value().compare("WorldEnd"))) {
		std::cout << "Debug: " << this->current_token().get_value() << std::endl;
		this->parse_world_directive();
	}
	return true;
}

void PBRTParser::parse_world_directive() {

	if (this->current_token().get_type() != LexemeType::IDENTIFIER)
		throw_syntax_error("Identifier expected.");

	if (!this->current_token().get_value().compare("Include")) {
		this->execute_Include();
	}
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
	else if (!this->current_token().get_value().compare("AttributeBegin")) {
		this->execute_AttributeBlock();
	}
	else if (!this->current_token().get_value().compare("TransformBegin")) {
		this->execute_TransformBlock();
	}
	else if (!this->current_token().get_value().compare("Shape")) {
		this->execute_Shape();
	}
	else if (!this->current_token().get_value().compare("ObjectBegin")) {
		this->execute_ObjectBlock();
	}
	else if (!this->current_token().get_value().compare("ObjectInstance")) {
		this->execute_ObjectInstance();
	}
	else if (!this->current_token().get_value().compare("LightSource")) {
		this->execute_LightSource();
	}
	else if (!this->current_token().get_value().compare("AreaLightSource")) {
		this->execute_AreaLightSource();
	}
	else if (!this->current_token().get_value().compare("Material")) {
		this->execute_Material();
	}
	else if (!this->current_token().get_value().compare("MakeNamedMaterial")) {
		this->execute_MakeNamedMaterial();
	}
	else if (!this->current_token().get_value().compare("NamedMaterial")) {
		this->execute_NamedMaterial();
	}
	else if (!this->current_token().get_value().compare("Texture")) {
		this->execute_Texture();
	}
	else {
		this->ignore_current_directive();
	}
}

void PBRTParser::parse_object_directive() {
	if (this->current_token().get_type() != LexemeType::IDENTIFIER)
		throw_syntax_error("Identifier expected.");

	if (!this->current_token().get_value().compare("Include")) {
		this->execute_Include();
	}
	else if (!this->current_token().get_value().compare("AttributeBegin")) {
		this->execute_AttributeBlock();
	}
	else if (!this->current_token().get_value().compare("TransformBegin")) {
		this->execute_TransformBlock();
	}
	else if (!this->current_token().get_value().compare("Shape")) {
		this->execute_Shape();
	}
	else if (!this->current_token().get_value().compare("LightSource")) {
		this->execute_LightSource();
	}
	else if (!this->current_token().get_value().compare("AreaLightSource")) {
		this->execute_AreaLightSource();
	}
	else if (!this->current_token().get_value().compare("Material")) {
		this->execute_Material();
	}
	else if (!this->current_token().get_value().compare("MakeNamedMaterial")) {
		this->execute_MakeNamedMaterial();
	}
	else if (!this->current_token().get_value().compare("NamedMaterial")) {
		this->execute_NamedMaterial();
	}
	else if (!this->current_token().get_value().compare("Texture")) {
		this->execute_Texture();
	}
	else {
		this->ignore_current_directive();
	}
}

std::string check_synonym(std::string s) {
	if (!s.compare("point"))
		return std::string("point3");
	if (!s.compare("normal"))
		return std::string("normal3");
	if (!s.compare("vector"))
		return std::string("vector3");
	if (!s.compare("rgb") || !s.compare("color"))
		return std::string("rgb");

	return s;
}

bool check_type_existence(std::string &val){
	std::vector<std::string> varTypes{ "integer", "float", "point2", "vector2", "point3", "vector3",\
		"normal3", "spectrum", "bool", "string", "rgb", "color", "point", "vector", "normal", "texture" };

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
	par.type = check_synonym(std::string(tokens[0]));

    if(!type.compare("") && !(type.compare(par.type))){
        std::stringstream ss;
        ss << "Expected parameter of type `" << type << "` but got `" << par.type << "`.";
        throw_syntax_error(ss.str());
    }
	par.name = tokens[1];

	// now, according to type, we parse the value
	if (!par.type.compare("string") || !par.type.compare("texture")) {
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
	else if (!par.type.compare("point3") || !par.type.compare("normal3") || !par.type.compare("rgb") || !par.type.compare("spectrum")) {
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
	else {
		throw_syntax_error("Cannot able to parse the value.");
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
	auto transl_mat = ygl::translation_mat4(transl_vec);
	this->gState.CTM = transl_mat * this->gState.CTM;
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
	auto scale_mat = ygl::scaling_mat4(scale_vec);
	this->gState.CTM =  scale_mat * this->gState.CTM;
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
	auto rot_mat = ygl::to_mat(ygl::rotation_frame3(rot_vec, angle));
	this->gState.CTM = rot_mat * this->gState.CTM;
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

	auto mm = ygl::to_mat(ygl::lookat_frame3(eye, look, up));
	this->defaultFocus = ygl::length(eye - look);
	this->gState.CTM = mm * this->gState.CTM;
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
	cam->aperture = 0;
	cam->yfov = 90.0f * ygl::pif / 180;
	cam->focus = defaultFocus;
	cam->name = join_string_int("camera", scn->cameras.size());
	// TODO: check if this is right
	// CTM defines world to camera transformation
	cam->frame = ygl::to_frame(this->gState.CTM);

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
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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
			cam->yfov = data->at(0)  * ygl::pif / 180;
			delete data;
		}
	}
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
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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

		for (auto cam : scn->cameras)
			cam->aspect = this->defaultAspect;
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

	while (!(this->current_token().get_type() == LexemeType::IDENTIFIER &&
		!this->current_token().get_value().compare("AttributeEnd"))) {
		this->parse_world_directive();
	}
	this->gState = oldState;
	this->advance();
}


void PBRTParser::execute_TransformBlock() {
	this->advance();
	// save the current state
	ygl::mat4f oldCTM = this->gState.CTM;

	while (!(this->current_token().get_type() == LexemeType::IDENTIFIER &&
		!this->current_token().get_value().compare("TransformEnd"))) {
		this->parse_world_directive();
	}

	this->gState.CTM = oldCTM;
	this->advance();
}

void PBRTParser::parse_cube(ygl::shape *shp) {
	// DEBUG: this is a debug function
	while (this->current_token().get_type() != LexemeType::IDENTIFIER)
		this->advance();
	tie(shp->quads, shp->pos, shp->norm, shp->texcoord) = ygl::make_uvcube(0);
}

void PBRTParser::parse_curve(ygl::shape *shp) {

	std::vector<ygl::vec3f> *p = nullptr; // max 4 points
	int degree = 3; // only legal options 2 and 3
	std::string type = "";
	std::vector<ygl::vec3f> *N = nullptr;
	int splitDepth = 3;
	int width = 1;

	// read parameters
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("p")) {
			if (par.type.compare("point3"))
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
			if (par.type.compare("normal3"))
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
			shp->pos.push_back(v);
			// TODO: normals?
			shp->radius.push_back(width);
		}
		shp->beziers.push_back({ 0, 1, 2, 3 });
		for (int i = 0; i < splitDepth; i++) {
			std::vector<int> verts;
			std::vector<ygl::vec4i> segments;
			tie(shp->beziers, verts, segments) =
				ygl::subdivide_bezier_recursive(shp->beziers, (int)shp->pos.size());
			shp->pos = ygl::subdivide_vert_bezier(shp->pos, verts, segments);
			shp->norm = ygl::subdivide_vert_bezier(shp->norm, verts, segments);
			shp->texcoord = ygl::subdivide_vert_bezier(shp->texcoord, verts, segments);
			shp->color = ygl::subdivide_vert_bezier(shp->color, verts, segments);
			shp->radius = ygl::subdivide_vert_bezier(shp->radius, verts, segments);
		}
	}
	else {
		throw_syntax_error("Other types of curve than cubic bezier are not supported.");
	}
}

void PBRTParser::parse_trianglemesh(ygl::shape *shp) {

	bool indicesCheck = false;
	bool PCheck = false;

	// read parameters
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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
				shp->triangles.push_back(triangle);
			}
			indicesCheck = true;
			delete data;
		}
		else if (!par.name.compare("P")) {
			if (par.type.compare("point3"))
				throw_syntax_error("'P' parameter must be of point3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				shp->pos.push_back(p);
				shp->radius.push_back(1.0f); // default radius
			}
			delete data;
			PCheck = true;
		}
		else if (!par.name.compare("N")) {
			if (par.type.compare("normal3"))
				throw_syntax_error("'N' parameter must be of normal3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				shp->norm.push_back(p);
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
				shp->texcoord.push_back(uv);
			}
			delete data;
		}

		// TODO: materials parameters overriding
	}

	if (!(indicesCheck && PCheck))
		throw_syntax_error("Missing indices or positions in triangle mesh specification.");
}

void PBRTParser::execute_Shape() {
	this->advance();

	// parse the shape name
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected shape name.");
	// TODO: check if the shape type is correct
	std::string shapeName = this->current_token().get_value();
	this->advance();
	
	ygl::shape *shp = new ygl::shape();
	std::string shpName = join_string_int("shape", scn->shapes.size());
	// add material to shape
	shp->mat = gState.mat;
	shp->name = shpName;
	// TODO: handle when shapes override some material properties

	// TODO: arealight stuff

	if (!shapeName.compare("curve"))
		this->parse_curve(shp);
	else if (!shapeName.compare("trianglemesh"))
		this->parse_trianglemesh(shp);
	else if (!shapeName.compare("cube"))
		this->parse_cube(shp);
	else
		this->ignore_current_directive();

	// add shp in scene
	ygl::shape_group *sg = new ygl::shape_group;
	sg->shapes.push_back(shp);
	sg->name = shpName;
	scn->shapes.push_back(sg);

	if (this->inObjectDefinition) {
		shapesInObject.push_back(shp);
	}
	else {
		// add a single instance directly to the scene
		ygl::instance *inst = new ygl::instance();
		inst->shp = sg;
		// TODO: check the correctness of this
		// NOTE: current transformation matrix is used to set the object to world transformation for the shape.
		inst->frame = ygl::to_frame(this->gState.CTM);
		inst->name = join_string_int("instance", scn->instances.size());
		scn->instances.push_back(inst);
	}
}

void PBRTParser::execute_ObjectBlock() {
	if (this->inObjectDefinition)
		throw_syntax_error("Cannot define an object inside another object.");

	this->advance();
	this->inObjectDefinition = true;
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//object space to instance's coordinate space
	std::string objName = this->current_token().get_value();
	if (nameToObject.find(objName) != nameToObject.end())
		throw_syntax_error("There already exists an object with the specified name.");
	this->advance();

	while (!(this->current_token().get_type() == LexemeType::IDENTIFIER &&
		!this->current_token().get_value().compare("ObjectEnd"))) {
		this->parse_object_directive();
	}

	nameToObject.insert(std::make_pair(objName, std::make_pair(std::vector<ygl::shape*>(shapesInObject), this->gState.CTM)));
	this->inObjectDefinition = false;
	this->shapesInObject.clear();
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

	auto obj = nameToObject.find(objName);
	if (obj == nameToObject.end())
		throw_syntax_error("Object name not found.");

	auto shapes = (obj->second).first;
	ygl::mat4f objectToInstanceCTM = (obj->second).second;
	ygl::mat4f finalCTM = this->gState.CTM * objectToInstanceCTM;

	ygl::shape_group *sg = new ygl::shape_group;
	ygl::instance *inst = new ygl::instance;
	inst->shp = sg;
	for (auto shp : shapes) {
		sg->shapes.push_back(shp);
	}
	// TODO: check the correctness of this
	inst->frame = ygl::to_frame(finalCTM);
	scn->instances.push_back(inst);
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
	// TODO: conversion from spectrum to rgb
	ygl::vec3f scale { 1, 1, 1 };
	ygl::vec3f L { 1, 1, 1 };
	std::string mapname;

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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
			if (par.type.compare("spectrum") && par.type.compare("rgb")) // implements black body
				throw_syntax_error("'L' parameter expects a spectrum or rgb type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			if(!par.type.compare("rgb"))
				L = ygl::rgb_to_xyz(data->at(0));
			else
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

	ygl::environment *env = new ygl::environment;
	env->name = join_string_int("env", scn->environments.size());
	env->ke = scale * L;
	if (mapname.length() > 0) {
		// TODO: better manage the textures
		this->load_texture_from_file(mapname, env->ke_txt);
	}

	scn->environments.push_back(env);
}

std::string process_filename(std::string filename) {
	// TODO: implement handling of relative paths, if needed
	return filename;
}

void PBRTParser::load_texture_from_file(std::string filename, ygl::texture_info &txtinfo) {	
	std::string fname = process_filename(filename);
	ygl::texture *txt = new ygl::texture;
	//TODO: maybe some bug in yocto
	txtinfo.txt = txt;
}

void PBRTParser::parse_PointLight() {
	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f I{ 1, 1, 1 };
	ygl::vec3f point;

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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
			if (par.type.compare("spectrum"))
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

	ygl::shape_group *sg = new ygl::shape_group;
	sg->name = join_string_int("shape", scn->shapes.size());

	ygl::shape *lgtShape = new ygl::shape;
	lgtShape->name = sg->name;
	lgtShape->pos.push_back(point);
	lgtShape->points.push_back(0);
	// default radius
	lgtShape->radius.push_back(1.0f);

	sg->shapes.push_back(lgtShape);

	ygl::material *lgtMat = new ygl::material;
	lgtMat->ke = I * scale;
	lgtShape->mat = lgtMat;
	lgtMat->name = join_string_int("material", scn->materials.size());
	scn->materials.push_back(lgtMat);

	scn->shapes.push_back(sg);
	ygl::instance *inst = new ygl::instance;
	inst->shp = sg;
	// TODO check validity of frame
	inst->frame = ygl::to_frame(gState.CTM);
	inst->name = join_string_int("instance", scn->instances.size());
	std::cout << "lightframe: " << inst->frame.o << "\n";
	scn->instances.push_back(inst);
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

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
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
	this->advance();
	// TODO: check material type
	// For now, implement matte, plastic, metal, mirror
	// NOTE: shapes can override material parameters
	this->gState.mat = new ygl::material();
	this->gState.mat->name = join_string_int("material", scn->materials.size());
	this->gState.mat->type = ygl::material_type::specular_roughness;
	
	scn->materials.push_back(this->gState.mat);
	
	if (!materialType.compare("matte"))
		this->parse_material_matte(this->gState.mat);
	else if (!materialType.compare("metal"))
		this->parse_material_metal(this->gState.mat);
	else if (!materialType.compare("mix"))
		this->parse_material_mix(this->gState.mat);
	else if (!materialType.compare("plastic"))
		this->parse_material_plastic(this->gState.mat);
	else if (!materialType.compare("mirror"))
		this->parse_material_mirror(this->gState.mat);
	else
		throw_syntax_error("Material not supported.");
}

void PBRTParser::parse_material_matte(ygl::material *mat) {
	ygl::vec3f kd{ 0.5f, 0.5f, 0.5f };
	std::string textureKd = "";

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kd")) {
			if (!par.type.compare("spectrum") || !par.type.compare("rgb")) {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				// TODO: conversion rgb - spectrum
				kd = data->at(0);
				delete data;
			}
			else if (!par.type.compare("texture")) {
				auto data = (std::vector<std::string> *)par.value;
				textureKd = data->at(0);
				delete data;
			}
			else {
				throw_syntax_error("'Kd' parameter must be a spectrum, rgb or a texture.");
			}
		}
	}

	if (textureKd.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		mat->kd = { 1, 1, 1 };
		mat->kd_txt = {};
		auto it = nameToTexture.find(textureKd);
		std::cout << "Im here\n";
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kd' parameter was not found.");
		mat->kd_txt.txt = it->second;
	}
	else {
		mat->kd = kd;
		mat->ks = ygl::zero3f;
		mat->rs = 1;
	}
}

void PBRTParser::parse_material_metal(ygl::material *mat) {
	// TODO: default values = copper
	ygl::vec3f eta{ 0.5, 0.5, 0.5 };
	std::string textureETA = "";
	
	ygl::vec3f k { 0.5, 0.5, 0.5 };
	std::string textureK = "";

	float rs = 0.01;
	std::string textureRS = "";

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("eta")) {
			if (!par.type.compare("spectrum") || !par.type.compare("rbg")) {
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
			if (!par.type.compare("spectrum") || !par.type.compare("rbg")) {
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

void PBRTParser::parse_material_mirror(ygl::material *mat) {
	ygl::vec3f kr{ 0.9f, 0.9f, 0.9f };
	std::string textureKr = "";
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kr")) {
			if (!par.type.compare("spectrum") || !par.type.compare("rbg")) {
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
		mat->kr = { 1, 1, 1 };
		mat->kr_txt = {};
		auto it = nameToTexture.find(textureKr);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kr' parameter was not found.");
		mat->kr_txt.txt = it->second;
	}
	else {
		mat->kd = kr;
	}
}

void PBRTParser::parse_material_mix(ygl::material *mat) {
	// TODO
}

void PBRTParser::parse_material_plastic(ygl::material *mat) {
	ygl::vec3f kd{ 0.25, 0.25, 0.25 };
	std::string textureKd = "";
	ygl::vec3f ks{ 0.25, 0.25, 0.25 };
	std::string textureKs = "";
	float rs = 0.1;
	std::string textureRS = "";

	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("Kd")) {
			if (!par.type.compare("spectrum") || !par.type.compare("rbg")) {
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
			if (!par.type.compare("spectrum") || !par.type.compare("rbg")) {
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
		mat->kd = { 1, 1, 1 };
		mat->kd_txt = {};
		auto it = nameToTexture.find(textureKd);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'kd' parameter was not found.");
		mat->kd_txt.txt = it->second;
	}
	else {
		mat->kd = kd;
	}

	if (textureKs.compare("")) {
		//TODO: check how yocto mixes kd and kdtxt
		mat->ks = { 1, 1, 1 };
		mat->ks_txt = {};
		auto it = nameToTexture.find(textureKs);
		if (it == nameToTexture.end())
			throw_syntax_error("the specified texture for 'ks' parameter was not found.");
		mat->ks_txt.txt = it->second;
	}
	else {
		mat->ks = ks;
	}
	if (textureRS.compare("")) {
		// TODO: check how yocto mixes kd and kdtxt
		// TODO: texture of only one float??
	}
	else {
		mat->rs = rs;
	}
}

void PBRTParser::execute_MakeNamedMaterial() {
	this->advance();

	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected material name as string.");

	std::string materialName = this->current_token().get_value();
	if (nameToMaterial.find(materialName) != nameToMaterial.end())
		throw_syntax_error("A material with the specified name already exists.");
	this->advance();

	PBRTParameter par;
	this->parse_parameter(par, "string");
	if (par.name.compare("type"))
		throw_syntax_error("Expected material type.");
	auto data = (std::vector<std::string> *)par.value;
	std::string materialType = data->at(0);
	delete data;
	
	ygl::material *mat = new ygl::material;
	mat->name = join_string_int("material", scn->materials.size());

	if (!materialType.compare("metal"))
		this->parse_material_metal(mat);
	else if (!materialType.compare("plastic"))
		this->parse_material_plastic(mat);
	else if (!materialType.compare("matte"))
		this->parse_material_matte(mat);
	else if (!materialType.compare("mirror"))
		this->parse_material_mirror(mat);
	else if (!materialType.compare("mix"))
		this->parse_material_mix(mat);
	else
		throw_syntax_error("Material type not supported or recognized.");
	nameToMaterial.insert(std::make_pair(materialName, mat));
}

void PBRTParser::execute_NamedMaterial() {
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected material name string.");
	std::string materialName = this->current_token().get_value();

	auto it = nameToMaterial.find(materialName);
	if (it == nameToMaterial.end())
		throw_syntax_error("No material with the specified name.");
	auto mat = it->second;
	this->gState.mat = mat;
}

void PBRTParser::execute_Texture() {
	// NOTE: repeat information is lost. One should save texture_info instead,
	// but then one should keep track of the various textures.
	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected texture name string.");
	std::string textureName = this->current_token().get_value();
	
	if (nameToTexture.find(textureName) != nameToTexture.end())
		throw_syntax_error("Texture name already used.");

	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected texture type string.");
	std::string textureType = this->current_token().get_value();

	if (textureType.compare("spectrum") && textureType.compare("rbg")
		&& textureType.compare("float"))
		throw_syntax_error("Unsupported texture type.");

	this->advance();
	if (this->current_token().get_type() != LexemeType::STRING)
		throw_syntax_error("Expected texture class string.");
	std::string textureClass = this->current_token().get_value();

	if (textureClass.compare("imagemap"))
		throw_syntax_error("Only 'imagemap' texture type is supported.");
	this->advance();

	std::string filename;
	while (this->current_token().get_type() != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (!par.name.compare("filename")) {
			if (par.type.compare("string"))
				throw_syntax_error("'filename' parameter must have string type.");
			auto data = (std::vector<std::string> *)par.value;
			filename = data->at(0);
			delete data;
		}
	}
	ygl::texture *txt = new ygl::texture;
	scn->textures.push_back(txt);
	txt->path = filename;
	if (ygl::endswith(filename, ".png")) {
		txt->ldr = ygl::load_image4b(filename);
	}
	else if(ygl::endswith(filename, ".exr")){
		txt->hdr = ygl::load_image4f(filename);
	}
	else {
		throw_syntax_error("Texture format not recognised.");
	}
	nameToTexture.insert(std::make_pair(textureName, txt));
}