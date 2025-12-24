#include <iostream>
#include <fstream>
#include "json.hpp"
#include "petriclasses.h"



using json = nlohmann::json;  // alias for nlohmann::json


#include <sys/stat.h> // Required for checking if folder exists

// Helper to find the correct base path


void getPetri(P_RCPSP::PetriExample& petriExample, int group, int exam, const std::string &instanceType = "j30") {
    petriExample.reset();
    petriExample.place_name_to_id.clear(); // Clear the map!


    std::string subFolder;
    if (group == -1) {
        subFolder = "smallData";
    } else {
        subFolder = instanceType + std::to_string(group) + "_" + std::to_string(exam);
    }

    // 2. Define the two possible paths
    // Option A: "json_outputs_j30/j3016_1/petri.json" (Console)
    std::string path1 = "json_outputs_" + instanceType + "/" + subFolder + "/petri.json";

    // Option B: "HOG2/RCPSP/json_outputs_j30/j3016_1/petri.json" (Green Button)
    std::string path2 = "HOG2/RCPSP/" + path1;

    // 3. Try to open Option A
    std::ifstream input_file(path1);

    // 4. If A failed, try Option B
    if (!input_file.is_open()) {
        input_file.clear(); // Reset error flags
        input_file.open(path2);
    }

    // 5. Final Check
    if (!input_file.is_open()) {
        std::cerr << "❌ Failed to open file!" << std::endl;
        std::cerr << "   Tried: " << path1 << std::endl;
        std::cerr << "   Tried: " << path2 << std::endl;
        return;
    }



    json j;
    try {
        input_file >> j;
    } catch (json::exception& e) {
        std::cerr << "JSON Parsing Failed: " << e.what() << std::endl;
        return;
    }

    // --- 2. READ METADATA ---
    int placeSize = j[0];
    int placedictSize = j[placeSize+1];
    int tranSize = j[placedictSize+1+placeSize+1];
    // int trandictSize = j[tranSize+1+placedictSize+1+placeSize+1]; // Unused

    // --- 3. LOAD PLACES (Build ID Map) ---
    for (int i = 1; i <= placeSize; i++) {
        std::string pName = j[i]["name"];
        petriExample.place_name_to_id[pName] = petriExample.places.size();

        // 1. Load State
        std::vector<std::vector<int>> state;
        if (j[i].contains("state") && j[i]["state"].is_array()) {
            state = j[i]["state"].get<std::vector<std::vector<int>>>();
        } else {
            state.push_back({0});
        }

        // 2. LOAD ARCS (The Fix!)
        // We must load these so .empty() works correctly later
        std::unordered_map<std::string, int> arcsIn;
        std::unordered_map<std::string, int> arcsOut;

        if (j[i]["arcs_in"].is_object()) {
            // Use unordered_map here too
            arcsIn = j[i]["arcs_in"].get<std::unordered_map<std::string, int>>();
        }
        if (j[i]["arcs_out"].is_object()) {
            // Use unordered_map here too
            arcsOut = j[i]["arcs_out"].get<std::unordered_map<std::string, int>>();
        }

        // 3. Create Place with REAL arcs
        P_RCPSP::Place place(pName, arcsIn, arcsOut, state, j[i]["duration"]);
        petriExample.places.push_back(place);
    }
    // ... (Skip Place_dict loop if not strictly needed for the solver) ...

    // --- 4. LOAD TRANSITIONS (Convert Strings to Integers) ---
    // Calculate offset to skip places + metadata
    int offset = placedictSize + 1 + placeSize + 1 + 1;

    for (int i = offset; i < offset + tranSize; i++) {
        // Parse Name and Duration
        std::string tNameStr = j[i]["name"];
        int tName = std::stoi(tNameStr);
        int duration = j[i]["duration"];

        // Create Transition with NEW Constructor (ID, Duration)
        P_RCPSP::Transition tran(tName, duration);

        // OPTIMIZATION: Convert String Arcs to Integer Indices
        if (j[i].contains("arcs_in") && j[i]["arcs_in"].is_object()) {
            for (auto& [key, val] : j[i]["arcs_in"].items()) {
                int placeID = petriExample.place_name_to_id.at(key); // Lookup ID
                int weight = val;
                tran.arcs_in_indices.push_back({placeID, weight});
            }
        }

        if (j[i].contains("arcs_out") && j[i]["arcs_out"].is_object()) {
            for (auto& [key, val] : j[i]["arcs_out"].items()) {
                int placeID = petriExample.place_name_to_id.at(key); // Lookup ID
                int weight = val;
                tran.arcs_out_indices.push_back({placeID, weight});
            }
        }

        petriExample.Transitions.push_back(tran);
    }
}

void getRCPSP(P_RCPSP::RCPSP_example& rcpsp_example,int group,int exam, const std::string &instanceType = "j30") {
    // Open the file for reading
    rcpsp_example.reset();

    std::string subFolder;
    if (group == -1) {
        subFolder = "smallData";
    } else {
        subFolder = instanceType + std::to_string(group) + "_" + std::to_string(exam);
    }

    // 2. Define the two possible paths
    // Option A: "json_outputs_j30/j3016_1/petri.json" (Console)
    std::string path1 = "json_outputs_" + instanceType + "/" + subFolder + "/rcpsp.json";

    // Option B: "HOG2/RCPSP/json_outputs_j30/j3016_1/petri.json" (Green Button)
    std::string path2 = "HOG2/RCPSP/" + path1;

    // 3. Try to open Option A
    std::ifstream input_file(path1);

    // 4. If A failed, try Option B
    if (!input_file.is_open()) {
        input_file.clear(); // Reset error flags
        input_file.open(path2);
    }

    // 5. Final Check
    if (!input_file.is_open()) {
        std::cerr << "❌ Failed to open file!" << std::endl;
        std::cerr << "   Tried: " << path1 << std::endl;
        std::cerr << "   Tried: " << path2 << std::endl;
        return;
    }
    // Create JSON object
    json j;

    // Read into the JSON object
    input_file >> j;

    // Check if the JSON is an array and contains at least one part
    if (j.is_array() && j.size() > 0) {
        // Print the names of the activities in Part 1 (first 32 activities)
int size= j.size();
        // Loop over the first 32 elements (0-31)
        for (int i = 0; i < j.size()-4 && i < j.size(); ++i) {
            const auto& activity1 = j[i];
            P_RCPSP::Activity activity(activity1["duration"], activity1["name"], activity1["resource_demands"]);
            rcpsp_example.addActivity(activity);
            // Check if the "name" and "duration" fields exist in the current activity
            if (activity1.contains("name") && activity1.contains("duration")) {


                // Print resource demands, if they exist
                if (activity1.contains("resource_demands") && !activity1["resource_demands"].empty()) {

                    for (auto& demand : activity1["resource_demands"].items()) {
                        //std::cout << "  " << demand.key() << ": " << demand.value() << std::endl;
                    }
                } else {
                    //std::cout << "No resource demands." << std::endl;
                }
            }
            //std::cout << std::endl;  // Space between activities
        }

    } else {
        //std::cerr << "The JSON structure is invalid or does not have enough parts." << std::endl;
    }
    rcpsp_example.dependencies.resize(j.size()-4);
    rcpsp_example.backword_dependencies.resize(j.size()-4);
    std::vector<std::pair<int, json>> sorted32;
    for (const auto& item : j[j.size()-4].items()) {
        sorted32.push_back({std::stoi(item.key()), item.value()});
    }

    // Sort by the integer value of the keys
    std::sort(sorted32.begin(), sorted32.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    // Print the sorted keys and their associated values - CONVERT STRINGS TO INTS
    for (const auto& [key, value] : sorted32) {
        for (const auto& val : value) {
            // Handle both string and int from JSON
            if (val.is_string()) {
                rcpsp_example.backword_dependencies[key-1].push_back(std::stoi(val.get<std::string>()));
            } else if (val.is_number_integer()) {
                rcpsp_example.backword_dependencies[key-1].push_back(val.get<int>());
            }
        }
    }

    std::vector<std::pair<int, json>> sorted33;
    for (const auto& item : j[j.size()-3].items()) {
        sorted33.push_back({std::stoi(item.key()), item.value()});
    }

    // Sort by the integer value of the keys
    std::sort(sorted33.begin(), sorted33.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    // Print the sorted keys and their associated values - CONVERT STRINGS TO INTS
    for (const auto& [key, value] : sorted33) {
        for (const auto& val : value) {
            // Handle both string and int from JSON
            if (val.is_string()) {
                rcpsp_example.dependencies[key-1].push_back(std::stoi(val.get<std::string>()));
            } else if (val.is_number_integer()) {
                rcpsp_example.dependencies[key-1].push_back(val.get<int>());
            }
        }
    }

    // Close the file after reading
    //std::cout << "Current size of dependencies: " << rcpsp_example.dependencies.size() << std::endl;

    const auto& resources_json = j[j.size()-1];  // Get the resources part

    // Loop through and add resources individually to the class
    for (auto& [key, value] : resources_json.items()) {
        // Assuming the value is always an integer (resources are integers)
        rcpsp_example.addResource(key, value.get<int>());
    }
    const auto& deep_set_array = j[j.size() - 2];

    if (deep_set_array.is_array()) {
        for (const auto& pair : deep_set_array) {
            if (pair.is_array() && pair.size() == 2) {
                std::string from = pair[0];
                std::string to = pair[1];
                rcpsp_example.deep_dependencies.insert({from, to});
            } else {
                std::cerr << "Malformed deep dependency pair in JSON." << std::endl;
            }
        }
    } else {
        std::cerr << "Expected an array for deep dependencies." << std::endl;
    }
    return;
}