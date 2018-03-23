
#include "PBRTParser.h"
#include <fstream>

int main(int argc, char** argv){
	/*std::cout << "Loading scene\n";
	auto obj = ygl::load_scene("C:/Users/cristian/Desktop/itrace/land.obj");
	std::cout << "make bvh\n";
	auto bvh = ygl::make_bvh(obj);
	auto tp = ygl::trace_params();
	tp.nsamples = 32;

	std::cout << "tracing..\n";
	ygl::trace_image(obj, obj->cameras[0], bvh, tp);
	exit(1);
	*/
	if (argc < 3)
	{
		//printf("Arguments missing.\n");
		//exit(1);
	}
	auto parser = PBRTParser(argv[1]);
	//auto parser = PBRTParser("C:\\Users\\cristian\\Documents\\Università\\Magistrale\\ComputerGraphics\\PBRTParser\\bin\\Release\\simple\\prova.pbrt");
	ygl::scene *scn;
	try {
		scn = parser.parse();
	}
	catch (std::exception ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}

	/*std::cout << "Trace scene..\n";
	auto par = ygl::trace_params();
	par.nsamples = 16;
	par.parallel = true;
	auto img = ygl::trace_image(scn, scn->cameras[0], ygl::make_bvh(scn), par);
	std::cout << "Save gen..\n";
	ygl::save_image("provad2.png", img, 1, 2.2);
	std::cout << "Save scene\n";
	std::cout << "Fine\n";*/
	//ygl::save_scene(argv[2], scn, ygl::save_options());
	ygl::save_scene("C:\\Users\\cristian\\Desktop\\itrace\\land.obj", scn, ygl::save_options());
	return 0;
}