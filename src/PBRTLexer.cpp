#include "PBRTLexer.h"

//
// constructor
//
PBRTLexer::PBRTLexer(std::string filename) {
	this->line = 1;
	this->column = 0;
	this->text = read_file(filename);
	this->lastPos = text.length() - 1;
	this->currentPos = 0;

	if (text.length() == 0)
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
	auto textToParse = ss.str();
	inputFile.close();
	return textToParse;
}


//
// next_lexeme
// get the next lexeme in the file.
//
bool PBRTLexer::next_lexeme() {
	this->remove_blanks();

	if (this->read_indentifier())
		return true;
	if (this->read_string())
		return true;
	if (this->read_number())
		return true;

	char c = this->peek();
	if (c == '[' || c == ']') {
		std::stringstream ss = std::stringstream();
		ss.put(c);
		this->currentLexeme = Lexeme(LexemeType::SINGLETON, ss.str());
		this->advance();
		return true;
	}

	std::stringstream errStr;
	errStr << "Lexical error (file: " << this->filename << ", line " << this->line << ", column " << this->column << ") input not recognized.";
	throw PBRTException(errStr.str());
}


bool PBRTLexer::read_indentifier() {
	char c = this->peek();

	std::stringstream ss;
	if (!std::isalpha(c))
		return false;
	do {
		ss << c;
		this->advance();
	} while (std::isalpha(c = this->peek()));

	this->currentLexeme = Lexeme(LexemeType::IDENTIFIER, ss.str());
	return true;
}


bool PBRTLexer::read_string() {
	char c = this->peek();
	std::stringstream ss;
	// remove "
	if (!(c == '"'))
		return false;
	this->advance();
	while ((c = this->peek()) != '"') {
		ss << c;
		this->advance();
	}
	this->advance();
	this->currentLexeme = Lexeme(LexemeType::STRING, ss.str());
	return true;
}

bool PBRTLexer::read_number() {
	char c = this->peek();
	if (!(c == '+' || c == '-' || c == '.' || std::isdigit(c)))
		return false;
	std::stringstream ss;
	bool point_seen = false;
	/*
	* state = 0: a digit, `+`, `-` or `.` expected
	* state = 7: `+` or `-` seen, waiting for one digit or dot (7 because otherwise I had to rename all the others)
	* state = 1: waiting for one mandatory digit
	* state = 2: waiting for more digits or `.` (if not seen before) [final state]
	* state = 3: waiting for zero or ore digits after point [final state]
	* state = 4: waiting for `-`, `+` or a digit after "E" (exponential)
	* state = 5: waiting for mandatory digit
	* state = 6: waiting for zero or more additional digits [final state]
	* state = 7: waiting for mandatoty digit or dot
	*/
	int state = 0;

	while (true) {
		if (state == 0 && (c == '-' || c == '+' || c == '.')) {
			if (c == '.') {
				point_seen = true;
				state = 1; // at least one digit needed
			}
			else {
				state = 7;
			}
		}
		else if (state == 0 && std::isdigit(c)) {
			state = 2;
		}
		else if (state == 1 && std::isdigit(c)) {
			if (point_seen)
				state = 3;
			else
				state = 2;
		}
		else if (state == 2 && c == '.' && !point_seen) {
			point_seen = true;
			state = 3;
		}
		else if (state == 2 && std::isdigit(c)) {
			state = 2;
		}
		else if (state == 2 && (std::tolower(c) == 'e')) {
			state = 4;
		}
		else if (state == 3 && std::isdigit(c)) {
			state = 3;
		}
		else if (state == 3 && (std::tolower(c) == 'e')) {
			state = 4;
		}
		else if (state == 4 && (c == '-' || c == '+')) {
			state = 5;
		}
		else if (state == 4 && std::isdigit(c)) {
			state = 6;
		}
		else if (state == 5 && std::isdigit(c)) {
			state = 6;
		}
		else if (state == 6 && std::isdigit(c)) {
			state = 6;
		}
		else if (state == 7 && std::isdigit(c)) {
			state = 2;
		}
		else if (state == 7 && c == '.') {
			point_seen = true;
			state = 1;
		}
		else if (state == 2 || state == 3 || state == 6) {
			// legal point of exit
			break;
		}
		else {
			std::stringstream errstr;
			errstr << "Lexical error (line: " << this->line << ", column " << this->column << "): wrong litteral specification.";
			throw PBRTException(errstr.str());
		}
		ss << c;
		this->advance();
		c = this->peek();
	}
	this->currentLexeme = Lexeme(LexemeType::NUMBER, ss.str());
	return true;
}

//
// advance
// Moves the lexer to the next character in the string.
//
void PBRTLexer::advance() {
	if (this->currentPos < this->lastPos) {
		this->currentPos++;
		this->column++;
		char tmp = this->peek();
		if (tmp == '\n') {
			this->line++;
			this->column = 0;
		}
		this->column++;
	}
	else if (this->inputEnded)
		throw InputEndedException();
	else {
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
void PBRTLexer::remove_blanks() {
	while (true) {
		char tmp = this->peek();
		if (tmp == ' ' || tmp == '\t' || tmp == '\r' || tmp == '\n') {
			this->advance();
			continue;
		}
		else if (tmp == '#') {
			// comment
			while (tmp != '\n') {
				this->advance();
				tmp = this->peek();
			}
			this->advance();
			continue;
		}
		else {
			// meaningful part of the input starts, exit the routine.
			return;
		}
	}
}
