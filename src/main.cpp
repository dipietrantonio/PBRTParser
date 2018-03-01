
#include "PBRTParser.cpp"
#include <fstream>

int main(int argc, char** argv){
	
	
	auto parser = PBRTParser("input.txt");
	ygl::scene *scn;
	try {
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