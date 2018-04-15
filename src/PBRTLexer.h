#ifndef __PBRTLEXER__
#define __PBRTLEXER__
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <cctype>
#include "utils.h"

class InputEndedException : public std::exception {

	virtual const char *what() const throw() {
		return "Input has ended.";
	}
};

class PBRTException : public std::exception {

	private:
	std::string _msg;

	public:
	PBRTException(std::string message) : _msg(message) {};
	virtual const char *what() const throw() {
		return _msg.c_str();
	}
};


enum LexemeType { IDENTIFIER, NUMBER, STRING, SINGLETON };

class Lexeme {
public:
	std::string value;
	LexemeType type;
	Lexeme() {};
	Lexeme(LexemeType type, std::string value) {
		this->type = type;
		this->value = value;
	}
};

class PBRTLexer {

private:
	// current line and column number in the file
	int line, column;
	// current position of the Lexer's head
	int currentPos;
	// text to be parsed
	std::string text;
	bool inputEnded;
	// index of last character of the text to be parsed
	int lastPos;

	// Private methods

	// remove whitespaces
	void remove_blanks();
	// advance the lexer head (currentPos)
	void advance();
	// read the file and store the content in text
	std::string read_file(std::string filename);
	// see the current character
	inline char peek(int i = 0) {
		return this->text[currentPos + i];
	}

	// the following functions implements reg exp parsers 
	// to get meaningful elements in the text (i.e. grammar's terminal symbols).
	bool read_indentifier();
	bool read_number();
	bool read_string();

public:
	std::string filename;
	std::string path;
	Lexeme currentLexeme;
	PBRTLexer(std::string text);
	bool next_lexeme();
	int get_column() { return this->column; };
	int get_line() { return this->line; };
};
#endif