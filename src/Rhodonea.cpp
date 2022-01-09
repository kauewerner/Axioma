#include "plugin.hpp"


struct Rhodonea : Module {
	enum ParamIds {
		A_KNOB_PARAM,
		D_KNOB_PARAM,
		N_KNOB_PARAM,
		PITCH_KNOB_PARAM,
		ANGLE_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		A_INPUT,
		D_INPUT,
		N_INPUT,
		PITCH_INPUT,
		ANGLE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		Y_OUTPUT,
		X_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float phase = 0.f;
	float phaseK = 0.f;
	float angle;
	float n;
	float d;
	float a;
	
	const float maxIdx = 10.f;
	const float highCutoff = 20.f;

	dsp::BiquadFilter hpf;


	Rhodonea() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(A_KNOB_PARAM, 0.0f, 1.f, 0.f, "a parameter (pure tone)");
		configParam(D_KNOB_PARAM, 1.f, maxIdx, 1.f, "d parameter");
		configParam(N_KNOB_PARAM, 1.f, maxIdx, 2.f, "n parameter");
		configParam(PITCH_KNOB_PARAM, -4.f, 4.f, 0.f, "pitch CV");
		configParam(ANGLE_KNOB_PARAM, 0.f, 2.f * M_PI, 0.f, "angle (phase) CV");

		
	}

	void process(const ProcessArgs& args) override {
		float pitch = params[PITCH_KNOB_PARAM].getValue();
		pitch += inputs[PITCH_INPUT].getVoltage();
		pitch = clamp(pitch, -4.f, 4.f);
		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);

		hpf.setParameters(dsp::BiquadFilter::Type::HIGHPASS, highCutoff / args.sampleRate,1.f,0.f);
		
		n = params[N_KNOB_PARAM].getValue();
		n += inputs[N_INPUT].getVoltage();
		n =  clamp(n, 1.f,maxIdx);

		d = params[D_KNOB_PARAM].getValue();
		d += inputs[D_INPUT].getVoltage();
		d = clamp(d, 1.f,maxIdx);

		a = params[A_KNOB_PARAM].getValue();
		a += (inputs[A_INPUT].getVoltage() / 10.f);
		a = clamp(a, 0.f,1.f);

		angle = params[ANGLE_KNOB_PARAM].getValue();
		angle += (2.f * M_PI * inputs[ANGLE_INPUT].getVoltage() / 10.f);
		angle = clamp(angle ,0.f, 2.f * M_PI);

		phase += freq * args.sampleTime;
		if (phase >= 0.5f)
			phase -= 1.f;

		float k =  n / d;

		phaseK += (k * freq * args.sampleTime);
		if (phaseK >= 0.5f)
			phaseK -= 1.f;

		if(outputs[X_OUTPUT].isConnected()){
			float x =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK ) ) * std::cos(2.f * M_PI * phase + angle);
			// float x =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK + angle) ) * std::cos(2.f * M_PI * phase );
			// float x =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK + angle) ) * std::cos(2.f * M_PI * phase + angle);
			outputs[X_OUTPUT].setVoltage(5.f * hpf.process(x));
		}
		
		if(outputs[Y_OUTPUT].isConnected()){
			float y =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK ) ) * std::sin(2.f * M_PI * phase + angle);
			// float y =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK + angle) ) * std::sin(2.f * M_PI * phase );
			// float y =  (a - (1.f - a) * std::cos(2.f * M_PI * phaseK + angle) ) * std::sin(2.f * M_PI * phase + angle);
			outputs[Y_OUTPUT].setVoltage(5.f * hpf.process(y));
		}
	}
};

struct RhodoneaDisplay : TransparentWidget {
	Rhodonea* module;
	float centerX;
	float centerY;
	int numberOfPoints = 1000;

	RhodoneaDisplay() {
		box.pos = mm2px(Vec(2.0, 6.75));
		box.size = mm2px(Vec(53.0, 53.0));
		centerX = box.pos.x + (box.size.x / 2.f);
		centerY = box.pos.y + (box.size.y / 2.f);
	}

	void drawRose(const DrawArgs& args, float nValue, float dValue, float aValue, float angleValue){
		float k = nValue / dValue;
		float theta = angleValue;
		float scale = 0.45f;
		float dTheta = dValue * 2 * M_PI / numberOfPoints;
		float currentX =  (aValue - (1.f - aValue) * std::cos(k * 2.f * M_PI * (theta - angleValue) ) ) * std::cos(2.f * M_PI * theta);
		float currentY =  (aValue - (1.f - aValue) * std::cos(k * 2.f * M_PI * (theta - angleValue) ) ) * std::sin(2.f * M_PI * theta);

		nvgScissor(args.vg, box.pos.x, box.pos.y, box.size.x, box.size.y);
		nvgStrokeColor(args.vg, nvgRGBAf(0.88f, 0.88f, 0.88f, 0.88f));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX + scale*box.size.x*currentX, centerY - scale*box.size.y*currentY);

		for(int i = 1; i < numberOfPoints; i++)
		{
			theta += dTheta;
			currentX =  (aValue - (1.f - aValue) * std::cos(k * 2.f * M_PI * (theta - angleValue)) ) * std::cos(2.f * M_PI * theta);
			currentY =  (aValue - (1.f - aValue) * std::cos(k * 2.f * M_PI * (theta - angleValue)) ) * std::sin(2.f * M_PI * theta);
			nvgLineTo(args.vg, centerX + scale*box.size.x*currentX, centerY - scale*box.size.y*currentY);
			nvgMoveTo(args.vg, centerX + scale*box.size.x*currentX, centerY - scale*box.size.y*currentY);
		}
		nvgClosePath(args.vg);
		nvgStroke(args.vg);

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

		drawRose(args, module->n, module->d, module->a, module->angle);
	}
};


struct RhodoneaWidget : ModuleWidget {
	RhodoneaWidget(Rhodonea* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Rhodonea.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			RhodoneaDisplay* display = new RhodoneaDisplay();
			display->module = module;
			addChild(display);
		}

		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(48.5, 82.5)), module, Rhodonea::A_KNOB_PARAM));
		addParam(createParamCentered<AxiomaSnapKnob>(mm2px(Vec(30.5, 82.5)), module, Rhodonea::D_KNOB_PARAM));
		addParam(createParamCentered<AxiomaSnapKnob>(mm2px(Vec(12.5, 82.5)), module, Rhodonea::N_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(36.5, 99.5)), module, Rhodonea::PITCH_KNOB_PARAM));
		addParam(createParamCentered<AxiomaKnob>(mm2px(Vec(36.5, 111.5)), module, Rhodonea::ANGLE_KNOB_PARAM));

		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(48.5, 72.0)), module, Rhodonea::A_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(30.5, 72.0)), module, Rhodonea::D_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(12.5, 72.0)), module, Rhodonea::N_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(25.5, 99.5)), module, Rhodonea::PITCH_INPUT));
		addInput(createInputCentered<AxiomaPort>(mm2px(Vec(25.5, 111.5)), module, Rhodonea::ANGLE_INPUT));

		addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(50.25, 100.5)), module, Rhodonea::Y_OUTPUT));
		addOutput(createOutputCentered<AxiomaPort>(mm2px(Vec(50.25, 110)), module, Rhodonea::X_OUTPUT));
	}
};


Model* modelRhodonea = createModel<Rhodonea, RhodoneaWidget>("Rhodonea");