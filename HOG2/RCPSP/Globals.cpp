//
// Created by idolu on 27/04/2026.
//
// Globals.cpp
#include "Globals.h"

PetriExample petri;
RCPSP_example RCPSPex;
short g_num_activities = 0;
short g_sink_id = 0;
std::string finalstatename;
std::string initialstatename;

std::vector<std::vector<short>> downstream;
std::vector<std::vector<short>> upstream;
std::vector<ResourceInfo> resource_info;

CBSConfig setting;
bool allcorrect = true;
long debug_cardinal_num = 0;