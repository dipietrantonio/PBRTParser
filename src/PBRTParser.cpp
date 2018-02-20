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

class PBRTParser {

    private:
    PBRTLexer *lexer;
	bool inputEnded = false;

	void advance() {
		try {
			this->lexer->next_lexeme();
		}
		catch (InputEndedException ex) {
			inputEnded = true;
		}
	};

	Lexeme& current_token() {
		return this->lexer->currentLexeme;
	};

	bool parse_preworld_statements(ygl::scene *scn, ygl::mat4f &CTM);
    bool parse_world_statements(ygl::scene *scn, ygl::mat4f &CTM);
    
	bool parse_parameter(PBRTParameter &par, std::string type = "");
	template<typename T1, typename T2>
	void parse_value_array(int groupSize, std::vector<T1> *vals, bool(*checkValue)(Lexeme &lexm),\
		T1(*convert)(T2*, int), T2(*convert2)(std::string));

	/*
	 * The following private methods correspond to Directives in pbrt format.
	 */
	void execute_Camera(ygl::scene *scn, ygl::mat4f &CTM);
	void execute_Translate(ygl::mat4f &CTM);
	void execute_Scale(ygl::mat4f &CTM);
	void execute_Rotate(ygl::mat4f &CTM);
	void execute_LookAt(ygl::mat4f &CTM);
	void execute_Film(ygl::scene *scn);

	inline void throw_syntax_error(std::string msg){
        std::stringstream ss;
        ss << "Syntax Error (line " << this->lexer->get_line() << ", column " << this->lexer->get_column() << "): " << msg;
        throw SyntaxErrorException(ss.str());
    };

    public:
    PBRTParser(std::string text){
        this->lexer = new PBRTLexer(text);
    };
    void parse(ygl::scene *scn);

};

void PBRTParser::parse(ygl::scene *scn) {
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
			return;
		}
		catch (LexicalErrorException ex) {
			std::cout << "ERRR2\n";
			std::cout << ex.what() << std::endl;
			return;
		}
	}
}

bool PBRTParser::parse_preworld_statements(ygl::scene *scn, ygl::mat4f &CTM) {
    // TODO
	return false;
}

bool PBRTParser::parse_world_statements(ygl::scene *scn, ygl::mat4f &CTM) {
	// TODO
	CTM = ygl::identity_mat4f;
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

void PBRTParser::execute_Translate(ygl::mat4f &CTM) {
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
	CTM = CTM * transl_mat;
	this->advance();
}

void PBRTParser::execute_Scale(ygl::mat4f &CTM){
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
	CTM = CTM * scale_mat;
	this->advance();
}
void PBRTParser::execute_Rotate(ygl::mat4f &CTM) {
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
	CTM = CTM * rot_mat;
	this->advance();
}

void PBRTParser::execute_LookAt(ygl::mat4f &CTM){
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
	CTM = CTM * mm;
	this->advance();
}

//
// SCENE-WIDE RENDERING OPTIONS
//


//
// execute_Camera
// Parse camera information.
// NOTE: only support perspective camera for now.
//
void PBRTParser::execute_Camera(ygl::scene *scn, ygl::mat4f &CTM) {
	ygl::camera *cam = new ygl::camera;
	cam->aspect = 0.0f;
	cam->aperture = 0.0f;
	cam->yfov = 90.0f;
	cam->focus = powf(10, 30);

	// TODO: check if this is right
	// CTM defines world to camera transformation
	cam->frame = ygl::to_frame3f(CTM);

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

void PBRTParser::execute_Film(ygl::scene *scn) {

}
