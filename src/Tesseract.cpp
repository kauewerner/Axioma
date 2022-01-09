#include "plugin.hpp"


struct Tesseract : Module {
	enum ParamIds {
		XY_KNOB_PARAM,
		YZ_KNOB_PARAM,
		XZ_KNOB_PARAM,
		YW_KNOB_PARAM,
		XW_KNOB_PARAM,
		ZW_KNOB_PARAM,
		DISTANCE_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		XY_INPUT,
		YZ_INPUT,
		XZ_INPUT,
		YW_INPUT,
		XW_INPUT,
		ZW_INPUT,
		DISTANCE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(POINT_X_OUTPUT, 16),
		ENUMS(POINT_Y_OUTPUT, 16),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	enum RotationIds {
		XY,
		YZ,
		XZ,
		YW,
		XW,
		ZW,
		NUM_ROTATION
	};

	const float controlRate = 1024.f;
	const float rotationSpeed = 0.02f;
	
	float delta[6] = {0.f,0.f,0.f,0.f,0.f,0.f};
	float phase = 0.f;
	float vertices2D[16][2];
	float vertices3D[16][3];
	float vertices4D[16][4] = {{-1.f, -1.f, -1.f, 1.f},
							 {1.f, -1.f, -1.f, 1.f},
							 {1.f, 1.f, -1.f, 1.f},
							 {-1.f, 1.f, -1.f, 1.f},
							 {-1.f, -1.f, 1.f, 1.f},
							 {1.f, -1.f, 1.f, 1.f},
							 {1.f, 1.f, 1.f, 1.f},
							 {-1.f, 1.f, 1.f, 1.f},
							 {-1.f, -1.f, -1.f, -1.f},
							 {1.f, -1.f, -1.f, -1.f},
							 {1.f, 1.f, -1.f, -1.f},
							 {-1.f, 1.f, -1.f, -1.f},
							 {-1.f, -1.f, 1.f, -1.f},
							 {1.f, -1.f, 1.f, -1.f},
							 {1.f, 1.f, 1.f, -1.f},
							 {-1.f, 1.f, 1.f, -1.f}};
							
	const int vertexColor[16][4] = {{255, 0, 0, 255},
							 {255, 102, 0, 255},
							 {200, 113, 55, 255},
							 {255, 204, 0, 255},
							 {212, 255, 42, 255},
							 {102, 255, 0, 255},
							 {55, 200, 113, 255},
							 {85, 255, 221, 255},
							 {55, 200, 171, 255},
							 {0, 170, 212, 255},
							 {44, 137, 160, 255},
							 {0, 102, 255, 255},
							 {44, 90, 160, 255},
							 {44, 44, 160, 255},
							 {127, 42, 255, 255},
							 {212, 42, 255, 255}};

	Tesseract() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(XY_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane XY");
		configParam(YZ_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane YZ");
		configParam(XZ_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane XZ");
		configParam(YW_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane YW");
		configParam(XW_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane XW");
		configParam(ZW_KNOB_PARAM, 0.f, 1.f, 0.f, "Rotation speed plane ZW");
		configParam(DISTANCE_KNOB_PARAM, 1.5f, 8.f, 3.f, "Perspective distance");

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 3; j++){
					vertices3D[i][j] = 0.f;
				if(j < 2)
					vertices2D[i][j] = 0.f;
			}
		}
	}

	void projectOn3D(int index){
		float projectionMatrix[3][4] = {
				{1.f,0.f,0.f,0.f},
				{0.f,1.f,0.f,0.f},
				{0.f,0.f,1.f,0.f}
			};
		
		for(int i = 0; i < 3; i++){
			float sum = 0;
			for(int j = 0; j < 4; j++){
				sum += (projectionMatrix[i][j] * vertices4D[index][j]);
			}
			vertices3D[index][i] = sum;
		}	
	}

	void projectOn2D(int index){
		float projectionMatrix[2][3] = {
				{1.f,0.f,0.f},
				{0.f,1.f,0.f}
			};
		
		for(int i = 0; i < 2; i++){
			float sum = 0;
			for(int j = 0; j < 3; j++){
				sum += (projectionMatrix[i][j] * vertices3D[index][j]);
			}
			vertices2D[index][i] = sum;
		}
	}

	void perspective(int index, float distance){
		vertices3D[index][0] /= (distance - vertices3D[index][2]);
		vertices3D[index][1] /= (distance - vertices3D[index][2]);
		vertices3D[index][2] = 1.f;
	}

	void rotate(int index, int plane, float angle){
		float rotationMatrix[4][4] = {
					{0.f,0.f,0.f,0.f},
					{0.f,0.f,0.f,0.f},
					{0.f,0.f,0.f,0.f},
					{0.f,0.f,0.f,0.f},
				};
		float tempVector[4] = {0.f,0.f,0.f,0.f};

		switch(plane){
			case ZW:
				// {
				// 	{1.f,0.f,0.f,0.f},
				// 	{0.f,1.f,0.f,0.f},
				// 	{0.f,0.f,std::cos(angle), -1.f*std::sin(angle)},
				// 	{0.f,0.f,std::sin(angle), std::cos(angle)},
				// };
				rotationMatrix[0][0] = 1.f;
				rotationMatrix[1][1] = 1.f;
				
				rotationMatrix[2][2] = std::cos(angle);
				rotationMatrix[2][3] = -1.f*std::sin(angle);

				rotationMatrix[3][2] = std::sin(angle);
				rotationMatrix[3][3] = std::cos(angle);

			break;
			case XW:
				// {
				// 	{std::cos(angle),0.f,0.f,-1.f*std::sin(angle)},
				// 	{0.f,1.f,0.f,0.f},
				// 	{0.f,0.f,1.f,0.f},
				// 	{std::sin(angle),0.f,0.f, std::cos(angle)},
				// };
				rotationMatrix[1][1] = 1.f;
				rotationMatrix[2][2] = 1.f;
				
				rotationMatrix[0][0] = std::cos(angle);
				rotationMatrix[0][3] = -1.f*std::sin(angle);

				rotationMatrix[3][0] = std::sin(angle);
				rotationMatrix[3][3] = std::cos(angle);
			break;
			case YW:
				// {
				// 	{1.f,0.f,0.f,0.f},
				// 	{0.f,std::cos(angle),0.f, -1.f*std::sin(angle)},
				// 	{0.f,0.f,1.f,0.f},
				// 	{0.f,std::sin(angle),0.f, std::cos(angle)},
				// };

				rotationMatrix[0][0] = 1.f;
				rotationMatrix[2][2] = 1.f;
				
				rotationMatrix[1][1] = std::cos(angle);
				rotationMatrix[1][3] = -1.f*std::sin(angle);

				rotationMatrix[3][1] = std::sin(angle);
				rotationMatrix[3][3] = std::cos(angle);
			break;
			case XZ:
				// {
				// 	{std::cos(angle),0.f, -1.f*std::sin(angle),0.f},
				// 	{0.f,1.f,0.f,0.f},
				// 	{std::sin(angle),0.f, std::cos(angle),0.f},
				// 	{0.f,0.f,0.f,1.f},
				// };
				rotationMatrix[1][1] = 1.f;
				rotationMatrix[3][3] = 1.f;
				
				rotationMatrix[0][0] = std::cos(angle);
				rotationMatrix[0][2] = -1.f*std::sin(angle);

				rotationMatrix[2][0] = std::sin(angle);
				rotationMatrix[2][2] = std::cos(angle);
			break;
			case YZ:
				// {
				// 	{1.f,0.f,0.f,0.f},
				// 	{0.f,std::cos(angle), -1.f*std::sin(angle),0.f},
				// 	{0.f,std::sin(angle), std::cos(angle), 0.f},
				// 	{0.f,0.f,0.f,1.f},
				// };
				rotationMatrix[0][0] = 1.f;
				rotationMatrix[3][3] = 1.f;
				
				rotationMatrix[1][1] = std::cos(angle);
				rotationMatrix[1][2] = -1.f*std::sin(angle);

				rotationMatrix[2][1] = std::sin(angle);
				rotationMatrix[2][2] = std::cos(angle);
			break;
			case XY:
				// {
				// 	{std::cos(angle), -1.f*std::sin(angle),0.f,0.f},
				// 	{std::sin(angle), std::cos(angle),0.f,0.f},
				// 	{0.f,0.f,1.f,0.f},
				// 	{0.f,0.f,0.f,1.f},
				// };
				rotationMatrix[2][2] = 1.f;
				rotationMatrix[3][3] = 1.f;
				
				rotationMatrix[0][0] = std::cos(angle);
				rotationMatrix[0][1] = -1.f*std::sin(angle);

				rotationMatrix[1][0] = std::sin(angle);
				rotationMatrix[1][1] = std::cos(angle);
			break;
		}
		for(int i = 0; i < 4; i++){
			float sum = 0;
			for(int j = 0; j < 4; j++){
				sum += (rotationMatrix[i][j] * vertices4D[index][j]);
			}
			tempVector[i] = sum;
		}	
		for(int i = 0; i < 4; i++){
			vertices4D[index][i] = tempVector[i];
		}
	}


	void process(const ProcessArgs& args) override {
		float dist = clamp((params[DISTANCE_KNOB_PARAM].getValue() + inputs[DISTANCE_INPUT].getVoltage()/10.f),1.5f,8.f);
		phase += controlRate * args.sampleTime;
		if (phase >= 1.f)
		{
			for(int i = 0; i < 16; i++){
				for(int j = 0; j < 6; j++){
					delta[j] = (params[j].getValue() + inputs[j].getVoltage())/20.f;
					rotate(i, j, delta[j] * rotationSpeed);
				}
				projectOn3D(i);
				perspective(i,dist);
				projectOn2D(i);
				outputs[POINT_X_OUTPUT + i].setVoltage(clamp( (vertices2D[i][0] + 1.f) * 5.f,0.f,10.f));
				outputs[POINT_Y_OUTPUT + i].setVoltage(clamp( (vertices2D[i][1] + 1.f) * 5.f,0.f,10.f));
			}
			phase = 0.f;
		}
	
	}
};

struct TesseractDisplay : TransparentWidget {
	Tesseract* module;
	float xpoints[16];
	float ypoints[16];
	float scale;
	float centerX;
	float centerY;

	TesseractDisplay() {
		std::memset(xpoints,0.f,sizeof(xpoints));
		std::memset(ypoints,0.f,sizeof(ypoints));
		box.pos = mm2px(Vec(3.5, 6.75));
		box.size = mm2px(Vec(42, 42));
		scale = box.size.x/2.f;
		centerX = box.pos.x + (box.size.x / 2.f);
		centerY = box.pos.y + (box.size.y / 2.f);
	}

	void drawLines(const DrawArgs& args) {	
		nvgScissor(args.vg, box.pos.x, box.pos.y, box.size.x, box.size.y);	
		nvgStrokeColor(args.vg, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
		nvgBeginPath(args.vg);
		for(int i = 0; i < 4; i++)
		{
			for(int offset = 0; offset < 16; offset += 8)
			{
				nvgMoveTo(args.vg, centerX + scale*xpoints[offset + i], centerY - scale*ypoints[offset + i]);
				nvgLineTo(args.vg, centerX + scale*xpoints[offset + ((i+1) % 4)], centerY - scale*ypoints[offset + ((i+1) % 4)]);
				nvgMoveTo(args.vg, centerX + scale*xpoints[offset + i + 4], centerY - scale*ypoints[offset + i + 4]);
				nvgLineTo(args.vg, centerX + scale*xpoints[offset + ((i+1) % 4) + 4], centerY - scale*ypoints[offset + ((i+1) % 4) + 4]);
				nvgMoveTo(args.vg, centerX + scale*xpoints[offset + i], centerY - scale*ypoints[offset + i]);
				nvgLineTo(args.vg, centerX + scale*xpoints[offset + i + 4], centerY - scale*ypoints[offset + i + 4]);
				
			}
		}
		for(int i = 0; i < 8; i++)
		{
			nvgMoveTo(args.vg, centerX + scale*xpoints[i], centerY - scale*ypoints[i]);
			nvgLineTo(args.vg, centerX + scale*xpoints[i + 8], centerY - scale*ypoints[i + 8]);
		}
		nvgClosePath(args.vg);
		nvgStroke(args.vg);
	
	}

	void drawPoints(const DrawArgs& args){

		nvgScissor(args.vg, box.pos.x, box.pos.y, box.size.x, box.size.y);
		
		for(int i = 0; i < 16; i++){
			nvgFillColor(args.vg, nvgRGBAf(module->vertexColor[i][0]/255.f, module->vertexColor[i][1]/255.f, module->vertexColor[i][2]/255.f, module->vertexColor[i][3]/255.f));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, centerX + scale*xpoints[i], centerY - scale*ypoints[i], 2.f);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		}
		// nvgFillColor(args.vg, nvgRGBAf(0.0f, 1.f, 0.0f, 0.25f));
		// nvgBeginPath(args.vg);
		// nvgRect(args.vg,box.pos.x, box.pos.y, box.size.x, box.size.y);
		// nvgClosePath(args.vg);
		// nvgFill(args.vg);
	};

	void drawLayer(const DrawArgs& args, int layer) override {
		if (!module)
			return;
		
		if (layer != 1)
			return;

		for(int i = 0; i < 16; i++){
			xpoints[i] = module->vertices2D[i][0];
			ypoints[i] = module->vertices2D[i][1];
		}
		drawLines(args);
		drawPoints(args);
		
	}
};

struct TesseractWidget : ModuleWidget {
	TesseractWidget(Tesseract* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Tesseract.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			TesseractDisplay* tessDisplay = new TesseractDisplay();
			tessDisplay->module = module;
			addChild(tessDisplay);
		}

		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(20.0, 65.5)), module, Tesseract::XY_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(45.0, 65.5)), module, Tesseract::YZ_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(20.0, 80.5)), module, Tesseract::XZ_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(45.0, 80.5)), module, Tesseract::YW_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(20.0, 95.5)), module, Tesseract::XW_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(45.0, 95.5)), module, Tesseract::ZW_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(42.0, 111.0)), module, Tesseract::DISTANCE_KNOB_PARAM));

		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(8.0, 65.5)), module, Tesseract::XY_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(33.0, 65.5)), module, Tesseract::YZ_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(8.0, 80.5)), module, Tesseract::XZ_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(33.0, 80.5)), module, Tesseract::YW_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(8.0, 95.5)), module, Tesseract::XW_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(33.0, 95.5)), module, Tesseract::ZW_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(30.0, 111.0)), module, Tesseract::DISTANCE_INPUT));

		static const float deltaX = 23.5f;
		static const float deltaY = 12.f;
		for (int i = 0; i < 16; i++) {
			if(i < 8){
				addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(58.5 , 23.5 + (i*deltaY))), module, Tesseract::POINT_X_OUTPUT + i));
				addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(70.0, 23.5 + (i*deltaY))), module, Tesseract::POINT_Y_OUTPUT + i));
			} else {
				addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(58.5 + deltaX, 23.5 + ((i - 8)*deltaY))), module, Tesseract::POINT_X_OUTPUT + i));
				addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(70.0 + deltaX, 23.5 + ((i - 8)*deltaY))), module, Tesseract::POINT_Y_OUTPUT + i));
			}
		}
	}
};


Model* modelTesseract = createModel<Tesseract, TesseractWidget>("Tesseract");