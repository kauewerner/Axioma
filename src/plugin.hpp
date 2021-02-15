#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelTheBifurcator;
extern Model* modelTesseract;
extern Model* modelIkeda;



//// ================= KNOBS ============================ ////

struct AxiomaKnob : RoundKnob {
    AxiomaKnob() {
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AxiomaKnob.svg")));
    }
};

struct AxiomaSnapKnob : RoundKnob {
	AxiomaSnapKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AxiomaKnob.svg")));
        snap = true;
	}
};

//// ================= PORTS ============================ ////

struct AxiomaPort : SvgPort {
    AxiomaPort() {
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/AxiomaPort.svg")));
    }
};