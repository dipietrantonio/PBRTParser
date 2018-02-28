
#include "PBRTParser.cpp"
#include <fstream>

int main(int argc, char** argv){
	
	if (argc < 2)
	{
		printf("Required source\n");
		return 1;
	}

	std::fstream inputFile;
	inputFile.open("input.txt", std::ios::in);
	std::stringstream ss;
	std::string line;
	while (std::getline(inputFile, line)) {
		ss << line << "\n";
	}
    auto istr =ss.str();
	std::cout << "check 1\n";
	auto parser = PBRTParser(istr);
	ygl::scene *scn;
	try {
		if (argv[1][0] == 'c')
			scn = ygl::make_cornell_box_scene();
		else
		 scn = parser.parse();
	}
	catch (SyntaxErrorException ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
	catch (LexicalErrorException ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}
	ygl::save_scene("C:\\Users\\cristian\\Desktop\\itrace\\prova.obj", scn, ygl::save_options());
	return 0;
}