#include "plugin.hpp"


struct Ikeda : Module {
	enum ParamIds {
		U_KNOB_PARAM,
		T_KNOB_PARAM,
		CLOCK_KNOB_PARAM,
		X_LEVEL_PARAM,
		Y_LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		U_INPUT,
		T_INPUT,
		EXT_CLOCK_INPUT,
		INT_CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_CV_OUTPUT,
		Y_CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float phase = 0.f;
	float currentX = 0.f;
	float currentY = 0.f;
	float currentU = 0.f;
	float currentT = 0.f;
	float u = 0.6f;
	float tt = 1.f;
	const float mapRange = 2.f;
	
	float plotBufferX[1000];
	float plotBufferY[1000];
	const int plotBufferSize = 1000;

	bool computeFlag = false;

	dsp::BooleanTrigger tapTrigger;
	dsp::PulseGenerator pulseGenerator;
	dsp::SchmittTrigger clockTrigger;

	Ikeda() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(U_KNOB_PARAM, 0.6f, 0.9f, 0.6f, "Parameter u");
		configParam(T_KNOB_PARAM, 0.25f, 3.5f, 1.f, "Time weight");
		configParam(CLOCK_KNOB_PARAM, -2.f, 6.f, 2.f, "Clock tempo", " bpm", 2.f, 60.f);
		configParam(X_LEVEL_PARAM, 0.f, 10.f, 5.f, "X CV Level", " V");
		configParam(Y_LEVEL_PARAM, 0.f, 10.f, 5.f, "Y CV Level", " V");
		std::memset(plotBufferX,0.f,sizeof(plotBufferX));
		std::memset(plotBufferY,0.f,sizeof(plotBufferY));
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
			float clockTime = std::pow(2.f, params[CLOCK_KNOB_PARAM].getValue() + inputs[INT_CLOCK_INPUT].getVoltage());
			phase += clockTime * args.sampleTime;
			if (phase >= 1.f) {
				computeFlag = true;
				phase = 0.f;
			}		
		}
		
		u = clamp((params[U_KNOB_PARAM].getValue() +  (inputs[U_INPUT].getVoltage() / 20.f) ), 0.6f, 0.9f);
		tt = clamp((params[T_KNOB_PARAM].getValue() +  (inputs[T_INPUT].getVoltage() / 2.f) ), 0.25f, 3.5f);

		if(computeFlag){
			
			computeIkedaPoint();
			if(currentU == u && currentT == tt)
				updatePlotBuffer(currentX,currentY);
			else {
				restartPlotBuffer(currentX,currentY);
				currentU = u;
				currentT = tt;
			}

			outputs[X_CV_OUTPUT].setVoltage( clamp(params[X_LEVEL_PARAM].getValue()*(currentX + 0.75*mapRange)/(2 * mapRange),0.f,10.f));
			outputs[Y_CV_OUTPUT].setVoltage( clamp(params[Y_LEVEL_PARAM].getValue()*(1.f - (currentY + mapRange)/(2 * mapRange)),0.f,10.f));

			computeFlag = false;
		}

		
	}

	void updatePlotBuffer(float newXValue, float newYValue){
			for(int i = 0; i < plotBufferSize-1; i++){
			plotBufferX[i] = plotBufferX[i+1];
			plotBufferY[i] = plotBufferY[i+1];
		}
		plotBufferX[plotBufferSize-1] = newXValue;
		plotBufferY[plotBufferSize-1] = newYValue;
	}

	void restartPlotBuffer(float newXValue, float newYValue){
			for(int i = 0; i < plotBufferSize-1; i++){
			plotBufferX[i] = 0.f;
			plotBufferY[i] = 0.f;
		}
		plotBufferX[plotBufferSize-1] = newXValue;
		plotBufferY[plotBufferSize-1] = newYValue;
	}

	void computeIkedaPoint(){
		float x0 = currentX;
		float y0 = currentY;
		float t = (0.4 - (6/(1 + x0*x0 + y0*y0)))*tt;
		
		currentX = 1 + u*(x0*cos(t) - y0*sin(t));
    	currentY = u*(x0*std::sin(t) + y0*std::cos(t));
	}
};

struct IkedaDisplay : TransparentWidget {
	Ikeda* module;
	float centerX;
	float centerY;

	IkedaDisplay() {
		box.pos = mm2px(Vec(3.5, 6.75));
		box.size = mm2px(Vec(48.9, 48.9));
		centerX = box.pos.x + (box.size.x / 2.f);
		centerY = box.pos.y + (box.size.y / 2.f);
	}

	void drawPoint(const DrawArgs& args, float xValue, float yValue){
		
		nvgScissor(args.vg, box.pos.x, box.pos.y, box.size.x, box.size.y);

		nvgFillColor(args.vg, nvgRGBAf(0.98f, 0.98f, 0.98f, 1.f));
		for(int i = 0; i < module->plotBufferSize; i++){	
			if(module->plotBufferX[i] != 0.f && module->plotBufferY[i] != 0.f){
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, 3*centerX/4.f + module->plotBufferX[i]*box.size.y/4.f, 1.25*centerY + module->plotBufferY[i]*box.size.y/4.f, 1.f);
				nvgClosePath(args.vg);
				nvgFill(args.vg);
			}
		}


		nvgFillColor(args.vg, nvgRGBAf(0.f, 0.98f, 0.f, 1.f));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, 3*centerX/4.f + xValue*box.size.y/4.f, 1.25*centerY + yValue*box.size.y/4.f, 2.f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);

		// nvgFillColor(args.vg, nvgRGBAf(0.0f, 1.f, 0.0f, 0.25f));
		// nvgBeginPath(args.vg);
		// nvgRect(args.vg,box.pos.x, box.pos.y, box.size.x, box.size.y);
		// nvgClosePath(args.vg);
		// nvgFill(args.vg);
		
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (!module)
			return;

		if (layer != 1)
			return;
		
		drawPoint(args, module->currentX,module->currentY);
	}
};

struct IkedaWidget : ModuleWidget {
	IkedaWidget(Ikeda* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ikeda.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			IkedaDisplay* display = new IkedaDisplay();
			display->module = module;
			addChild(display);
		}

		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(24.0, 74.5)), module, Ikeda::U_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(48.0, 74.5)), module, Ikeda::T_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(48.0, 89.5)), module, Ikeda::CLOCK_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(24.0, 106.0)), module, Ikeda::X_LEVEL_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(37.0, 106.0)), module, Ikeda::Y_LEVEL_PARAM));

		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(11.0, 74.5)), module, Ikeda::U_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(36.0, 74.5)), module, Ikeda::T_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(26.174, 89.5)), module, Ikeda::EXT_CLOCK_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(35.299, 89.5)), module, Ikeda::INT_CLOCK_INPUT));
		addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(11.0, 106.0)), module, Ikeda::X_CV_OUTPUT));
		addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(50.0, 106.0)), module, Ikeda::Y_CV_OUTPUT));
	}
};


Model* modelIkeda = createModel<Ikeda, IkedaWidget>("Ikeda");