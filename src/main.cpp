
#include "PBRTParser.h"
#include <fstream>

int main(int argc, char** argv){
	
	if (argc < 3)
	{
		printf("Usage: command <input_scene_file> <output_scene_file>\n");
		exit(1);
	}
	auto parser = PBRTParser(argv[1]);
	ygl::scene *scn;
	try {
		scn = parser.parse();
	}
	catch (std::exception ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}

	try {
		std::cout << "Conversion ended. Saving obj to file..\n";
		auto so = ygl::save_options();
		so.skip_missing = false;
		ygl::save_scene(argv[2], scn, so);
	}
	catch (PBRTException ex) {
		std::cout << ex.what() << "\n";
	}
	
	return 0;
}
