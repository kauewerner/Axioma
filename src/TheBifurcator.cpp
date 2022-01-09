#include "plugin.hpp"


struct TheBifurcator : Module {
	enum ParamIds {
		FUNCTION_KNOB_PARAM,
		ITER_KNOB_PARAM,
		R_KNOB_PARAM,
		CLOCK_KNOB_PARAM,
		TYPE_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		R_INPUT,
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUT, 8),
		ENUMS(GATE_OUTPUT, 8),
		ENUMS(TRIG_OUTPUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(OUTPUT_LIGHTS, 8),
		NUM_LIGHTS
	};

	enum FunctionTypes {
		LOGISTIC,
		TENT,
		PARABOLA,
		NUM_FUNCTION_TYPES
	};

	float phase = 0.f;
	float currentX = 0.5f;
	float lastPoints[5];
	float plotBuffer[190];
	

	int plotIndex = 0;
	int plotBufferSize = 190;

	bool computeFlag = false;
	bool outputActivation[8] = {};
	
	dsp::BooleanTrigger tapTriggers[8];
	dsp::PulseGenerator pulseGenerators[8];
	dsp::SchmittTrigger clockTrigger;

	TheBifurcator() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FUNCTION_KNOB_PARAM, 0.f, 2.f, 0.f, "Function Type ( 0-logistic, 1-tent, 2-parabola )");
		configParam(ITER_KNOB_PARAM, 1.f, 7.f, 1.f, "Number Of Iterations (1 - 7)");
		configParam(R_KNOB_PARAM, 0.f, 10.f, 5.0f, "Bifurcation parameter");
		configParam(CLOCK_KNOB_PARAM, -2.f, 6.f, 2.f, "Clock tempo", " bpm", 2.f, 60.f);
		configParam(TYPE_KNOB_PARAM, 0.f, 3.f, 3.f, "Number of map sections");
		std::memset(outputActivation,false,sizeof(outputActivation));
		std::memset(lastPoints,0.f,sizeof(lastPoints));
		std::memset(plotBuffer,0.f,sizeof(plotBuffer));
	}

	void process(const ProcessArgs& args) override {
		
		if (inputs[EXT_CLOCK_INPUT].isConnected()) {
			// External clock
			if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) {
					computeFlag = true;
				}
			}
		else {
			// Internal clock
			float clockTime = std::pow(2.f, params[CLOCK_KNOB_PARAM].getValue() + inputs[CLOCK_INPUT].getVoltage());
			phase += clockTime * args.sampleTime;
			if (phase >= 1.f) {
				computeFlag = true;
				phase = 0.f;
			}		
		}
		
		if(computeFlag){
			float rValue = clamp((params[R_KNOB_PARAM].getValue() + inputs[R_INPUT].getVoltage())/ 10.f,0.f,1.f);
			currentX = computeFunction(currentX,(int) std::round(params[ITER_KNOB_PARAM].getValue()),(int) std::round(params[FUNCTION_KNOB_PARAM].getValue()),rValue);
			plotBuffer[plotIndex] = currentX;
			if(plotIndex < plotBufferSize)
				plotIndex++;
			else
				plotIndex = 0;
			reorderLastPoints(currentX);
			int mapIndex = (int) std::round(params[TYPE_KNOB_PARAM].getValue());
			for(int i = 0; i < 8; i++){
				outputActivation[i] = setActiveIndex(i, 1.f - currentX, (int)std::pow(2,mapIndex));
				if(outputActivation[i]){
					float cv = setCVOutput(i, 1.f - currentX, (int)std::pow(2,mapIndex));
					outputs[CV_OUTPUT + i].setVoltage(cv * 10.f);
				} 
				// else {
				// 	outputs[CV_OUTPUT + i].setVoltage(0.f);
				// }
				outputs[GATE_OUTPUT + i].setVoltage(outputActivation[i] ? 10.f : 0.f);
			}
			computeFlag = false;
		}
		for(int i = 0; i < 8; i++){
			if(tapTriggers[i].process(outputActivation[i])){
				pulseGenerators[i].trigger(1e-3f);	
			}
			bool pulse = pulseGenerators[i].process(args.sampleTime);
			outputs[TRIG_OUTPUT + i].setVoltage(pulse ? 10.f : 0.f);
			lights[OUTPUT_LIGHTS + i].setSmoothBrightness(outputActivation[i], args.sampleTime);
		}
		
	}

	void reorderLastPoints(float newValue){
		for(int i = 0; i < 4; i++){
			lastPoints[i] = lastPoints[i+1];
		}
		lastPoints[4] = newValue;
	}

	float computeFunction(float x, int nIter, int type, float rValue){
		float y = 0.f;
		float xn;
		float rBase;
		float rMax;
		xn = x;
		for(int i = 0; i < nIter; i++){
			switch (type) {
				case LOGISTIC:
					rBase = 2.5f;
					rMax = 3.99f;
					y = (rBase + (rValue*(rMax - rBase)))*(xn - std::pow(xn,2));
				break;
				case TENT:
					rBase = 1.0f;
					rMax = 1.99f;
					if(xn < 0.5f){
						y = (rBase + (rValue*(rMax - rBase)))*xn;
					} else {
						y = (rBase + (rValue*(rMax - rBase)))*(1.f - xn);
					}
				break;
				case PARABOLA:
					rBase = 1.0f;
					rMax = 1.99f;
					y = std::pow((rBase + (rValue*(rMax - rBase)))*(xn - 0.5f),2);
				break;
				default:
					rBase = 2.5f;
					rMax = 3.99f;
					y = (rBase + (rValue*(rMax - rBase)))*(xn - std::pow(xn,2));

			}
			xn = y;
		}
		return y;
	}

	float setCVOutput(int i, float yValue, int mapType){
		float yRescale = 0.f;
		if(mapType != 1){
			for(int j = 0; j < mapType; j++){
				if(yValue >= (j*(1.f/mapType)) && yValue < ((j + 1)*(1.f/mapType)) && (i >= (j*8/mapType)) && (i < ((j + 1)*8/mapType))){
					yRescale = yValue - (j*(1.f/mapType));
					yRescale *= mapType;
				}
			}
		} else {
			yRescale = yValue;
		}
		return yRescale;
	}

	bool setActiveIndex(int i, float yValue, int mapType){
		bool isActive = false;
		if(mapType != 1){
			for(int j = 0; j < mapType; j++){
				if(yValue >= (j*(1.f/mapType)) && yValue < ((j + 1)*(1.f/mapType)) && (i >= (j*8/mapType)) && (i < ((j + 1)*8/mapType)))
					isActive = true;
			}
		} else {
			isActive = true;
		}
		return isActive;
	}
};

struct BifurcationDisplay : TransparentWidget {
	TheBifurcator* module;

	BifurcationDisplay() {}

	void drawPoint(const DrawArgs& args, float yValue){
		
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		nvgFillColor(args.vg, nvgRGBAf(0.9f, 0.9f, 0.9f, 1.f));

		if(module->plotIndex > 0){
			for(int i = 0; i < module->plotIndex; i++){	
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, (float)i, module->plotBuffer[i]*(box.size.y-1), 1.f);
				nvgClosePath(args.vg);
				nvgFill(args.vg);
			}
		}

		nvgFillColor(args.vg, nvgRGBAf(0.0f, 1.f, 0.f, 1.f));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, (float)(module->plotIndex), yValue*(box.size.y-1), 3.f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);
		
	}

	void drawLines(const DrawArgs& args, int numberOfLines) {
		
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
	
		if(numberOfLines < 2)
			return;

		nvgStrokeColor(args.vg, nvgRGBAf(0.6f, 0.6f, 0.6f, 1.f));
		nvgBeginPath(args.vg);
		for(int i = 1; i < numberOfLines; i++)
		{
			nvgMoveTo(args.vg, box.size.x, box.size.y*i/numberOfLines);
			nvgLineTo(args.vg, 0, box.size.y*i/numberOfLines);
		}
		nvgClosePath(args.vg);
		nvgStroke(args.vg);

	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (!module)
			return;
		
		if (layer != 1)
			return;

		int mapIndex = (int) std::round(module->params[TheBifurcator::TYPE_KNOB_PARAM].getValue()); 
		drawLines(args, (int) std::pow(2,mapIndex));
		drawPoint(args, module->currentX);
	}
};

struct CobwebDisplay : TransparentWidget {
	TheBifurcator* module;
	float xprev = 0.5f;
	float xcurr = 0.5f;
	float xpoints[5];

	CobwebDisplay() {
		std::memset(xpoints,0.f,sizeof(xpoints));
	}

	void drawFunction(const DrawArgs& args, float rValue, int type, int iter) {

		float y = 0.f;
		float yp = 0.f;
		float rBase;
		float rMax;
		float r;

		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		nvgStrokeColor(args.vg, nvgRGBAf(0.9f, 0.9f, 0.9f, 1.f));
		nvgStrokeWidth(args.vg, 1.f);
		nvgBeginPath(args.vg);
		for(int i = 0; i < box.size.x-1; i++)
		{
			float x = i/(box.size.x-1);
			float xp = (i+1)/(box.size.x-1);
			
			for(int j = 0; j < iter; j++){
				switch(type){
					case 0:
						rBase = 2.5f;
						rMax = 3.99f;
						r = (rBase + (rValue*(rMax - rBase)));
						y = r*(x - std::pow(x,2));
						yp = r*(xp - std::pow(xp,2));
					break;
					case 1:
						rBase = 1.0f;
						rMax = 1.99f;
						r = (rBase + (rValue*(rMax - rBase)));
						if(x < 0.5f){
							y = r*x;
						} else {
							y = r*(1.f - x);
						}
						if(xp < 0.5f){
							yp = r*xp;
						} else {
							yp = r*(1.f - xp);
						}
					break;
					case 2:
						rBase = 1.0f;
						rMax = 1.99f;
						r = (rBase + (rValue*(rMax - rBase)));
						y = std::pow(r*(x - 0.5f),2);
						yp = std::pow(r*(xp - 0.5f),2);

					break;
					default:
						rBase = 2.5f; 
						rMax = 3.99f;
						r = (rBase + (rValue*(rMax - rBase)));
						y = r*(x - std::pow(x,2));
						yp = r*(xp - std::pow(xp,2));
				}
				x = y;
				xp = yp;
			}

			nvgMoveTo(args.vg, i, (1 - y)*(box.size.y-1));
			nvgLineTo(args.vg, (i+1), (1 - yp)*(box.size.y-1));
			
		}

		nvgClosePath(args.vg);
		nvgStroke(args.vg);

		nvgFillColor(args.vg, nvgRGBAf(0.0f, 1.f, 0.f, 1.f));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, xprev*box.size.x, (1.f - xcurr)*box.size.y, 3.f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);

		nvgStrokeColor(args.vg, nvgRGBAf(0.85f, 0.85f, 0.85f, 1.f));

		nvgStrokeWidth(args.vg, 0.35f);
		nvgBeginPath(args.vg);
	
		for(int i = 0; i < 4; i++){
			nvgMoveTo(args.vg, box.size.x*xpoints[i], box.size.y);
			nvgLineTo(args.vg, box.size.x*xpoints[i], box.size.y*(1.f - xpoints[i+1]));
			nvgMoveTo(args.vg, box.size.x*xpoints[i], box.size.y*(1.f - xpoints[i+1]));
			nvgLineTo(args.vg, box.size.x*xpoints[i+1], box.size.y*(1.f - xpoints[i+1]));
		}

		nvgClosePath(args.vg);
		nvgStroke(args.vg);

		
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (!module)
			return;

		if (layer != 1)
			return;

		int functionType = (int) std::round(module->params[TheBifurcator::FUNCTION_KNOB_PARAM].getValue());
		int nIter = (int) std::round(module->params[TheBifurcator::ITER_KNOB_PARAM].getValue());
		float rValue = clamp((module->params[TheBifurcator::R_KNOB_PARAM].getValue() + module->inputs[TheBifurcator::R_INPUT].getVoltage())/ 10.f,0.f,1.f);
		xprev = module->lastPoints[3];
		xcurr = module->currentX;
		for(int i = 0; i < 5; i++){
			xpoints[i] = module->lastPoints[i];
		}
		drawFunction(args, rValue,functionType,nIter);
	}
};



struct TheBifurcatorWidget : ModuleWidget {
	TheBifurcatorWidget(TheBifurcator* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TheBifurcator.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			BifurcationDisplay* display = new BifurcationDisplay();
			display->module = module;
			display->box.pos = mm2px(Vec(48, 20));
			display->box.size = mm2px(Vec(65, 98));
			addChild(display);
		}

		{
			CobwebDisplay* cobDisplay = new CobwebDisplay();
			cobDisplay->module = module;
			cobDisplay->box.pos = mm2px(Vec(7, 19.5));
			cobDisplay->box.size = mm2px(Vec(34, 34));
			addChild(cobDisplay);
		}

		addParam(createParamCentered<AxiomaSnapKnob>(mm2px(Vec(15.0, 63.5)), module, TheBifurcator::FUNCTION_KNOB_PARAM));
		addParam(createParamCentered<AxiomaSnapKnob>(mm2px(Vec(35.0, 63.5)), module, TheBifurcator::ITER_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(33.5, 83.5)), module, TheBifurcator::R_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(39.0, 95.0)), module, TheBifurcator::CLOCK_KNOB_PARAM));
		addParam(createParamCentered<AxiomaSnapKnob>(mm2px(Vec(34.75, 116.0)), module, TheBifurcator::TYPE_KNOB_PARAM));

		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(22.0, 83.5)), module, TheBifurcator::R_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(27.5, 95.5)), module, TheBifurcator::CLOCK_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(17.5, 95.5)), module, TheBifurcator::EXT_CLOCK_INPUT));

		static const float delta = 12.f;
		static const float topYposition = 101.1f;
		for (int i = 0; i < 8; i++) {
			addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(115.f, topYposition - (i*delta) + 8.5f)), module, TheBifurcator::OUTPUT_LIGHTS + i));
			addOutput(createOutput<AxiomaPort>(mm2px(Vec(123.f, topYposition - (i*delta) + 5.f)), module, TheBifurcator::CV_OUTPUT + i));
			addOutput(createOutput<AxiomaPort>(mm2px(Vec(135.f, topYposition - (i*delta) + 5.f)), module, TheBifurcator::GATE_OUTPUT + i));
			addOutput(createOutput<AxiomaPort>(mm2px(Vec(147.f, topYposition - (i*delta) + 5.f)), module, TheBifurcator::TRIG_OUTPUT + i));
		}

	}
};


Model* modelTheBifurcator = createModel<TheBifurcator, TheBifurcatorWidget>("TheBifurcator");
