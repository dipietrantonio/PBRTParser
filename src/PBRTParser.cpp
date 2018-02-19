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
};

class SyntaxErrorException : public std::exception{

    private:
    std::string message;

    public:
    SyntaxErrorException(std::string msg){
        this->message = msg;
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

    bool read_indentifier(Lexeme &lex);
    bool read_number(Lexeme &lex);
    bool read_string(Lexeme &lex);

    public:
    PBRTLexer(std::string text);
    bool get_next_lexeme(Lexeme &lex);
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
bool PBRTLexer::get_next_lexeme(Lexeme &lex){
    this->remove_blanks();

    if(this->read_indentifier(lex))
        return true;
    if(this->read_string(lex))
        return true;
    if(this->read_number(lex))
        return true;
    
    char c = this->look();
    if(c == '[' || c == ']' || c == '\n'){
        std::stringstream ss = std::stringstream();
        ss.put(c);
        lex = Lexeme(LexemeType::SINGLETON, ss.str());
        if(c == '\n')
            this->column++;
        this->advance();
        return true;
    }

    std::stringstream errStr;
    errStr << "Lexical error (line " << this->line << ", column " << this->column << ") input not recognized.";
    throw LexicalErrorException(errStr.str());
}

bool PBRTLexer::read_indentifier(Lexeme &lex){
    char c = this->look();
    
    std::stringstream ss;
    if(!std::isalpha(c))
        return false;
    do{
        ss << c;
        this->advance();
    }while(std::isalpha(c = this->look()));

   lex = Lexeme(LexemeType::INDENTIFIER, ss.str());
   return true;
}

bool PBRTLexer::read_string(Lexeme &lex){
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
   lex = Lexeme(LexemeType::STRING, ss.str());
   return true;
}

bool PBRTLexer::read_number(Lexeme &lex){
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
    lex = Lexeme(LexemeType::STRING, ss.str());
    return true;
}

//
// advance
// Moves the lexer to the next character in the string.
//
void PBRTLexer::advance(){
    if(this->currentPos < this->lastPos)
        this->currentPos++;
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
        if(tmp == ' ' || tmp == '\t'){
            // '\n' is part of the grammar
            this->column++;
            this->advance();
            continue;
        }else if(tmp == '#'){
            // comment
            while(tmp != '\n'){
                this->advance();
                tmp = this->look();
                this->column++;
            }
            this->line++;
            continue;
        }else{
            // meaningful part of the input starts, exit the routine.
            return;
        }
    }
}

/*
 * PBRTParser
 * Parses a stream of lexemes.
 */

struct PBRTParameter{
    std::string type;
    std::string name;
    void *value;
};

class PBRTParser {

    private:
    PBRTLexer *lexer;
    void parse_preworld_statements(ygl::scene *scn);
    void parse_world_statements(ygl::scene *scn);
    void parse_parameter(PBRTParameter &par, std::string type = "");

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


void PBRTParser::parse_preworld_statements(ygl::scene *scn){
    // TODO
}

void PBRTParser::parse_world_statements(ygl::scene *scn) {
	// TODO
}
void parse_statement(){

}

std::vector<std::string> split(std::string text){

    std::vector<std::string> results;
    std::stringstream ss;
    
    for (char c : text){
        if(c == ' '){
            auto str = ss.str();
            if(str.size() > 0){
                results.push_back(str);
                ss = std::stringstream();
            }
        }else{
            ss << c;
        }
    }
    auto str = ss.str();
    if(str.length() > 0)
        results.push_back(str);
    return results;
}

bool check_type_existence(std::string &val){
    if(!val.compare("integer")) return true;
    if(!val.compare("float")) return true;
    if(!val.compare("point2")) return true;
    if(!val.compare("vector2")) return true;
    if(!val.compare("point3")) return true;
    if(!val.compare("vector3")) return true;
    if(!val.compare("normal3")) return true;
    if(!val.compare("spectrum")) return true;
    if(!val.compare("bool")) return true;
    if(!val.compare("string")) return true;
	if (!val.compare("rgb")) return true;
	if (!val.compare("color")) return true;
    if(!val.compare("point")) return true;
    if(!val.compare("vector")) return true;
    if(!val.compare("normal")) return true;
    return false;
}

void PBRTParser::parse_parameter(PBRTParameter &par, std::string type){
    
    Lexeme lex;
    this->lexer->get_next_lexeme(lex);
    
    if(lex.get_type() != LexemeType::STRING)
        throw_syntax_error("Expected a string of the form \"type name\".");
    auto tokens = split(lex.get_value());
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
		this->lexer->get_next_lexeme(lex);
		// it can be a single value or multiple values
		if (lex.get_type() == LexemeType::STRING) {
			// single value
			vals->push_back(lex.get_value());
		}
		else if (lex.get_type() == LexemeType::SINGLETON && !lex.get_value().compare("[")) {
			// start array of value(s)
			while(true) {
				this->lexer->get_next_lexeme(lex);
				if (lex.get_type() == LexemeType::SINGLETON && !lex.get_value().compare("]"))
					break; // finished to read the array
				if (lex.get_type() != LexemeType::STRING)
					throw_syntax_error("One of the values is not a string.");
				vals->push_back(lex.get_value());
			}
		}
		else {
			throw_syntax_error("Expected a string value.");
		}
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
	else if (!par.type.compare("point") || !par.type.compare("point3") || par.type.compare("normal") || !par.type.compare("normal3")) {
		std::vector<ygl::vec3f> *vals = new std::vector<ygl::vec3f>();
		this->lexer->get_next_lexeme(lex);
		if (lex.get_type() == LexemeType::SINGLETON && !lex.get_value().compare("[")) {
			// start array of value(s)
			bool stopped = false;
			while (true) {
				ygl::vec3f value;
				for (int i = 0; i < 3; i++) {
					this->lexer->get_next_lexeme(lex);
					if (lex.get_type() == LexemeType::SINGLETON && !lex.get_value().compare("]")) {
						if (i == 0) {
							stopped = true;
							break;
						}
						else
							throw_syntax_error("Too few values specified.");
					}
						
					if (lex.get_type() != LexemeType::NUMBER)
						throw_syntax_error("One of the values is not a number.");
					value[i] = atof(lex.get_value().c_str());
				}
				if(!stopped)
					vals->push_back(value);
			}
		}
		else {
			throw_syntax_error("Expected a string value.");
		}
		if (vals->size() == 0)
			throw_syntax_error("No value specified.");
		par.value = (void *)vals;
	}
}