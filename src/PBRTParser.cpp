#include "PBRTParser.h"

// TODOs
//	- Fix memory leaks (use shared pointers?)
//  - spectrum type support http://www.fourmilab.ch/documents/specrend/


// =====================================================================================
//                           HELPER FUNCTIONS
// =====================================================================================

//
// check_synonym
// some types are synonyms, transform them to default.
//
std::string check_synonym(std::string s) {
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
// check_type_existence
//
bool check_type_existence(std::string &val) {
	std::vector<std::string> varTypes{ "integer", "float", "point2", "vector2", "point3", "vector3",\
		"normal3", "spectrum", "bool", "string", "rgb", "color", "point", "vector", "normal", "texture" };
	for (auto s : varTypes)
		if (s == val)
			return true;
	return false;
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
			std::cout << "(Line " << this->lexers.at(0)->get_line() << ") Ignoring "\
				<< this->current_token().value << " directive..\n";
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
		std::cout << "(Line " << this->lexers.at(0)->get_line() << ") Ignoring "\
			<< this->current_token().value << " directive..\n";
		this->ignore_current_directive();
	}
}

// ----------------------------------------------------------------------------
//                   DIRECTIVE PARAMETERS PARSING
//
// The following functions are used to parse a directive parameter, i.e.
// "type name" <value>, where value can be a single value or array of values.
// Type checking is also performed.
// NOTE: to parse values of different types, i tried to use a template function
// but the result was messy and less readable than the current solution.
// ----------------------------------------------------------------------------

//
// parse_value_int
// Parse a single value or array of integers.
//
void PBRTParser::parse_value_int(std::vector<int> *vals) {

	bool isArray = false;
	if (this->current_token().value == "[") {
		this->advance();
		isArray = true;
	}

	while (this->current_token().type == LexemeType::NUMBER) {
		vals->push_back(atoi(this->current_token().value.c_str()));
		this->advance();
		if (!isArray)
			break;
	}
	if (isArray) {
		if (this->current_token().value == "]")
			this->advance();
		else
			throw_syntax_error("Expected closing ']'.");
	}
	if (vals->size() == 0)
		throw_syntax_error("The array parsed is empty.");
}

//
// parse_value_float
// Parse a single value or array of floats.
//
void PBRTParser::parse_value_float(std::vector<float> *vals) {

	bool isArray = false;
	if (this->current_token().value == "[") {
		this->advance();
		isArray = true;
	}

	while (this->current_token().type == LexemeType::NUMBER) {
		vals->push_back(atof(this->current_token().value.c_str()));
		this->advance();
		if (!isArray)
			break;
	}
	if (isArray) {
		if (this->current_token().value == "]")
			this->advance();
		else
			throw_syntax_error("Expected closing ']'.");
	}
	if (vals->size() == 0)
		throw_syntax_error("The array parsed is empty.");
}

//
// parse_value_string
// Parse a single value or array of strings.
//
void PBRTParser::parse_value_string(std::vector<std::string> *vals) {

	bool isArray = false;
	if (this->current_token().value == "[") {
		this->advance();
		isArray = true;
	}

	while (this->current_token().type == LexemeType::STRING) {
		vals->push_back(this->current_token().value);
		this->advance();
		if (!isArray)
			break;
	}
	if (isArray) {
		if (this->current_token().value == "]")
			this->advance();
		else
			throw_syntax_error("Expected closing ']'.");
	}
	if (vals->size() == 0)
		throw_syntax_error("The array parsed is empty.");
}

//
// parse_parameter
// Fills the PBRTParameter structure with type name and value of the
// parameter. It is upon to the caller to convert "void * value" to
// the correct pointer type (by reading the name and type).
//
void PBRTParser::parse_parameter(PBRTParameter &par){

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected a string with type and name of a parameter.");
    
	auto tokens = split(this->current_token().value);
    
	// handle type
	if(!check_type_existence(tokens[0]))
        throw_syntax_error("Unrecognized type.");
	par.type = check_synonym(std::string(tokens[0]));
	par.name = tokens[1];
	this->advance();

	// now, according to type, we parse the value
	if (par.type == "string" || par.type == "texture") {
		std::vector<std::string> vals{};
		this->parse_value_string(&vals);
		if (vals.size() > 1)
			throw_syntax_error("Expected only one value.");
		std::string *v = new std::string();
		*v = vals[0];
		par.value = (void *)v;
	}

	else if (par.type == "float") {
		std::vector<float> *vals = new std::vector<float>();
		this->parse_value_float(vals);
		par.value = (void *)vals;
	}

	else if (par.type == "integer") {
		// This can be a single value or multiple values.
		std::vector<int> *vals = new std::vector<int>();
		this->parse_value_int(vals);
		par.value = (void *)vals;
	}
	else if (par.type == "bool") {
		std::vector<std::string> vals{};
		this->parse_value_string(&vals);
		if (vals.size() > 1)
			throw_syntax_error("Expected only one value.");
		std::string *v = new std::string();
		*v = vals[0];
		par.value = (void *)v;

		if (vals[0] != "true" && vals[0] != "false")
			throw_syntax_error("Invalid value for boolean variable.");
	}
	// now we come at a special case of arrays of vec3f
	else if (par.type == "point3" || par.type == "normal3"|| par.type == "rgb" || par.type == "spectrum") {
		std::vector<float> *vals = new::std::vector<float>();
		this->parse_value_float(vals);
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
	else {
		throw_syntax_error("Cannot able to parse the value: type '" + par.type + "' not supported.");
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

// ------------------------------------------------------------------------------------
//                               TRANSFORMATIONS
// ------------------------------------------------------------------------------------

//
// execute_Translate()
//
void PBRTParser::execute_Translate() {
	float x, y, z;

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Translate directive.");
	x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Translate directive.");
	y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Translate directive.");
	z = atof(this->current_token().value.c_str());
	
	const ygl::vec3f transl_vec { x, y, z };
	auto transl_mat = ygl::frame_to_mat(ygl::translation_frame(transl_vec));
	this->gState.CTM = this->gState.CTM * transl_mat;
	this->advance();
}

//
// execute_Scale()
//
void PBRTParser::execute_Scale(){
	float x, y, z;

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Scale directive.");
	x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Scale directive.");
	y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Scale directive.");
	z = atof(this->current_token().value.c_str());

	const ygl::vec3f scale_vec{ x, y, z };
	
	auto scale_mat = ygl::frame_to_mat(ygl::scaling_frame(scale_vec));
	this->gState.CTM =  this->gState.CTM * scale_mat;
	this->advance();
}

//
// execute_Rotate()
//
void PBRTParser::execute_Rotate() {
	float angle, x, y, z;

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'angle' parameter of Rotate directive.");
	angle = atof(this->current_token().value.c_str());
	angle = angle *ygl::pif / 180;

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'x' parameter of Rotate directive.");
	x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'y' parameter of Rotate directive.");
	y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'z' parameter of Rotate directive.");
	z = atof(this->current_token().value.c_str());

	const ygl::vec3f rot_vec{ x, y, z };
	auto rot_mat = ygl::frame_to_mat(ygl::rotation_frame(rot_vec, angle));
	this->gState.CTM = this->gState.CTM * rot_mat;
	this->advance();
}

//
// execute_LookAt()
//
void PBRTParser::execute_LookAt(){
	float eye_x, eye_y, eye_z, look_x, look_y, look_z, up_x, up_y, up_z;
	
	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_x' parameter of LookAt directive.");
	eye_x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_y' parameter of LookAt directive.");
	eye_y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'eye_z' parameter of LookAt directive.");
	eye_z = atof(this->current_token().value.c_str());

	const ygl::vec3f eye{ eye_x, eye_y, eye_z };

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_x' parameter of LookAt directive.");
	look_x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_y' parameter of LookAt directive.");
	look_y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'look_z' parameter of LookAt directive.");
	look_z = atof(this->current_token().value.c_str());

	const ygl::vec3f peek { look_x, look_y, look_z };

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_x' parameter of LookAt directive.");
	up_x = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_y' parameter of LookAt directive.");
	up_y = atof(this->current_token().value.c_str());

	this->advance();
	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_error("Expected a float value for 'up_z' parameter of LookAt directive.");
	up_z = atof(this->current_token().value.c_str());

	const ygl::vec3f up{ up_x, up_y, up_z };

	auto fm = ygl::lookat_frame(eye, peek, up);
	fm.x = -fm.x;
	fm.z = -fm.z;
	auto mm = ygl::frame_to_mat(fm);
	this->defaultFocus = ygl::length(eye - peek);
	this->gState.CTM = this->gState.CTM * ygl::inverse(mm); // inverse here because pbrt boh
	this->advance();
}

//
// execute_Transform()
//
void PBRTParser::execute_Transform() {
	this->advance();
	std::vector<float> *vals = new std::vector<float>();
	this->parse_value_float(vals);
	ygl::mat4f nCTM;
	if (vals->size() != 16)
		throw_syntax_error("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals->at(i * 4 + j);
		}
	}
	gState.CTM = nCTM;
}

//
// execute_ConcatTransform()
//
void PBRTParser::execute_ConcatTransform() {
	this->advance();
	std::vector<float> *vals = new std::vector<float>();
	this->parse_value_float(vals);
	ygl::mat4f nCTM;
	if (vals->size() != 16)
		throw_syntax_error("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals->at(i * 4 + j);
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
	ygl::camera *cam = new ygl::camera;
	cam->aspect = defaultAspect;
	cam->aperture = 0;
	cam->yfov = 90.0f * ygl::pif / 180;
	cam->focus = defaultFocus;
	char buff[100];
	sprintf(buff, "c%d", scn->cameras.size());
	cam->name = std::string(buff);
	
	// CTM defines world to camera transformation
	cam->frame = ygl::mat_to_frame(ygl::inverse(this->gState.CTM));
	cam->frame.z = -cam->frame.z;
	
	// Parse the camera parameters
	// First parameter is the type
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string camType = this->current_token().value;
	this->advance();

	// RESTRICTION: only perspective camera is supported
	if (camType != "perspective")
		throw_syntax_error("Only perspective camera type supported.");

	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "frameaspectratio") {
			if (par.type != "float")
				throw_syntax_error("'frameaspectratio' must have type float.");
			auto data = (std::vector<float> *)par.value;
			cam->aspect = data->at(0);
			delete data;
		}
		/*
		else if (par.name == "lensradius") {
			if (par.type != "float")
				throw_syntax_error("'lensradius' must have type float.");
			auto data = (std::vector<float> *)par.value;
			cam->aperture = data->at(0);
			delete data;
		}
		else if (par.name == "focaldistance") {
			if (par.type != "float")
				throw_syntax_error("'focaldistance' must have type float.");
			auto data = (std::vector<float> *)par.value;
			cam->focus = data->at(0);
			delete data;
		}
		*/
		else if (par.name == "fov") {
			if (par.type != "float")
				throw_syntax_error("'fov' must have type float.");
			auto data = (std::vector<float> *)par.value;
			// NOTE: this is true if the image is vertical
			cam->yfov = data->at(0)  * ygl::pif / 180;
			delete data;
		}
	}
	scn->cameras.push_back(cam);
}

//
// execute_Film
//
void PBRTParser::execute_Film() {
	
	// First parameter is the type
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected type string.");
	std::string filmType = this->current_token().value;
	this->advance();

	if (filmType != "image")
		throw_syntax_error("Only image \"film\" is supported.");

	int xres = 0;
	int yres = 0;
	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "xresolution"){
			if (par.type != "integer")
				throw_syntax_error("'xresolution' must have integer type.");

			auto data = (std::vector<int> *)par.value;
			xres = data->at(0);
			delete data;
		}
		else if (par.name == "yresolution"){
			if (par.type != "integer")
				throw_syntax_error("'yresolution' must have integer type.");

			auto data = (std::vector<int> *)par.value;
			yres = data->at(0);
			delete data;
		}
	}

	if (xres && yres) {
		this->defaultAspect = ((float)xres) / ((float)yres);
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
// TODO: curves
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

	std::vector<ygl::vec3f> *p = nullptr; // max 4 points
	int degree = 3; // only legal options 2 and 3
	std::string type = "";
	std::vector<ygl::vec3f> *N = nullptr;
	int splitDepth = 3;
	int width = 1;

	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "p"){
			if (par.type != "point3")
				throw_syntax_error("Parameter 'p' must be of point type.");
			p = (std::vector<ygl::vec3f> *)par.value;
		}
		else if (par.name == "degree"){
			if (par.type != "integer")
				throw_syntax_error("Parameter 'degree' must be of integer type.");
			auto data = (std::vector<int> *)par.value;
			degree = data->at(0);
			delete data;
		}
		else if (par.name == "type"){
			if (par.type != "string")
				throw_syntax_error("Parameter 'type' must be of string type.");
			auto data = (std::string *)par.value;
			type = data->at(0);
			delete data;
		}
		else if (par.name == "N"){
			if (par.type != "normal3")
				throw_syntax_error("Parameter 'N' must be of normal type.");
			N = (std::vector<ygl::vec3f> *)par.value;
		}
		else if (par.name == "splitdepth"){
			if (par.type != "integer")
				throw_syntax_error("Parameter 'splitdepth' must be of normal type.");
			auto data = (std::vector<int> *)par.value;
			splitDepth = data->at(0);
			delete data;
		}
		else if (par.name == "width"){
			if (par.type != "float")
				throw_syntax_error("Parameter 'width' must be of float type.");
			auto data = (std::vector<float> *)par.value;
			width = data->at(0);
			delete data;
		}
	}
	std::cerr << "Curves are not supported for now..\n";
	// TODO
	
}

//
// my_compute_normals
// because pbrt computes it differently
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

	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "indices"){
			if (par.type != "integer")
				throw_syntax_error("'indices' parameter must have integer type.");
			std::vector<int> *data = (std::vector<int> *) par.value;
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
		else if (par.name == "P"){
			if (par.type != "point3")
				throw_syntax_error("'P' parameter must be of point3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				shp->pos.push_back(p);
				shp->radius.push_back(1.0f); // default radius
			}
			delete data;
			PCheck = true;
		}
		else if (par.name == "N"){
			if (par.type != "normal3")
				throw_syntax_error("'N' parameter must be of normal3 type.");
			std::vector<ygl::vec3f> *data = (std::vector<ygl::vec3f> *)par.value;
			for (auto p : *data) {
				shp->norm.push_back(p);
			}
			delete data;
		}
		else if (par.name == "uv" || par.name == "st"){
			if (par.type != "float")
				throw_syntax_error("'uv' parameter must be of float type.");
			std::vector<float> *data = (std::vector<float> *)par.value;
			int j = 0;
			while (j < data->size()){
				ygl::vec2f uv;
				uv[0] = data->at(j++);
				uv[1] = data->at(j++);
				shp->texcoord.push_back(uv);
			}
			delete data;
		}
		else {
			std::cout << "ignoring parameter '" << par.name << "' ..\n";
		}

		// TODO: materials parameters overriding
	}
	if (shp->norm.size() == 0);
		//my_compute_normals(shp->triangles, shp->pos, shp->norm, true);

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
	std::string shpName = get_unique_id(CounterID::shape);
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
	shp->name = shpName;
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

		auto data = (std:: string *)par.value;
		std::string fname = this->current_path() + "/" + *data;
		delete data;

		if (!parse_ply(fname, &shp)){
			throw_syntax_error("Error parsing ply file: " + fname);
		}

		while (this->current_token().type != LexemeType::IDENTIFIER)
			this->advance();
	
	}
	else {
		this->ignore_current_directive();
		std::cout << "Ignoring shape " << shapeName << "..\n";
		return;
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
	// DEGUB
	this->advance();

	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value == "ObjectEnd")){
		this->execute_world_directive();
	}

	auto it = nameToObject.find(objName);
	if (it == nameToObject.end()){
		nameToObject.insert(std::make_pair(objName, ShapeData(shapesInObject, this->gState.CTM)));
	}		
	else {
		auto prev = it->second;
		if (prev.referenced == false) {
			delete prev.sg;
		}
		it->second = ShapeData(this->shapesInObject, this->gState.CTM);
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

	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "scale"){
			if (par.type != "spectrum" && par.type != "rgb")
				throw_syntax_error("'scale' parameter expects a 'spectrum' or 'rgb' type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (par.name == "L"){
			if (par.type != "spectrum" && par.type != "rgb") // TODO: implement black body
				throw_syntax_error("'L' parameter expects a spectrum or rgb type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			L = data->at(0);
			delete data;
		}
		else if (par.name == "mapname"){
			if (par.type != "string")
				throw_syntax_error("'mapname' parameter expects a string type.");
			auto data = (std::string *)par.value;
			mapname = *data;
			delete data;
		}
	}

	ygl::environment *env = new ygl::environment;
	env->name = get_unique_id(CounterID::environment);
	env->ke = scale * L;

	if (mapname.length() > 0) {
		auto completePath = this->current_path() + mapname;
		auto path_name = get_path_and_filename(completePath);
		ygl::texture *txt = new ygl::texture;
		scn->textures.push_back(txt);
		txt->path = completePath;
		txt->name = get_unique_id(CounterID::texture);
		if (ygl::endswith(mapname, ".png")){
			txt->ldr = ygl::load_image4b(completePath);
		}
		else if (ygl::endswith(mapname, ".exr")){
			txt->hdr = ygl::load_image4f(completePath);
		}
		else {
			throw_syntax_error("Texture format not recognised.");
		}
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

	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "scale"){
			if (par.type != "spectrum")
				throw_syntax_error("'scale' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (par.name == "I"){
			if (par.type != "spectrum")
				throw_syntax_error("'I' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			I = data->at(0);
			delete data;
		}
		else if (par.name == "from"){
			if (par.type == "point3" && par.type == "point")
				throw_syntax_error("'from' parameter expects a point type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			point = data->at(0);
			delete data;
		}
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
	// TODO check validity of frame
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

	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "scale"){
			if (par.type != "spectrum")
				throw_syntax_error("'scale' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			scale = data->at(0);
			delete data;
		}
		else if (par.name == "L"){
			if (par.type != "specturm" && par.type != "rgb")
				throw_syntax_error("'L' parameter expects a spectrum type.");
			auto data = (std::vector<ygl::vec3f>*)par.value;
			L = data->at(0);
			delete data;
		}
		else if (par.name == "twosided"){
			if (par.type != "bool")
				throw_syntax_error("'twosided' parameter expects a bool type.");
			auto data = (std::vector<bool>*)par.value;
			twosided = data->at(0);
			delete data;
		}
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
// execute_Material
//
void PBRTParser::execute_Material() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected lightsource type as a string.");

	std::string materialType = this->current_token().value;
	this->advance();
	// TODO: check material type
	// For now, implement matte, plastic, metal, mirror
	// NOTE: shapes can override material parameters
	ygl::material *newMat = new ygl::material();
	
	newMat->name = get_unique_id(CounterID::material);
	newMat->type = ygl::material_type::specular_roughness;
		
	ParsedMaterialInfo pmi;

	if (materialType == "matte")
		this->parse_material_matte(newMat, pmi, false);
	else if (materialType == "metal")
		this->parse_material_metal(newMat, pmi, false);
	else if (materialType == "mix")
		this->parse_material_mix(newMat, pmi, false);
	else if (materialType == "plastic")
		this->parse_material_plastic(newMat, pmi, false);
	else if (materialType == "mirror")
		this->parse_material_mirror(newMat, pmi, false);
	else if (materialType == "uber")
		this->parse_material_uber(newMat, pmi, false);
	else if (materialType == "translucent")
		this->parse_material_translucent(newMat, pmi, false);
	else {
		//throw_syntax_error("Material '" + materialType + " not supported. Ignoring..\n");
		std::cerr << "Material '" + materialType + "' not supported. Ignoring and using 'matte'..\n";
		this->parse_material_matte(newMat, pmi, false);
	}
	this->gState.mat = newMat;
	scn->materials.push_back(this->gState.mat);
		
}

//
// parse_material_properties
//
void PBRTParser::parse_material_properties(ParsedMaterialInfo &pmi) {

	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "type"){
			pmi.b_type = true;
			if (par.type != "string") 
				throw_syntax_error("Parameter 'type' expects a 'string' type.");
			auto data = (std:: string *)par.value;
			pmi.type = *data;
			delete data;
		}
		else if (par.name == "Kd"){
			pmi.b_kd = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				// TODO: conversion rgb - spectrum
				pmi.kd = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.textureKd = {};
				pmi.kd = { 1, 1, 1 };
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'Kd' parameter was not found.");
				pmi.textureKd = it->second;
			}
			else {
				throw_syntax_error("'Kd' parameter must be a spectrum, rgb or a texture.");
			}
		}
		else if (par.name == "Ks"){
			pmi.b_ks = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				// TODO: conversion rgb - spectrum
				pmi.ks = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.ks = { 1,1,1 };
				pmi.textureKs = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'Ks' parameter was not found.");
				pmi.textureKs = it->second;
			}
			else {
				throw_syntax_error("'Ks' parameter must be a spectrum, rgb or a texture.");
			}
		}
		else if (par.name == "Kr" || par.name == "reflect"){
			pmi.b_kr = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				// TODO: conversion rgb - spectrum
				pmi.kr = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.kr = { 1,1,1 };
				pmi.textureKr = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'Kr' parameter was not found.");
				pmi.textureKr = it->second;
			}
			else {
				throw_syntax_error("'Kr' parameter must be a spectrum, rgb or a texture.");
			}
		}
		else if (par.name == "Kt" || par.name == "transmit"){
			pmi.b_kt = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				// TODO: conversion rgb - spectrum
				pmi.kt = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.kt = { 1,1,1 };
				pmi.textureKt = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'Kt' parameter was not found.");
				pmi.textureKt = it->second;
			}
			else {
				throw_syntax_error("'Kt' parameter must be a spectrum, rgb or a texture.");
			}
		}
		else if (par.name == "roughness"){
			pmi.b_rs = true;
			if (par.type == "float"){
				auto data = (std::vector<float> *)par.value;
				pmi.rs = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.rs = 1;
				pmi.textureRs = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'roughness' parameter was not found.");
				pmi.textureKs = it->second;
			}
			else {
				throw_syntax_error("'roughness' parameter must be a float or a texture.");
			}
		}else if (par.name == "eta"){
			pmi.b_eta = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				pmi.eta = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.eta = { 1,1,1 };
				pmi.textureETA = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'eta' parameter was not found.");
				pmi.textureETA = it->second;
			}
			else {
				throw_syntax_error("'eta' parameter must be a spectrum or a texture.");
			}
		}
		else if (par.name == "k"){
			pmi.b_k = true;
			if (par.type == "spectrum" || par.type == "rgb"){
				auto data = (std::vector<ygl::vec3f> *)par.value;
				pmi.k = data->at(0);
				delete data;
			}
			else if (par.type == "texture"){
				auto data = (std:: string *)par.value;
				auto txtName = *data;
				delete data;
				pmi.k = { 1,1,1 };
				pmi.textureK = {};
				auto it = gState.nameToTexture.find(txtName);
				if (it == gState.nameToTexture.end())
					throw_syntax_error("the specified texture for 'k' parameter was not found.");
				pmi.textureK = it->second;
			}
			else {
				throw_syntax_error("'k' parameter must be a spectrum or a texture.");
			}
		}
		else if (par.name == "amount"){
			pmi.b_amount = true;
			if (par.type != "float" && par.type != "rgb")
				throw_syntax_error("'amount' parameter expects a 'float' or 'rgb' type.");
			
			if (par.type == "float") {
				auto data = (std::vector<float> *)par.value;
				pmi.amount = data->at(0);
				delete data;
			}
			else {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				pmi.amount = (data->at(0).x + data->at(0).y + data->at(0).z) / 3;
				delete data;
			}
		}
		else if (par.name == "namedmaterial1") {
			pmi.b_namedMaterial1 = true;
			if (par.type != "string")
				throw_syntax_error("'namedmaterial' expects a 'string' type.");
			auto data = (std:: string *)par.value;
			pmi.namedMaterial1 = *data;
			delete data;
		}
		else if (par.name == "namedmaterial2") {
			pmi.b_namedMaterial2 = true;
			if (par.type != "string")
				throw_syntax_error("'namedmaterial' expects a 'string' type.");
			auto data = (std:: string *)par.value;
			pmi.namedMaterial2 = *data;
			delete data;
		}
		else if (par.name == "bumpmap") {
			pmi.b_bump = true;
			if (par.type != "texture")
				throw_syntax_error("'bumpmap' expects a 'texture' type.");
			auto data = (std:: string *)par.value;
			auto it = gState.nameToTexture.find(*data);
			delete data;
			if (it == gState.nameToTexture.end())
				throw_syntax_error("The specified texture for parameter 'bumpmap' was not found.");
			pmi.bump = {};
			pmi.bump = it->second;
		}
		else {
			std::cerr << "Material property " << par.name << " ignored..\n";
		}
	}
}

//
// parse_material_matte
//
void PBRTParser::parse_material_matte(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	mat->kd = { 0.5f, 0.5f, 0.5f };
	
	if(!already_parsed)
		this->parse_material_properties(pmi);

	if (pmi.b_kd) {
		mat->kd = pmi.kd;
		mat->kd_txt = pmi.textureKd;
	}
	if (pmi.b_bump) {
		mat->bump_txt = pmi.bump;
	}
}

//
// parse_material_uber
//
void PBRTParser::parse_material_uber(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	mat->kd = { 0.25f, 0.25f, 0.25f };
	mat->ks = { 0.25f, 0.25f, 0.25f };
	mat->kr = { 0, 0, 0 };
	mat->rs = 0.01f;
	if (!already_parsed)
		this->parse_material_properties(pmi);


	if (pmi.b_kd) {
		mat->kd = pmi.kd;
		mat->kd_txt = pmi.textureKd;
	}
	if (pmi.b_ks) {
		mat->ks= pmi.ks;
		mat->ks_txt = pmi.textureKs;
	}
	if (pmi.b_kr) {
		mat->kr = pmi.kr;
		mat->kr_txt = pmi.textureKr;
	}
	if (pmi.b_rs) {
		mat->rs = pmi.rs;
		mat->rs_txt = pmi.textureRs;
	}
	if (pmi.b_bump) {
		mat->bump_txt = pmi.bump;
	}
}

//
// parse_material_translucent
//
void PBRTParser::parse_material_translucent(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	mat->kd = { 0.25f, 0.25f, 0.25f };
	mat->ks = { 0.25f, 0.25f, 0.25f };
	mat->kr = { 0.5f, 0.5f, 0.5f };
	mat->kt = { 0.5f, 0.5f, 0.5f };
	mat->rs = 0.1f;
	if (!already_parsed)
		this->parse_material_properties(pmi);


	if (pmi.b_kd) {
		mat->kd = pmi.kd;
		mat->kd_txt = pmi.textureKd;
	}
	if (pmi.b_ks) {
		mat->ks = pmi.ks;
		mat->ks_txt = pmi.textureKs;
	}
	if (pmi.b_kr) {
		mat->kr = pmi.kr;
		mat->kr_txt = pmi.textureKr;
	}
	if (pmi.b_kt) {
		mat->kt = pmi.kt;
		mat->kt_txt = pmi.textureKt;
	}
	if (pmi.b_rs) {
		mat->rs = pmi.rs;
		mat->rs_txt = pmi.textureRs;
	}
	if (pmi.b_bump) {
		mat->bump_txt = pmi.bump;
	}
}

//
// parser_material_metal
//
void PBRTParser::parse_material_metal(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	// TODO: default values = copper
	ygl::vec3f eta{ 0.5, 0.5, 0.5 };
	ygl::vec3f k { 0.5, 0.5, 0.5 };
	mat->rs = 0.01;
	if (!already_parsed)
		this->parse_material_properties(pmi);

	if (pmi.b_eta) {
		mat->ks = pmi.eta;
		mat->ks_txt = pmi.textureETA;
	}
	if (pmi.b_k) {
		mat->kd = pmi.k;
		mat->kd_txt = pmi.textureK;
	}
	if (pmi.b_rs) {
		mat->rs = pmi.rs;
		mat->rs_txt = pmi.textureRs;
	}
	mat->type = ygl::material_type::metallic_roughness;
}

//
// parse_material_mirror
//
void PBRTParser::parse_material_mirror(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	mat->kr = { 0.9f, 0.9f, 0.9f };
	if (!already_parsed)
		this->parse_material_properties(pmi);


	if (pmi.b_kr) {
		mat->kr = pmi.kr;
		mat->kr_txt = pmi.textureKr;
	}
	if (pmi.b_bump) {
		mat->bump_txt = pmi.bump;
	}
}

//
// parse_material_plastic
//
void PBRTParser::parse_material_plastic(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	mat->kd = { 0.25, 0.25, 0.25 };
	mat->ks = { 0.25, 0.25, 0.25 };
	mat->rs = 0.1;
	if(!already_parsed)
		this->parse_material_properties(pmi);

	if (pmi.b_kr) {
		mat->kr = pmi.kr;
		mat->kr_txt = pmi.textureKr;
	}
	if (pmi.b_bump) {
		mat->bump_txt = pmi.bump;
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
void PBRTParser::parse_material_mix(ygl::material *mat, ParsedMaterialInfo &pmi, bool already_parsed) {
	if (!already_parsed)
		this->parse_material_properties(pmi);

	float amount = 0.5f;
	if (pmi.b_amount)
		amount = pmi.amount;

	if (!pmi.b_namedMaterial1)
		throw_syntax_error("Missing material1 to mix.");

	if (!pmi.b_namedMaterial2)
		throw_syntax_error("Missing material2 to mix.");

	auto i1 = gState.nameToMaterial.find(pmi.namedMaterial1);
	if (i1 == gState.nameToMaterial.end())
		throw_syntax_error("NamedMaterial1 " + pmi.namedMaterial1 + " was not declared.");
	auto mat1 = i1->second;

	auto i2 = gState.nameToMaterial.find(pmi.namedMaterial2);
	if (i2 == gState.nameToMaterial.end())
		throw_syntax_error("NamedMaterial2 " + pmi.namedMaterial2 + " was not declared.");
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

	ParsedMaterialInfo pmi;
	this->parse_material_properties(pmi);

	ygl::material *mat = new ygl::material;
	mat->name = get_unique_id(CounterID::material);

	if (pmi.b_type == false)
		throw_syntax_error("Expected material type.");

	if (pmi.type == "metal")
		this->parse_material_metal(mat, pmi, true);
	else if (pmi.type == "plastic")
		this->parse_material_plastic(mat, pmi, true);
	else if (pmi.type == "matte")
		this->parse_material_matte(mat, pmi, true);
	else if (pmi.type == "mirror")
		this->parse_material_mirror(mat, pmi, true);
	else if (pmi.type == "uber")
		this->parse_material_uber(mat, pmi, true);
	else if (pmi.type == "mix")
		this->parse_material_mix(mat, pmi, true);
	else if (pmi.type == "translucent")
		this->parse_material_translucent(mat, pmi, true);
	else
		throw_syntax_error("Material type " + pmi.type + " not supported or recognized.");
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
ygl::image4b make_constant_image(float v) {
	auto  x = ygl::image4b(1, 1);
	auto b = ygl::float_to_byte(v);
	x.at(0, 0) = { b, b, b, 255 };
	return x;
}

//
// make_constant_image
//
ygl::image4b make_constant_image(ygl::vec3f v) {
	auto  x = ygl::image4b(1, 1);
	x.at(0, 0) = ygl::float_to_byte({ v.x, v.y, v.z, 1 });
	return x;
}

void PBRTParser::parse_imagemap_texture(ygl::texture *txt) {
	std::string filename = "";
	
	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "filename") {
			if (par.type != "string")
				throw_syntax_error("'filename' parameter must have string type.");
			auto data = (std::string *)par.value;
			filename = this->current_path() + "/" + *data;
			delete data;
		}
	}
	if (filename == "")
		throw_syntax_error("No texture filename provided.");

	txt->path = filename;
	if (ygl::endswith(filename, ".png")) {
		txt->ldr = ygl::load_image4b(filename);
	}
	else if (ygl::endswith(filename, ".exr")) {
		txt->hdr = ygl::load_image4f(filename);
	}
	else {
		throw_syntax_error("Texture format not recognized.");
	}
}

//
// parse_constant_texture
//
void PBRTParser::parse_constant_texture(ygl::texture *txt) {
	ygl::vec3f value { 1, 1, 1 };
	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "value") {
			if (par.type == "float") {
				auto data = (std::vector<float> *)par.value;
				auto v = data->at(0);
				value.x = v;
				value.y = v;
				value.z = v;
				delete data;
			}
			else if (par.type == "spectrum" || par.type == "rgb") {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				value = data->at(0);
				delete data;

			}
			else {
				throw_syntax_error("'value' parameter must have float/spectrum type.");
			}
		}
	}
	txt->ldr = make_constant_image(value);
	txt->path = txt->name + ".png";
}

//
// parse_checkerboard_texture
//
void PBRTParser::parse_checkerboard_texture(ygl::texture *txt) {
	float uscale = 1, vscale = 1;
	ygl::vec4f tex1{ 0,0,0, 255 }, tex2{ 1,1,1, 255};

	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		PBRTParameter par;
		this->parse_parameter(par);

		if (par.name == "uscale") {
			if (par.type != "float")
				throw_syntax_error("'uscale' parameter must have float type.");
			auto data = (std::vector<float> *)par.value;
			uscale = data->at(0);
			delete data;
		}else if(par.name == "vscale") {
			if (par.type != "float")
				throw_syntax_error("'vscale' parameter must have float type.");
			auto data = (std::vector<float> *)par.value;
			vscale = data->at(0);
			delete data;
		}
		else if (par.name == "tex1") {
			if (par.type == "float") {
				auto data = (std::vector<float> *)par.value;
				auto v = data->at(0);
				delete data;
				tex1.x = v;
				tex1.y = v;
				tex1.z = v;
			}
			else if (par.type == "spectrum" || par.type == "rgb") {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				auto v = data->at(0);
				tex1.x = v.x;
				tex1.y = v.y;
				tex1.z = v.z;
				delete data;
			}
			else {
				throw_syntax_error("'vscale' parameter must have float/spectrum type.");
			}
		}
		else if (par.name == "tex2") {
			if (par.type == "float") {
				auto data = (std::vector<float> *)par.value;
				auto v = data->at(0);
				delete data;
				tex2.x = v;
				tex2.y = v;
				tex2.z = v;
			}
			else if (par.type == "spectrum" || par.type == "rgb") {
				auto data = (std::vector<ygl::vec3f> *)par.value;
				auto v = data->at(0);
				tex2.x = v.x;
				tex2.y = v.y;
				tex2.z = v.z;
				delete data;
			}
			else {
				throw_syntax_error("'vscale' parameter must have float/spectrum type.");
			}
		}
	}

	// hack
	if (uscale < 0) uscale = 1;
	if (vscale < 0) vscale = 1;

	txt->ldr = ygl::make_checker_image(128 * vscale, 128 * uscale, 64, float_to_byte(tex1), float_to_byte(tex2));
	txt->path = txt->name + ".png";
}

//
// parse_fbm_texture
// TODO: implement and test it
//
void PBRTParser::parse_fbm_texture(ygl::texture *txt) {
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
	std::string textureType = check_synonym(this->current_token().value);

	if (textureType != "spectrum" && textureType != "rgb" && textureType != "float")
		throw_syntax_error("Unsupported texture base type: " + textureType);
	
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_error("Expected texture class string.");
	std::string textureClass = this->current_token().value;
	this->advance();

	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	scn->textures.push_back(txt);

	if (textureClass == "imagemap") {
		this->parse_imagemap_texture(txt);
	}
	else if (textureClass == "checkerboard") {
		this->parse_checkerboard_texture(txt);
	}
	else if (textureClass == "constant") {
		this->parse_constant_texture(txt);
	}
	else {
		throw_syntax_error("Texture class not supported: " + textureClass);
	}

	gState.nameToTexture.insert(std::make_pair(textureName, txt));
}