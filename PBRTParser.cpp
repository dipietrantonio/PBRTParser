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

enum LexemeType {INDENTIFIER, NUMBER, STRING, ARRAY, SINGLETON};

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
    try{
        this->remove_blanks();

    if(this->read_indentifier(lex))
        return true;
    if(this->read_string(lex))
        return true;
    if(this->read_number(lex))
        return true;
    
    char c = this->look();
    if(c == '[' || c == ']'){
        std::stringstream ss = std::stringstream();
        ss.put(c);
        lex = Lexeme(LexemeType::SINGLETON, ss.str());
        return true;
    }

    std::stringstream errStr;
    errStr << "Lexical error (line " << this->line << ", column " << this->column << ") input not recognized.";
    throw LexicalErrorException(errStr.str());

    }catch(InputEndedException c){
        return false;
    }
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
class PBRTParser {

    private:
    PBRTLexer *lexer;

    public:
    PBRTParser(std::string)

}