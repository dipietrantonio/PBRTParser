#include "PBRTParser.h"

// TODOs
//	- Fix memory leaks (use shared pointers?)
//  - spectrum type support http://www.fourmilab.ch/documents/specrend/


// =====================================================================================
//                           TYPE CHECKING
// =====================================================================================

//
// check_synonym
// some types are synonyms, transform them to default.
//
std::string PBRTParser::check_synonyms(std::string s) {
	if (s == "point")
		return std::string("point3");
	if (s == "normal")
		return std::string("normal3");
	if (s == "vector")
		return std::string("vector3");
	if (s == "color")
		return std::string("rgb");

	return s;
}

//
// fill_parameter_to_type_mapping
//
#define MP std::make_pair<std::string, std::vector<std::string>>

void PBRTParser::fill_parameter_to_type_mapping() {
	// camera parameters
	parameterToType.insert(MP("frameaspectratio", { "float" }));
	parameterToType.insert(MP("lensradius", { "float" }));
	parameterToType.insert(MP("focaldistance", { "float" }));
	parameterToType.insert(MP("fov", { "float" }));
	// film
	parameterToType.insert(MP("xresolution", { "integer" }));
	parameterToType.insert(MP("yresolution", { "integer" }));
	parameterToType.insert(MP("lensradius", { "float" }));
	// curve
	parameterToType.insert(MP("p", { "point3" }));
	parameterToType.insert(MP("type", { "string" }));
	parameterToType.insert(MP("N", { "normal3" }));
	parameterToType.insert(MP("splitdepth", { "integer" }));
	parameterToType.insert(MP("width", { "float" }));
	// triangle mesh
	parameterToType.insert(MP("indices", { "integer" }));
	parameterToType.insert(MP("P", { "point3" }));
	parameterToType.insert(MP("uv", { "float" }));
	// lights
	parameterToType.insert(MP("scale", { "spectrum", "rgb" }));
	parameterToType.insert(MP("L", { "spectrum", "rgb", "blackbody" }));
	parameterToType.insert(MP("mapname", { "string" }));
	parameterToType.insert(MP("I", { "spectrum" }));
	parameterToType.insert(MP("from", { "point3" }));
	parameterToType.insert(MP("twosided", { "bool" }));
	// materials
	parameterToType.insert(MP("Kd", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Ks", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Kr", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("reflect", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Kt", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("transmit", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("roughness", { "float", "texture" }));
	parameterToType.insert(MP("eta", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("index", { "float" }));
	parameterToType.insert(MP("amount", { "float", "rgb" }));
	parameterToType.insert(MP("namedmaterial1", { "string" }));
	parameterToType.insert(MP("namedmaterial2", { "string" }));
	parameterToType.insert(MP("bumpmap", { "texture" }));
	// textures
	parameterToType.insert(MP("filename", { "string" }));
	parameterToType.insert(MP("value", { "float", "spectrum", "rgb" }));
	parameterToType.insert(MP("uscale", { "float" }));
	parameterToType.insert(MP("vscale", { "float" }));
	parameterToType.insert(MP("tex1", { "texture", "float", "spectrum", "rgb" }));
	parameterToType.insert(MP("tex2", { "texture", "float", "spectrum", "rgb" }));
}

// =====================================================================================
//                        PBRTParser IMPLEMENTATION
// =====================================================================================

//
// parse
//
ygl::scene *PBRTParser::parse() {
	ygl::mat4f CTM = ygl::identity_mat4f;
	this->advance();
	this->execute_preworld_directives();
	this->execute_world_directives();
	return scn;
}

//
// check_param_type
// returns true if the check goes well, false if the parameter is unknown,
// throws an exception if the type differs from the expected one.
//
bool PBRTParser::check_param_type(std::string par, std::string parsedType) {
	auto p = parameterToType.find(par);
	if (p == parameterToType.end()) {
		return false;
	}
	auto v = p->second;
	if (std::find(v.begin(), v.end(), parsedType) == v.end()) {
		// build expected type string
		std::stringstream exp;
		for (auto x : v)
			exp << x << "/";
		std::string expstr = exp.str();
		expstr = expstr.substr(0, expstr.length() - 1);
		throw_syntax_error("Parameter '" + par + "' expects a " + expstr + " type. ");
	}
	return true;
}

//
// advance
// Fetches the next lexeme (token)
//
void PBRTParser::advance() {
	try {
		this->lexers.at(0)->next_lexeme();
	}
	catch (InputEndedException ex) {
		this->lexers.erase(this->lexers.begin());
		if (lexers.size() == 0) {
			throw InputEndedException();
		}
		// after being restored, we still need to flush away the included filename.
		// See execute_Include() for more info.
		this->advance();
	}
};

//
// Remove all the tokens till the next directive
//
void PBRTParser::ignore_current_directive() {
	this->advance();
	while (this->current_token().type != LexemeType::IDENTIFIER)
		this->advance();
}

//
// execute_preworld_directives
//
void PBRTParser::execute_preworld_directives() {
    // When this method starts executing, the first token must be an Identifier of
	// a directive.

	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value =="WorldBegin")) {

		if (this->current_token().type != LexemeType::IDENTIFIER)
			throw_syntax_error("Identifier expected, got " + this->current_token().value + " instead.");

		// Scene-Wide rendering options
		else if (this->current_token().value =="Camera") {
			this->execute_Camera();
		}
		else if (this->current_token().value =="Film"){
			this->execute_Film();
		}

		else if (this->current_token().value =="Include") {
			this->execute_Include();
		}
		else if (this->current_token().value =="Translate") {
			this->execute_Translate();
		}
		else if (this->current_token().value =="Transform") {
			this->execute_Transform();
		}
		else if (this->current_token().value =="ConcatTransform") {
			this->execute_ConcatTransform();
		}
		else if (this->current_token().value =="Scale") {
			this->execute_Scale();
		}
		else if (this->current_token().value =="Rotate") {
			this->execute_Rotate();
		}
		else if (this->current_token().value =="LookAt"){
			this->execute_LookAt();
		}
		else {
			warning_message("Ignoring " + this->current_token().value + " directive..");
			this->ignore_current_directive();
		}
	}
}

//
// execute_world_directives
//
void PBRTParser::execute_world_directives() {
	this->gState.CTM = ygl::identity_mat4f;
	this->advance();
	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value =="WorldEnd")) {
		this->execute_world_directive();
	}
}

//
// execute_world_directive
// NOTE: This has been separated from execute_world_directives because it will be called
// also inside ObjectBlock.
//
void PBRTParser::execute_world_directive() {

	if (this->current_token().type != LexemeType::IDENTIFIER)
		throw_syntax_error("Identifier expected, got " + this->current_token().value + " instead.");

	if (this->current_token().value =="Include") {
		this->execute_Include();
	}
	else if (this->current_token().value =="Translate") {
		this->execute_Translate();
	}
	else if (this->current_token().value =="Transform") {
		this->execute_Transform();
	}
	else if (this->current_token().value =="ConcatTransform") {
		this->execute_ConcatTransform();
	}
	else if (this->current_token().value =="Scale") {
		this->execute_Scale();
	}
	else if (this->current_token().value =="Rotate") {
		this->execute_Rotate();
	}
	else if (this->current_token().value =="LookAt") {
		this->execute_LookAt();
	}
	else if (this->current_token().value =="AttributeBegin") {
		this->execute_AttributeBegin();
	}
	else if (this->current_token().value =="TransformBegin") {
		this->execute_TransformBegin();
	}
	else if (this->current_token().value =="AttributeEnd") {
		this->execute_AttributeEnd();
	}
	else if (this->current_token().value =="TransformEnd") {
		this->execute_TransformEnd();
	}
	else if (this->current_token().value =="Shape") {
		this->execute_Shape();
	}
	else if (this->current_token().value =="ObjectBegin") {
		this->execute_ObjectBlock();
	}
	else if (this->current_token().value =="ObjectInstance") {
		this->execute_ObjectInstance();
	}
	else if (this->current_token().value =="LightSource") {
		this->execute_LightSource();
	}
	else if (this->current_token().value =="AreaLightSource") {
		this->execute_AreaLightSource();
	}
	else if (this->current_token().value =="Material") {
		this->execute_Material();
	}
	else if (this->current_token().value =="MakeNamedMaterial") {
		this->execute_MakeNamedMaterial();
	}
	else if (this->current_token().value =="NamedMaterial") {
		this->execute_NamedMaterial();
	}
	else if (this->current_token().value =="Texture") {
		this->execute_Texture();
	}
	else {
		warning_message("Ignoring " + this->current_token().value + " directive..");
		this->ignore_current_directive();
	}
}

//
// get_unique_id
//
std::string PBRTParser::get_unique_id(CounterID id) {
	char buff[300];
	unsigned int val;
	char *st = "";

	if (id == CounterID::shape)
		st = "s_", val = shapeCounter++;
	else if (id == CounterID::shape_group)
		st = "sg_", val = shapeGroupCounter++;
	else if (id == CounterID::instance)
		st = "i_", val = instanceCounter++;
	else if (id == CounterID::material)
		st = "m_", val = materialCounter++;
	else if (id == CounterID::environment)
		st = "e_", val = envCounter++;
	else
		st = "t_", val = textureCounter++;

	sprintf(buff, "%s%u", st, val);
	return std::string(buff);
};

// ----------------------------------------------------------------------------
//                      PARAMETERS PARSING
// ----------------------------------------------------------------------------


//
// parse_parameter
// Fills the PBRTParameter structure with type name and value of the
// parameter. It is upon to the caller to convert "void * value" to
// the correct pointer type (by reading the name and type).
//
void PBRTParser::parse_parameter(PBRTParameter &par){

	auto valueToString = [](std::string x)->std::string {return x; };
	auto valueToFloat = [](std::string x)->float {return atof(x.c_str()); };
	auto valueToInt = [](std::string x)->int {return atoi(x.c_str()); };

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected a string with type and name of a parameter.");
    
	auto tokens = split(this->current_token().value);
    
	par.type = check_synonyms(std::string(tokens[0]));
	par.name = tokens[1];

	check_param_type(par.name, par.type);
	this->advance();

	// now, according to type, we parse the value
	if (par.type == "string" || par.type == "texture") {
		std::vector<std::string> *vals = new std::vector<std::string>();
		this->parse_value<std::string, LexemeType::STRING>(vals, valueToString);
		par.value = (void *)vals;
	}

	else if (par.type == "float") {
		std::vector<float> *vals = new std::vector<float>();
		this->parse_value<float, LexemeType::NUMBER>(vals, valueToFloat);
		par.value = (void *)vals;
	}

	else if (par.type == "integer") {
		std::vector<int> *vals = new std::vector<int>();
		this->parse_value<int, LexemeType::NUMBER>(vals, valueToInt);
		par.value = (void *)vals;
	}
	else if (par.type == "bool") {
		std::vector<std::string> *vals = new std::vector<std::string>();
		this->parse_value<std::string, LexemeType::STRING>(vals, valueToString);
		for (auto v : *vals) {
			if (v != "false" && v != "true")
				throw_syntax_error("A value diffeent from true and false "\
					"has been given to a bool type parameter.");
		}
		par.value = (void *)vals;
	}
	// now we come at a special case of arrays of vec3f
	else if (par.type == "point3" || par.type == "normal3"|| par.type == "rgb") {
		std::vector<float> *vals = new::std::vector<float>();
		this->parse_value<float, LexemeType::NUMBER>(vals, valueToFloat);
		if (vals->size() % 3 != 0)
			throw_syntax_error("Wrong number of values given.");
		std::vector<ygl::vec3f> *vectors = new std::vector<ygl::vec3f>();
		int count = 0;
		while (count < vals->size()) {
			ygl::vec3f v;
			for (int i = 0; i < 3; i++, count++) {
				v[i] = vals->at(count);
			}
			vectors->push_back(v);
		}
		delete vals;
		par.value = (void *)vectors;
	}
	else if (par.type == "spectrum") {
		// spectrum data can be given using a file or directly as list
		std::vector<ygl::vec2f> samples;
		if (this->current_token().type == LexemeType::STRING) {
			// filename given
			std::string fname = this->current_path() + "/" + this->current_token().value;
			this->advance();
			if (!load_spectrum_from_file(fname, samples))
				throw_syntax_error("Error loading spectrum data from file.");
		}else {
			// step 1: read raw data
			std::unique_ptr<std::vector<float>> vals(new::std::vector<float>());
			this->parse_value<float, LexemeType::NUMBER>(vals.get(), valueToFloat);
			// step 2: pack it in list of vec2f (lambda, val)
			if (vals->size() % 2 != 0)
				throw_syntax_error("Wrong number of values given.");
			int count = 0;
			while (count < vals->size()) {
				auto lamb = vals->at(count++);
				auto v = vals->at(count++);
				samples.push_back({ lamb, v });
			}
		}
		// step 3: convert to rgb and store in a vector (because it simplifies interface to get data)
		std::vector<ygl::vec3f> *data = new std::vector<ygl::vec3f>();
		data->push_back(spectrum_to_rgb(samples));
		par.value = (void *)data;
		par.type = std::string("rgb");
	}
	else if (par.type == "blackbody") {
		// step 1: read raw data
		std::unique_ptr<std::vector<float>> vals(new::std::vector<float>());
		this->parse_value<float, LexemeType::NUMBER>(vals.get(), valueToFloat);
		// step 2: pack it in list of vec2f (lambda, val)
		if (vals.get()->size() != 2) // NOTE: actually must be % 2 != 0
			throw_syntax_error("Wrong number of values given.");
		
		// step 3: convert to rgb and store in a vector (because it simplifies interface to get data)
		std::vector<ygl::vec3f> *data = new std::vector<ygl::vec3f>();
		data->push_back(blackbody_to_rgb(vals.get()->at(0), vals.get()->at(1)));
		par.value = (void *)data;
		par.type = std::string("rgb");
	}
	else {
		throw_syntax_error("Cannot able to parse the value: type '" + par.type + "' not supported.");
	}
}

//
// parse_parameters
//
void PBRTParser::parse_parameters(std::vector<PBRTParameter> &pars) {
	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);
		pars.push_back(par);
	}
}

// ------------------ END PARAMETER PARSING --------------------------------

//
// execute_Include
//
void PBRTParser::execute_Include() {
	this->advance();
	// now read the file to include
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected the name of the file to be included.");

	std::string fileToBeIncl = this->current_token().value;

	// call advance here is dangerous. It could end the parsing too soon.
	// better call it in advance() method, directly on the lexter after being
	// restored.

	if (fileToBeIncl.length() == 0)
		throw_syntax_error("Empty filename.");

	//distinguish if it is relative or absolute path
	std::stringstream ss;
	for (char ch : fileToBeIncl) {
		if (ch == '\\')
			ss << '/';
		else
			ss << ch;
	}
	fileToBeIncl = ss.str();

	if (fileToBeIncl[0] == '/' || (fileToBeIncl.length() > 3 && fileToBeIncl[1] == ':' && fileToBeIncl[2] == '/')) {
		// absolute path
		this->lexers.insert(this->lexers.begin(), new PBRTLexer(fileToBeIncl));
	}
	else {
		// relative path
		std::stringstream builtPath;
		builtPath << this->current_path() << "/" << fileToBeIncl;
		this->lexers.insert(this->lexers.begin(), new PBRTLexer(builtPath.str()));
	}
	this->advance();
}

//
// find_param
// search for a parameter by name in a vector of parsed parameters.
// Returns the index of the searched parameter in the vector if found, -1 otherwise.
//
int PBRTParser::find_param(std::string name, std::vector<PBRTParameter> &vec) {
	int count = 0;
	for (PBRTParameter P : vec) {
		if (P.name == name)
			return count;
		else
			count++;
	}
	return -1;
}

// ------------------------------------------------------------------------------------
//                               TRANSFORMATIONS
// ------------------------------------------------------------------------------------

//
// execute_Translate()
//
void PBRTParser::execute_Translate() {
	this->advance();
	ygl::vec3f transl_vec{ };

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_error("Expected a float value.");
		transl_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	
	auto transl_mat = ygl::frame_to_mat(ygl::translation_frame(transl_vec));
	this->gState.CTM = this->gState.CTM * transl_mat;
}

//
// execute_Scale()
//
void PBRTParser::execute_Scale(){
	this->advance();
	ygl::vec3f scale_vec;

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_error("Expected a float value.");
		scale_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	
	auto scale_mat = ygl::frame_to_mat(ygl::scaling_frame(scale_vec));
	this->gState.CTM =  this->gState.CTM * scale_mat;
}

//
// execute_Rotate()
//
void PBRTParser::execute_Rotate() {
	this->advance();
	float angle;
	ygl::vec3f rot_vec;

	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'angle' parameter of Rotate directive.");
	angle = atof(this->current_token().value.c_str());
	angle = angle * (ygl::pif / 180.0f);
	
	this->advance();

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_error("Expected a float value.");
		rot_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	auto rot_mat = ygl::frame_to_mat(ygl::rotation_frame(rot_vec, angle));
	this->gState.CTM = this->gState.CTM * rot_mat;
}

//
// execute_LookAt()
//
void PBRTParser::execute_LookAt(){
	this->advance();
	std::vector<ygl::vec3f> vects(3); //eye, look and up
	
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (this->current_token().type != LexemeType::NUMBER)
				throw_syntax_error("Expected a float value.");
			vects[i][j] = atof(this->current_token().value.c_str());
			this->advance();
		}
	}

	auto fm = ygl::lookat_frame(vects[0], vects[1], vects[2]);
	fm.x = -fm.x;
	fm.z = -fm.z;
	auto mm = ygl::frame_to_mat(fm);
	this->defaultFocus = ygl::length(vects[0] - vects[1]);
	this->gState.CTM = this->gState.CTM * ygl::inverse(mm); // inverse here because pbrt boh
}

//
// execute_Transform()
//
void PBRTParser::execute_Transform() {
	this->advance();
	std::vector<float> vals;
	this->parse_value<float, LexemeType::NUMBER>(&vals, [](std::string x)->float {return atof(x.c_str()); });
	ygl::mat4f nCTM;
	if (vals.size() != 16)
		throw_syntax_error("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals.at(i * 4 + j);
		}
	}
	gState.CTM = nCTM;
}

//
// execute_ConcatTransform()
//
void PBRTParser::execute_ConcatTransform() {
	this->advance();
	std::vector<float> vals;
	this->parse_value<float, LexemeType::NUMBER>(&vals, [](std::string x)->float {return atof(x.c_str()); });
	ygl::mat4f nCTM;
	if (vals.size() != 16)
		throw_syntax_error("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals.at(i * 4 + j);
		}
	}
	gState.CTM = gState.CTM * nCTM;
}

// --------------------------------------------------------------------------
//                  SCENE-WIDE RENDERING OPTIONS
// --------------------------------------------------------------------------

//
// execute_Camera
// Parse camera information.
// NOTE: only support perspective camera for now.
//
void PBRTParser::execute_Camera() {
	this->advance();

	ygl::camera *cam = new ygl::camera;
	cam->aspect = defaultAspect;
	cam->aperture = 0;
	cam->yfov = 90.0f * ygl::pif / 180;
	cam->focus = defaultFocus;
	char buff[100];
	sprintf(buff, "c%lu", scn->cameras.size());
	cam->name = std::string(buff);
	
	// CTM defines world to camera transformation
	cam->frame = ygl::mat_to_frame(ygl::inverse(this->gState.CTM));
	cam->frame.z = -cam->frame.z;
	
	// Parse the camera parameters
	// First parameter is the type
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string camType = this->current_token().value;
	// RESTRICTION: only perspective camera is supported
	if (camType != "perspective")
		throw_syntax_error("Only perspective camera type is supported.");
	this->advance();

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);

	int i_frameaspect = find_param("frameaspectratio", params);
	if (i_frameaspect >= 0)
		cam->aspect = get_single_value<float>(params[i_frameaspect]);
	
	int i_fov = find_param("fov", params);
	if (i_fov >= 0) {
		cam->yfov = get_single_value<float>(params[i_fov])*ygl::pif / 180;
	}
		
	scn->cameras.push_back(cam);
}

//
// execute_Film
//
void PBRTParser::execute_Film() {
	this->advance();

	// First parameter is the type
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string filmType = this->current_token().value;
	
	if (filmType != "image")
		throw_syntax_error("Only image \"film\" is supported.");

	this->advance();
	int xres = 0;
	int yres = 0;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);

	int i_xres = find_param("xresolution", params);
	if (i_xres >= 0)
		xres = get_single_value<int>(params[i_xres]);

	int i_yres = find_param("yresolution", params);
	if (i_yres >= 0)
		yres = get_single_value<int>(params[i_yres]);

	if (xres && yres) {
		auto asp = ((float)xres) / ((float)yres);
		if (asp < 1)
			asp = 1; // TODO: vertical images
		this->defaultAspect = asp;

		for (auto cam : scn->cameras)
			cam->aspect = this->defaultAspect;
	}
}

// -----------------------------------------------------------------------------
//                         DESCRIBING THE SCENE
// -----------------------------------------------------------------------------

//
// execute_AttributeBegin
// Enter into a inner graphics scope
//
void PBRTParser::execute_AttributeBegin() {
	this->advance();
	// save the current state
	stateStack.push_back(this->gState);
}

//
// execute_AttributeEnd
//
void PBRTParser::execute_AttributeEnd() {
	this->advance();
	if (stateStack.size() == 0) {
		throw_syntax_error("AttributeEnd instruction unmatched with AttributeBegin.");
	}
	this->gState = stateStack.back();
	stateStack.pop_back();
}

//
// execute_TransformBegin
//
void PBRTParser::execute_TransformBegin() {
	this->advance();
	// save the current CTM
	CTMStack.push_back(this->gState.CTM);
}

//
// execute_TransformEnd
//
void PBRTParser::execute_TransformEnd() {
	this->advance();
	// save the current CTM
	if (CTMStack.size() == 0) {
		throw_syntax_error("TranformEnd instruction unmatched with TransformBegin.");
	}
	this->gState.CTM = CTMStack.back();
	CTMStack.pop_back();
}

// ---------------------------------------------------------------------------------
//                                  SHAPES
// ---------------------------------------------------------------------------------

//
// parse_cube
// DEBUG FUNCTION
//
void PBRTParser::parse_cube(ygl::shape *shp) {
	// DEBUG: this is a debug function
	while (this->current_token().type != LexemeType::IDENTIFIER)
		this->advance();
	ygl::make_uvcube(shp->quads, shp->pos, shp->norm, shp->texcoord, 1);
}

//
// parse_curve
// TODO: implement this
//
void PBRTParser::parse_curve(ygl::shape *shp) {
	throw_syntax_error("curves are not supported for now.");

	std::vector<ygl::vec3f> *p = nullptr; // max 4 points
	int degree = 3; // only legal options 2 and 3
	std::string type = "";
	std::vector<ygl::vec3f> *N = nullptr;
	int splitDepth = 3;
	int width = 1;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);
	
	int i_p = find_param("p", params);
	if (i_p >= 0)
		p = (std::vector<ygl::vec3f> *) params[i_p].value;

	int i_N = find_param("N", params);
	if (i_N >=0)
		N = (std::vector<ygl::vec3f> *) params[i_N].value;

	int i_deg = find_param("degree", params);
	if (i_deg >= 0)
		degree = get_single_value<int>(params[i_deg]);

	int i_type = find_param("type", params);
	if (i_type >= 0)
		type = get_single_value<std::string>(params[i_type]);

	int i_spd = find_param("splitdepth", params);
	if (i_spd >= 0)
		splitDepth = get_single_value<int>(params[i_spd]);

	int i_w = find_param("width", params);
	if (i_w >= 0)
		width = get_single_value<float>(params[i_w]);
	
	//TODO: do something
}

//
// my_compute_normals
// because pbrt computes it differently
// TODO: must be removed in future.
//
void my_compute_normals(const std::vector<ygl::vec3i>& triangles,
	const std::vector<ygl::vec3f>& pos, std::vector<ygl::vec3f>& norm, bool weighted ) {
	norm.resize(pos.size());
	for (auto& n : norm) n = ygl::zero3f;
	for (auto& t : triangles) {
		auto n = cross(pos[t.y] - pos[t.z], pos[t.x] - pos[t.z]); // it is different here
		if (!weighted) n = normalize(n);
		for (auto vid : t) norm[vid] += n;
	}
	for (auto& n : norm) n = normalize(n);
}

//
// parse_triangle_mesh
//
void PBRTParser::parse_trianglemesh(ygl::shape *shp) {

	bool indicesCheck = false;
	bool PCheck = false;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);
	// vertices
	int i_p = find_param("P", params);
	if (i_p >= 0) {
		auto data = (std::vector<ygl::vec3f> *) params[i_p].value;
		for (auto p : *data) {
			shp->pos.push_back(p);
		}
		delete data;
		PCheck = true;

	}
	// normals
	int i_N = find_param("N", params);
	if (i_N >= 0) {
		auto data = (std::vector<ygl::vec3f> *) params[i_N].value;
		for (auto p : *data) {
			shp->norm.push_back(p);
		}
		delete data;
	}
	// indices
	int i_indices = find_param("indices", params);
	if (i_indices >= 0) {
		std::vector<int> *data = (std::vector<int> *) params[i_indices].value;
		if (data->size() % 3 != 0)
			throw_syntax_error("The number of triangle vertices must be multiple of 3.");

		int count = 0;
		while (count < data->size()) {
			ygl::vec3i triangle;
			triangle[0] = data->at(count++);
			triangle[1] = data->at(count++);
			triangle[2] = data->at(count++);
			shp->triangles.push_back(triangle);
		}
		indicesCheck = true;
		delete data;
	}

	int i_uv = find_param("uv", params);
	if (i_uv == -1)
		i_uv = find_param("st", params);
	if(i_uv >= 0){
		std::vector<float> *data = (std::vector<float> *)params[i_uv].value;
		int j = 0;
		while (j < data->size()) {
			ygl::vec2f uv;
			uv[0] = data->at(j++);
			uv[1] = data->at(j++);
			shp->texcoord.push_back(uv);
		}
		delete data;
	}
	
	// TODO: materials parameters overriding
	// Single material parameters (e.g. Kd, Ks) can be directly specified on a shape
	// overriding (for this shape) the value of the current material in the graphical state.

	// TODO: solve problem with normals
	if (shp->norm.size() == 0);
		my_compute_normals(shp->triangles, shp->pos, shp->norm, true);

	if (!(indicesCheck && PCheck))
		throw_syntax_error("Missing indices or positions in triangle mesh specification.");
}

//
// execute_Shape
//
void PBRTParser::execute_Shape() {
	this->advance();

	// parse the shape name
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected shape name.");
	std::string shapeName = this->current_token().value;
	this->advance();
	
	ygl::shape *shp = new ygl::shape();
	shp->name = get_unique_id(CounterID::shape);
	// add material to shape
	if (!gState.mat) {
		// since no material was defined, empty material is created
		std::cout << "Empty material created..\n";
		ygl::material *nM = new ygl::material();
		shp->mat = nM;
		scn->materials.push_back(nM);
	}
	else {
		shp->mat = gState.mat;
	}
	// TODO: handle when shapes override some material properties

	if (this->gState.ALInfo.active) {
		shp->mat->ke = gState.ALInfo.L;
		shp->mat->double_sided = gState.ALInfo.twosided;
	}

	if (shapeName == "curve") //TODO needs to be implemented
		this->parse_curve(shp);
	
	else if (shapeName == "trianglemesh")
		this->parse_trianglemesh(shp);

	else if (shapeName == "cube")
		this->parse_cube(shp);

	else if (shapeName == "plymesh"){
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name != "filename")
			throw_syntax_error("Expected ply file path.");

		std::string fname = this->current_path() + "/" + get_single_value<std::string>(par);

		if (!parse_ply(fname, shp)){
			throw_syntax_error("Error parsing ply file: " + fname);
		}

		while (this->current_token().type != LexemeType::IDENTIFIER)
			this->advance();
	
	}
	else {
		this->ignore_current_directive();
		warning_message("Ignoring shape " + shapeName + ".");
		return;
	}

	// handle texture coordinate scaling
	for (int i = 0; i < shp->texcoord.size(); i++) {
		shp->texcoord[i].x *= gState.uscale;
		shp->texcoord[i].y *= gState.vscale;
	}

	if (this->inObjectDefinition) {
		shapesInObject->shapes.push_back(shp);
	}
	else {
		// add shp in scene
		ygl::shape_group *sg = new ygl::shape_group;
		sg->shapes.push_back(shp);
		sg->name = get_unique_id(CounterID::shape_group);
		scn->shapes.push_back(sg);
		// add a single instance directly to the scene
		ygl::instance *inst = new ygl::instance();
		inst->shp = sg;
		// TODO: check the correctness of this
		// NOTE: current transformation matrix is used to set the object to world transformation for the shape.
		inst->frame = ygl::mat_to_frame(this->gState.CTM);
		inst->name = get_unique_id(CounterID::instance);
		scn->instances.push_back(inst);
	}
}

// ------------------- END SHAPES --------------------------------------------------

//
// execute_ObjectBlock
//
void PBRTParser::execute_ObjectBlock() {
	if (this->inObjectDefinition)
		throw_syntax_error("Cannot define an object inside another object.");
	this->execute_AttributeBegin(); // it will execute advance() too
	this->inObjectDefinition = true;
	this->shapesInObject = new ygl::shape_group();
	this->shapesInObject->name = get_unique_id(CounterID::shape_group);
	int start = this->lexers[0]->get_line();

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//object space to instance's coordinate space
	std::string objName = this->current_token().value;
	this->advance();

	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value == "ObjectEnd")){
		this->execute_world_directive();
	}

	auto it = nameToObject.find(objName);
	if (it == nameToObject.end()){
		nameToObject.insert(std::make_pair(objName, DeclaredObject(shapesInObject, this->gState.CTM)));
	}		
	else {
		auto prev = it->second;
		if (prev.referenced == false) {
			delete prev.sg;
		}
		it->second = DeclaredObject(this->shapesInObject, this->gState.CTM);
		std::cout << "Object defined at line " << start << " overrides an existent one.\n";
	}
		
	this->inObjectDefinition = false;
	this->execute_AttributeEnd();
}

//
// execute_ObjectInstance
//
void PBRTParser::execute_ObjectInstance() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//instance space to world coordinate space
	std::string objName = this->current_token().value;
	this->advance();

	auto obj = nameToObject.find(objName);
	if (obj == nameToObject.end())
		throw_syntax_error("Object name not found.");

	auto shapes = obj->second.sg;
	if (shapes->shapes.size() > 0) {
		ygl::mat4f finalCTM = this->gState.CTM * obj->second.CTM;
		if (!obj->second.referenced) {
			obj->second.referenced = true;
			scn->shapes.push_back(shapes);
		}
		ygl::instance *inst = new ygl::instance();
		inst->shp = shapes;
		inst->frame = ygl::mat_to_frame(finalCTM);
		inst->name = get_unique_id(CounterID::instance);
		scn->instances.push_back(inst);
	}
}

// --------------------------------------------------------------------------
//                        LIGHTS
// --------------------------------------------------------------------------

//
// execute_LightSource
//
void PBRTParser::execute_LightSource() {
	// TODO: check how scale exactly is used
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");
	// NOTE: when a light source is defined, the CTM is uded to define
	// the light-to-world transformation. 
	std::string lightType = this->current_token().value;
	this->advance();

	if (lightType == "point")
		this->parse_PointLight();
	else if (lightType == "infinite")
		this->parse_InfiniteLight();
	else if (lightType == "distant"){
		// TODO: implement real distant light
		this->parse_InfiniteLight();
	}else
		throw_syntax_error("Light type " + lightType + " not supported.");
}

//
// parse_InfiniteLight
//
void PBRTParser::parse_InfiniteLight() {
	// TODO: conversion from spectrum to rgb
	ygl::vec3f scale { 1, 1, 1 };
	ygl::vec3f L { 1, 1, 1 };
	std::string mapname;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);

	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = get_single_value<ygl::vec3f>(params[i_scale]);
	}
	int i_L = find_param("L", params);
	if (i_L >= 0) {
		L = get_single_value<ygl::vec3f>(params[i_L]);
	}
	int i_map = find_param("mapname", params);
	if (i_map >= 0) {
		mapname = get_single_value<std::string>(params[i_map]);
	}
	
	ygl::environment *env = new ygl::environment;
	env->name = get_unique_id(CounterID::environment);
	env->ke = scale * L;
	auto fm = ygl::mat_to_frame(gState.CTM);
	env->frame = fm; 
	if (mapname.length() > 0) {
		ygl::texture *txt = new ygl::texture;
		txt->name = get_unique_id(CounterID::texture);
		load_texture(txt, mapname, false);
		scn->textures.push_back(txt);
		env->ke_txt_info = new ygl::texture_info();
		env->ke_txt = txt;
	}
	scn->environments.push_back(env);
}

//
// parse_PointLight
//
void PBRTParser::parse_PointLight() {
	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f I{ 1, 1, 1 };
	ygl::vec3f point;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);

	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = get_single_value<ygl::vec3f>(params[i_scale]);
	}
	int i_I = find_param("I", params);
	if (i_I >= 0) {
		I = get_single_value<ygl::vec3f>(params[i_I]); //TODO: actually spectrum is different from vec3f
	}
	int i_from = find_param("from", params);
	if (i_from >= 0) {
		point = get_single_value<ygl::vec3f>(params[i_from]);
	}

	ygl::shape_group *sg = new ygl::shape_group;
	sg->name = get_unique_id(CounterID::shape_group);

	ygl::shape *lgtShape = new ygl::shape;
	lgtShape->name = get_unique_id(CounterID::shape);
	lgtShape->pos.push_back(point);
	lgtShape->points.push_back(0);
	// default radius
	lgtShape->radius.push_back(1.0f);

	sg->shapes.push_back(lgtShape);

	ygl::material *lgtMat = new ygl::material;
	lgtMat->ke = I * scale;
	lgtShape->mat = lgtMat;
	lgtMat->name = get_unique_id(CounterID::material);
	scn->materials.push_back(lgtMat);

	scn->shapes.push_back(sg);
	ygl::instance *inst = new ygl::instance;
	inst->shp = sg;

	inst->frame = ygl::mat_to_frame(gState.CTM);
	inst->name = get_unique_id(CounterID::instance);
	scn->instances.push_back(inst);
}

//
// execute_AreaLightSource
//
void PBRTParser::execute_AreaLightSource() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");
	
	std::string areaLightType = this->current_token().value;
	// TODO: check type
	this->advance();

	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f L{ 1, 1, 1 };
	bool twosided = false;

	std::vector<PBRTParameter> params;
	this->parse_parameters(params);
	
	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = get_single_value<ygl::vec3f>(params[i_scale]);
	}
	int i_L = find_param("L", params);
	if (i_L >= 0) {
		L = get_single_value<ygl::vec3f>(params[i_L]);
	}
	
	int i_ts = find_param("twosided", params);
	if (i_ts >= 0) {
		twosided = get_single_value<std::string>(params[i_ts]) == "true" ? true : false;
	}
	
	this->gState.ALInfo.active = true;
	this->gState.ALInfo.L = L;
	this->gState.ALInfo.twosided = twosided;
}
// --------------------------- END LIGHTS ---------------------------------------

// ------------------------------------------------------------------------------
//                             MATERIALS
// ------------------------------------------------------------------------------

//
// set_k_property
// Convenience function to set kd, ks, kt, kr from parsed parameter
//
void PBRTParser::set_k_property(PBRTParameter &par, ygl::vec3f &k, ygl::texture **txt) {

	if (par.type == "texture") {
		auto txtName = get_single_value<std::string>(par);
		auto it = gState.nameToTexture.find(txtName);
		if (it == gState.nameToTexture.end())
			throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
				+ par.name + "' was not found.");
		auto declTexture = it->second;

		if (!it->second.referenced) {
			it->second.referenced = true;
			scn->textures.push_back(it->second.txt);
		}

		*txt = declTexture.txt;
		gState.uscale = declTexture.uscale;
		gState.vscale = declTexture.vscale;
		k = { 1, 1, 1 };
	}
	else {
		k = get_single_value<ygl::vec3f>(par);
	}
}

//
// execute_Material
//
void PBRTParser::execute_Material() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");

	std::string materialType = this->current_token().value;
	this->advance();

	// NOTE: shapes can override material parameters
	ygl::material *newMat = new ygl::material();
	
	newMat->name = get_unique_id(CounterID::material);
	newMat->type = ygl::material_type::specular_roughness;
		
	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);

	// bump is common to every material
	int i_bump = find_param("bump", params);
	if (i_bump >= 0) {
		auto txtName = get_single_value<std::string>(params[i_bump]);
		auto it = gState.nameToTexture.find(txtName);
		if (it == gState.nameToTexture.end())
			throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
				+ params[i_bump].name + "' was not found.");

		auto declTexture = it->second;
		if (!it->second.referenced) {
			it->second.referenced = true;
			scn->textures.push_back(it->second.txt);
		}
		newMat->bump_txt = declTexture.txt;
		gState.uscale = declTexture.uscale;
		gState.vscale = declTexture.vscale;
	}

	if (materialType == "matte")
		this->parse_material_matte(newMat, params);
	else if (materialType == "metal")
		this->parse_material_metal(newMat, params);
	else if (materialType == "mix")
		this->parse_material_mix(newMat, params);
	else if (materialType == "plastic")
		this->parse_material_plastic(newMat, params);
	else if (materialType == "mirror")
		this->parse_material_mirror(newMat, params);
	else if (materialType == "uber")
		this->parse_material_uber(newMat, params);
	else if (materialType == "translucent")
		this->parse_material_translucent(newMat, params);
	else if (materialType == "glass")
		this->parse_material_glass(newMat, params);
	else {
		std::cerr << "Material '" + materialType + "' not supported. Ignoring and using 'matte'..\n";
		this->parse_material_matte(newMat, params);
	}
	this->gState.mat = newMat;
	scn->materials.push_back(this->gState.mat);
		
}

//
// parse_material_matte
//
void PBRTParser::parse_material_matte(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->kd = { 0.5f, 0.5f, 0.5f };
	mat->rs = 1;
	
	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], mat->kd, &(mat->kd_txt));
	}
}

//
// parse_material_uber
//
void PBRTParser::parse_material_uber(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->kd = { 0.25f, 0.25f, 0.25f };
	mat->ks = { 0.25f, 0.25f, 0.25f };
	mat->kr = { 0, 0, 0 };
	mat->rs = 0.01f;

	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], mat->kd, &(mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], mat->ks, &(mat->ks_txt));
	}

	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], mat->kr, &(mat->kr_txt));
	}

	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs].type == "texture") {
			auto txtName = get_single_value < std::string>(params[i_rs]);
			auto it = gState.nameToTexture.find(txtName);
			if (it == gState.nameToTexture.end())
				throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
					+ params[i_rs].name + "' was not found.");
			if (!it->second.referenced) {
				it->second.referenced = true;
				scn->textures.push_back(it->second.txt);
			}
			mat->rs = 1;
			mat->rs_txt = it->second.txt;
		}
		else {
			mat->rs = get_single_value<float>(params[i_rs]);
		}
	}
	
}

//
// parse_material_translucent
//
void PBRTParser::parse_material_translucent(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->kd = { 0.25f, 0.25f, 0.25f };
	mat->ks = { 0.25f, 0.25f, 0.25f };
	mat->kr = { 0.5f, 0.5f, 0.5f };
	mat->kt = { 0.5f, 0.5f, 0.5f };
	mat->rs = 0.1f;
	
	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], mat->kr, &(mat->kr_txt));
	}

	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], mat->kd, &(mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], mat->ks, &(mat->ks_txt));
	}

	int i_kt = find_param("Kt", params);
	if (i_kt >= 0) {
		set_k_property(params[i_kt], mat->kt, &(mat->kt_txt));
	}

	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs].type == "texture") {
			auto txtName = get_single_value < std::string>(params[i_rs]);
			auto it = gState.nameToTexture.find(txtName);
			if (it == gState.nameToTexture.end())
				throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
					+ params[i_rs].name + "' was not found.");
			if (!it->second.referenced) {
				it->second.referenced = true;
				scn->textures.push_back(it->second.txt);
			}
			mat->rs = 1;
			mat->rs_txt = it->second.txt;
		}
		else {
			mat->rs = get_single_value<float>(params[i_rs]);
		}
	}

}

//
// parser_material_metal
//
void PBRTParser::parse_material_metal(ygl::material *mat, std::vector<PBRTParameter> &params) {
	ygl::vec3f eta{ 0.5, 0.5, 0.5 };
	ygl::texture *etaTexture = nullptr;
	ygl::vec3f k { 0.5, 0.5, 0.5 };
	ygl::texture *kTexture = nullptr;
	mat->rs = 0.01;

	int i_eta = find_param("eta", params);
	if (i_eta >= 0) {
		set_k_property(params[i_eta], eta, &(etaTexture));
	}
	int i_k = find_param("k", params);
	if (i_k >= 0) {
		set_k_property(params[i_k], k, &(kTexture));
	}
	
	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs].type == "texture") {
			auto txtName = get_single_value < std::string>(params[i_rs]);
			auto it = gState.nameToTexture.find(txtName);
			if (it == gState.nameToTexture.end())
				throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
					+ params[i_rs].name + "' was not found.");
			if (!it->second.referenced) {
				it->second.referenced = true;
				scn->textures.push_back(it->second.txt);
			}
			mat->rs = 1;
			mat->rs_txt = it->second.txt;
		}
		else {
			mat->rs = get_single_value<float>(params[i_rs]);
		}
	}
	mat->ks = ygl::fresnel_metal(1, eta, k);
}

//
// parse_material_mirror
//
void PBRTParser::parse_material_mirror(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->kr = { 0.9f, 0.9f, 0.9f };
	mat->rs = 0;
	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], mat->kr, &(mat->kr_txt));
	}
}

//
// parse_material_plastic
//
void PBRTParser::parse_material_plastic(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->kd = { 0.25, 0.25, 0.25 };
	mat->ks = { 0.25, 0.25, 0.25 };
	mat->rs = 0.1;
	
	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], mat->kd, &(mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], mat->ks, &(mat->ks_txt));
	}
}

//
// parse_material_pglass
//
void PBRTParser::parse_material_glass(ygl::material *mat, std::vector<PBRTParameter> &params) {
	mat->ks = { 0.04f, 0.04f, 0.04f };
	mat->kt = { 1, 1, 1 };
	mat->rs = 0.1;
	
	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], mat->ks, &(mat->ks_txt));
	}

	int i_kt = find_param("Kt", params);
	if (i_kt >= 0) {
		set_k_property(params[i_kt], mat->kt, &(mat->kt_txt));
	}
}

// --------------------------------------------------------------------------------
// The following functions are used to mix materials
// TODO: test them. Actually I should check if PBRT implements this in a similar way
// --------------------------------------------------------------------------------

//
// blend_textures
// Mix two textures
//
ygl::texture* PBRTParser::blend_textures(ygl::texture *txt1, ygl::texture *txt2, float amount) {

	auto ts1 = TextureSupport(txt1);
	auto ts2 = TextureSupport(txt2);
	ygl::texture *txt = nullptr;

	auto scale_single_texture = [&amount](TextureSupport &txtin)->ygl::texture* {
		ygl::texture *txt = new ygl::texture();
		txt->ldr = ygl::image4b(txtin.width, txtin.height);
			for (int w = 0; w < txtin.width; w++) {
				for (int h = 0; h < txtin.height; h++) {
					txt->ldr.at(w, h) = ygl::float_to_byte(txtin.at(w, h)*(1 - amount));
				}
			}
			return txt;
	};
	
	if (!txt1 && !txt2)
		return nullptr;
	else if (!txt1) {
		txt = scale_single_texture(ts2);
	}
	else if (!txt2) {
		txt = scale_single_texture(ts1);
	}
	else {
		txt = new ygl::texture();

		int width = ts1.width > ts2.width ? ts1.width : ts2.width;
		int height = ts1.height > ts2.height ? ts1.height : ts2.height;

		auto img = ygl::image4b(width, height);

		for (int w = 0; w < width; w++) {
			for (int h = 0; h < height; h++) {
				int w1 = w % ts1.width;
				int h1 = h % ts1.height;
				int w2 = w % ts2.width;
				int h2 = h % ts2.height;

				auto newPixel = ts1.at(w1, h1)* amount + ts2.at(w2, h2)* (1 - amount);
				img.at(w, h) = ygl::float_to_byte(newPixel);
			}
		}
		txt->ldr = img;
	}
	txt->name = get_unique_id(CounterID::texture);
	txt->path = txt->name + ".png";
	scn->textures.push_back(txt);
	return txt;
}

//
// parse_material_mix
//
void PBRTParser::parse_material_mix(ygl::material *mat, std::vector<PBRTParameter> &params) {
	
	float amount = 0.5f;
	std::string m1, m2;
	int i_am = find_param("amount", params);
	if (i_am >= 0) {
		amount = get_single_value<float>(params[i_am]);
	}
	int i_m1 = find_param("namedmaterial1", params);
	if (i_m1 >= 0) {
		m1 = get_single_value<std::string>(params[i_m1]);
	}
	else {
		throw_syntax_error("Missing namedmaterial1.");
	}
	int i_m2 = find_param("namedmaterial2", params);
	if (i_m2 >= 0) {
		m2 = get_single_value<std::string>(params[i_m2]);
	}
	else {
		throw_syntax_error("Missing namedmaterial2.");
	}
	auto i1 = gState.nameToMaterial.find(m1);
	if (i1 == gState.nameToMaterial.end())
		throw_syntax_error("NamedMaterial1 " + m1 + " was not defined.");
	auto mat1 = i1->second;

	auto i2 = gState.nameToMaterial.find(m2);
	if (i2 == gState.nameToMaterial.end())
		throw_syntax_error("NamedMaterial2 " + m2 + " was not defined.");
	auto mat2 = i2->second;

	// blend vectors
	mat->kd = (1 - amount)*mat2->kd + amount * mat1->kd;
	mat->kr = (1 - amount)*mat2->kr + amount * mat1->kr;
	mat->ks = (1 - amount)*mat2->ks + amount * mat1->ks;
	mat->kt = (1 - amount)*mat2->kt + amount * mat1->kt;
	mat->rs = (1 - amount)*mat2->rs + amount * mat1->rs;
	// blend textures
	mat->kd_txt = blend_textures(mat1->kd_txt, mat2->kd_txt, amount);
	mat->kr_txt = blend_textures(mat1->kr_txt, mat2->kr_txt, amount);
	mat->ks_txt = blend_textures(mat1->ks_txt, mat2->ks_txt, amount);
	mat->kt_txt = blend_textures(mat1->kt_txt, mat2->kt_txt, amount);
	mat->rs_txt = blend_textures(mat1->rs_txt, mat2->rs_txt, amount);
	mat->bump_txt = blend_textures(mat1->bump_txt, mat2->bump_txt, amount);
	mat->disp_txt = blend_textures(mat1->disp_txt, mat2->disp_txt, amount);
	mat->norm_txt = blend_textures(mat1->norm_txt, mat2->norm_txt, amount);

	mat->op = mat1->op * amount + mat2->op *(1 - amount);
}
// -------------- END MATERIAL MIX ----------------------------

//
// execute_MakeNamedMaterial
//
void PBRTParser::execute_MakeNamedMaterial() {
	this->advance();

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected material name as string.");

	std::string materialName = this->current_token().value;
	if (gState.nameToMaterial.find(materialName) != gState.nameToMaterial.end())
		throw_syntax_error("A material with the specified name already exists.");
	this->advance();


	ygl::material *mat = new ygl::material;
	mat->name = get_unique_id(CounterID::material);

	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);

	std::string mtype;
	int i_mtype = find_param("type", params);
	if (i_mtype < 0)
		throw_syntax_error("Expected type of named material.");
	else
		mtype = get_single_value<std::string>(params[i_mtype]);

	// bump is common to every material
	int i_bump = find_param("bump", params);
	if (i_bump >= 0) {
		auto txtName = get_single_value < std::string>(params[i_bump]);
		auto it = gState.nameToTexture.find(txtName);
		if (it == gState.nameToTexture.end())
			throw_syntax_error("the specified texture '" + txtName + "' for parameter '"\
				+ params[i_bump].name + "' was not found.");
		if (!it->second.referenced) {
			it->second.referenced = true;
			scn->textures.push_back(it->second.txt);
		}
		mat->bump_txt = it->second.txt;
		gState.uscale = it->second.uscale;
		gState.vscale = it->second.vscale;
	}

	if (mtype == "metal")
		this->parse_material_metal(mat, params);
	else if (mtype == "plastic")
		this->parse_material_plastic(mat, params);
	else if (mtype == "matte")
		this->parse_material_matte(mat, params);
	else if (mtype == "mirror")
		this->parse_material_mirror(mat, params);
	else if (mtype == "uber")
		this->parse_material_uber(mat, params);
	else if (mtype == "mix")
		this->parse_material_mix(mat, params);
	else if (mtype == "translucent")
		this->parse_material_translucent(mat, params);
	else if (mtype == "glass")
		this->parse_material_glass(mat, params);
	else
		throw_syntax_error("Material type " + mtype + " not supported or recognized.");
	gState.nameToMaterial.insert(std::make_pair(materialName, mat));
	scn->materials.push_back(mat);
}

//
// execute_NamedMaterial
//
void PBRTParser::execute_NamedMaterial() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected material name string.");
	std::string materialName = this->current_token().value;
	this->advance();
	auto it = gState.nameToMaterial.find(materialName);
	if (it == gState.nameToMaterial.end())
		throw_syntax_error("No material with the specified name.");
	auto mat = it->second;
	this->gState.mat = mat;
}

// -----------------------------------------------------------------------------
//                                TEXTURES
// -----------------------------------------------------------------------------

//
// make_constant_image
//
ygl::image4b PBRTParser::make_constant_image(float v) {
	auto  x = ygl::image4b(1, 1);
	auto b = ygl::float_to_byte(v);
	x.at(0, 0) = { b, b, b, 255 };
	return x;
}

//
// make_constant_image
//
ygl::image4b PBRTParser::make_constant_image(ygl::vec3f v) {
	auto  x = ygl::image4b(1, 1);
	x.at(0, 0) = ygl::float_to_byte({ v.x, v.y, v.z, 1 });
	return x;
}

//
// flip_image
// flip an image on the y axis.
//
template <typename T>
ygl::image<T> flip_image(ygl::image<T> in) {
	ygl::image<T> nI(in.width(), in.height());

	for (int j = 0; j < in.height(); j++) {
		for (int i = 0; i < in.width(); i++) {
			nI.at(i, j) = in.at(i, in.height() - j - 1);
		}
	}
	return nI;
}


//
// load_texture image from file
//
void PBRTParser::load_texture(ygl::texture *txt, std::string &filename, bool flip) {
	auto completePath = this->current_path() + "/" + filename;
	auto ext = ygl::path_extension(filename);
	auto name = ygl::path_basename(filename);
	ext = ext == ".exr" ? ".hdr" : ext;
	txt->path = name + ext;
	if (ext == ".hdr") {
		auto im = ygl::load_image4f(completePath);
		txt->hdr = flip ? flip_image(im) : im;
	}
	else {
		auto im = ygl::load_image4b(completePath);
		txt->ldr = flip ? flip_image(im) : im;
	}
}


void PBRTParser::parse_imagemap_texture(DeclaredTexture &dt) {
	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	dt.txt = txt;

	std::string filename = "";
	// read parameters
	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt.uscale = get_single_value<float>(params[i_u]);

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt.vscale = get_single_value<float>(params[i_v]);
	
	int i_fn = find_param("filename", params);
	if (i_fn >= 0) {
		filename = get_single_value<std::string>(params[i_fn]);
	}
	else {
		throw_syntax_error("No texture filename provided.");
	}

	if (dt.uscale < 1) dt.uscale = 1;
	if (dt.vscale < 1) dt.vscale = 1;

	load_texture(txt, filename);

	// TODO: implement scaling factors
}

//
// parse_constant_texture
//
void PBRTParser::parse_constant_texture(DeclaredTexture &dt) {
	
	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	txt->path = txt->name + ".png";
	dt.txt = txt;

	ygl::vec3f value{ 1, 1, 1 };
	// read parameters
	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);

	int i_v = find_param("value", params);
	if (i_v >= 0) {
		if (params[i_v].type == "float") {
			auto v = get_single_value<float>(params[i_v]);
			value.x = v;
			value.y = v;
			value.z = v;
		}
		else {
			value = get_single_value<ygl::vec3f>(params[i_v]);
		}
	}
	txt->ldr = make_constant_image(value);
}

//
// parse_checkerboard_texture
//
void PBRTParser::parse_checkerboard_texture(DeclaredTexture &dt) {
	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	txt->path = txt->name + ".png";
	dt.txt = txt;

	ygl::vec4f tex1{ 0,0,0, 255 }, tex2{ 1,1,1, 255};

	// read parameters
	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt.uscale = get_single_value<float>(params[i_u]);

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt.vscale = get_single_value<float>(params[i_v]);

	int i_txt1 = find_param("tex1", params);
	if (i_txt1 >= 0) {
		if (params[i_txt1].type == "float") {
			auto v = get_single_value<float>(params[i_txt1]);
			tex1.x = v;
			tex1.y = v;
			tex1.z = v;
		}
		else {
			auto v = get_single_value<ygl::vec3f>(params[i_txt1]);
			tex1.x = v.x;
			tex1.y = v.y;
			tex1.z = v.z;
		}
	}
	int i_txt2 = find_param("tex2", params);
	if (i_txt2 >= 0) {
		if (params[i_txt2].type == "float") {
			auto v = get_single_value<float>(params[i_txt2]);
			tex2.x = v;
			tex2.y = v;
			tex2.z = v;
		}
		else {
			auto v = get_single_value<ygl::vec3f>(params[i_txt2]);
			tex2.x = v.x;
			tex2.y = v.y;
			tex2.z = v.z;
		}
	}

	// hack
	if (dt.uscale < 0) dt.uscale = 1;
	if (dt.vscale < 0) dt.vscale = 1;

	txt->ldr = ygl::make_checker_image(128, 128, 64, float_to_byte(tex1), float_to_byte(tex2));
}

//
// parse_scale_texture
//
void PBRTParser::parse_scale_texture(DeclaredTexture &dt) {
	
	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	txt->path = txt->name + ".png";
	dt.txt = txt;
	ygl::texture *ytex1;
	ygl::texture *ytex2;

	// read parameters
	std::vector<PBRTParameter> params{};
	this->parse_parameters(params);
	
	// first get the first texture
	int i_tex1 = find_param("tex1", params);
	if (i_tex1 == -1)
		throw_syntax_error("Impossible to create scale texture, missing tex1.");

	if (params[i_tex1].type == "texture") {
		std::string tex1;
		tex1 = get_single_value<std::string>(params[i_tex1]);
		auto it1 = gState.nameToTexture.find(tex1);
		if (it1 == gState.nameToTexture.end())
			throw_syntax_error("tex1 not found in the loaded textures.");
		ytex1 = it1->second.txt;

	}
	else {
		ytex1 = new ygl::texture();
		if (params[i_tex1].type == "float")
			ytex1->ldr = make_constant_image(get_single_value<float>(params[i_tex1]));
		else if (params[i_tex1].type == "rgb")
			ytex1->ldr = make_constant_image(get_single_value<ygl::vec3f>(params[i_tex1]));
		else
			throw_syntax_error("Texture argument 'tex1' type not recognised in scale texture.");
	}
	// retrieve the textures
	int i_tex2 = find_param("tex2", params);
	if (i_tex2 == -1)
		throw_syntax_error("Impossible to create scale texture, missing tex2.");
	
	if (params[i_tex2].type == "texture") {
		std::string tex2;
		tex2 = get_single_value<std::string>(params[i_tex2]);
		auto it2 = gState.nameToTexture.find(tex2);
		if (it2 == gState.nameToTexture.end())
			throw_syntax_error("tex2 not found in the loaded textures.");
		ytex2 = it2->second.txt;

	}
	else {
		ytex2 = new ygl::texture();
		if (params[i_tex2].type == "float")
			ytex2->ldr = make_constant_image(get_single_value<float>(params[i_tex2]));
		else if (params[i_tex2].type == "rgb")
			ytex2->ldr = make_constant_image(get_single_value<ygl::vec3f>(params[i_tex2]));
		else
			throw_syntax_error("Texture argument 'tex2' type not recognised in scale texture.");
	}
	auto ts1 = TextureSupport(ytex1);
	auto ts2 = TextureSupport(ytex2);
	// NOTE: tiling of the smaller texture is performed here. Check if pbrt does the same

	int width = ts1.width > ts2.width ? ts1.width : ts2.width;
	int height = ts1.height > ts2.height ? ts1.height : ts2.height;
	auto img = ygl::image4b(width, height);

	for (int w = 0; w < width; w++) {
		for (int h = 0; h < height; h++) {
			int w1 = w % ts1.width;
			int h1 = h % ts1.height;
			int w2 = w % ts2.width;
			int h2 = h % ts2.height;

			auto newPixel = ts1.at(w1, h1)* ts2.at(w2, h2);
			img.at(w, h) = ygl::float_to_byte(newPixel);
		}
	}

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt.uscale = get_single_value<float>(params[i_u]);

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt.vscale = get_single_value<float>(params[i_v]);

	txt->ldr = img;
}

//
// parse_fbm_texture
// TODO: implement and test it
//
void PBRTParser::parse_fbm_texture(DeclaredTexture &dt) {
	throw_syntax_error("FBM textures are not supported yet");
	int octaves = 8;
	float roughness = 0.5;

	// TODO implement it
}

//
// execute_Texture
//
void PBRTParser::execute_Texture() {
	// TODO: repeat information is lost. One should save texture_info instead

	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected texture name string.");
	std::string textureName = this->current_token().value;
	
	if (gState.nameToTexture.find(textureName) != gState.nameToTexture.end())
		throw_syntax_error("Texture name already used.");

	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected texture type string.");
	std::string textureType = check_synonyms(this->current_token().value);

	if (textureType != "spectrum" && textureType != "rgb" && textureType != "float")
		throw_syntax_error("Unsupported texture base type: " + textureType);
	
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected texture class string.");
	std::string textureClass = this->current_token().value;
	this->advance();

	DeclaredTexture declTxt;

	if (textureClass == "imagemap") {
		this->parse_imagemap_texture(declTxt);
	}
	else if (textureClass == "checkerboard") {
		this->parse_checkerboard_texture(declTxt);
	}
	else if (textureClass == "constant") {
		this->parse_constant_texture(declTxt);
	}
	else if (textureClass == "scale") {
		this->parse_scale_texture(declTxt);
	}
	else {
		throw_syntax_error("Texture class not supported: " + textureClass);
	}

	gState.nameToTexture.insert(std::make_pair(textureName, declTxt));
}