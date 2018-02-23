#include "PBRTParser.cpp"
#include <fstream>

int main(int argc, char** argv){
    if (argc < 2){
        printf("Usage: parse <filename>\n");
        exit(1);
    }
	std::fstream inputFile;
	inputFile.open(argv[1], std::ios::in);
	std::stringstream ss;
	std::string line;
	while (std::getline(inputFile, line)) {
		ss << line << "\n";
	}
    auto istr =ss.str();
	auto parser = PBRTParser(istr);
	parser.parse();
}