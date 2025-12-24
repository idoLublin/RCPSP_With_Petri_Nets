#ifndef PETRICLASSES_H  // <--- 1. ADD THIS (Check)
#define PETRICLASSES_H

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <set>


namespace P_RCPSP {
class Activity {
public:
    int duration;
    int early_finish;
    int early_start;
    int late_finish;
    int late_start;
    std::string name;
    std::map<std::string, int> resource_demands; // For resources like 'R1': 3
    Activity(): duration(0), early_finish(0), early_start(0), late_finish(0), late_start(0) {
    };
    // Constructor
    Activity(int dur, const std::string& n, const std::map<std::string, int>& resources)
        : duration(dur), name(n), resource_demands(resources), early_finish(0), early_start(0), late_finish(0), late_start(0) {}
    ~Activity() = default;
};

class RCPSP_example{
  public:
    //for better time array insted of vec?
    std::vector<Activity> activities; // List of Activity objects
    int activity_len;
    // Add an activity
    void addActivity(const Activity& activity) {
        activities.push_back(activity);
    }
   //skip on activity name duration as it can be acssesd with acvivity[num].duration
    std::vector<std::vector<std::string>> dependencies;
    std::vector<std::vector<std::string>> backword_dependencies;

    std::vector<std::pair<std::string, int>> resources;
    std::set<std::pair<std::string, std::string>> deep_dependencies;

    // Function to add a single resource to the vector
    void addResource(const std::string& name, int value) {
        resources.push_back({name, value});  // Push as a pair
    }
    ~RCPSP_example() = default;
    RCPSP_example(): activity_len(0) {
    };
    void reset() {
        activities.clear();
        dependencies.clear();
        backword_dependencies.clear();
        resources.clear();
        activity_len = 0;
        deep_dependencies.clear();  // Add this line!

    }
    void computeAndStoreDeepDependencies() {
        deep_dependencies.clear();

        // Map activity names to indices
        std::unordered_map<std::string, int> name_to_index;
        for (int i = 0; i < activities.size(); ++i) {
            name_to_index[activities[i].name] = i;
        }

        for (const auto& activity : activities) {
            for (const auto& target : activities) {
                if (activity.name != target.name) {
                    if (depends_on(activity.name, target.name, name_to_index)) {
                        deep_dependencies.emplace(activity.name, target.name);
                    }
                }
            }
        }
    }
    bool depends_on(const std::string& a_name, const std::string& t_name,
                   const std::unordered_map<std::string, int>& name_to_index) const {
        std::unordered_set<std::string> visited;
        return dfs(a_name, t_name, name_to_index, visited);
    }

    bool dfs(const std::string& current, const std::string& target,
             const std::unordered_map<std::string, int>& name_to_index,
             std::unordered_set<std::string>& visited) const {
        if (current == target)
            return true;
        if (visited.count(current))
            return false;

        visited.insert(current);
        auto it = name_to_index.find(current);
        if (it == name_to_index.end())
            return false;

        int idx = it->second;
        for (const std::string& dep : dependencies[idx]) {
            if (dfs(dep, target, name_to_index, visited))
                return true;
        }

        return false;
    }
    //didnt put activity_names_duration activity_names_set depenedncy_deep_set
};



class Place {
public:
    std::string name;
    std::unordered_map<std::string, int> arcs_in;
    std::unordered_map<std::string, int> arcs_out;
    std::vector<std::vector<int>> state;
    int duration;             // TODO delete

    ~Place() {};
    // Constructor
    Place(const std::string& placeName,
          const std::unordered_map<std::string, int>& inputArcs = {},
          const std::unordered_map<std::string, int>& outputArcs = {},
          const std::vector<std::vector<int>>& initialState = {},
          int initialDuration = 0)
        : name(placeName), arcs_in(inputArcs), arcs_out(outputArcs), state(initialState), duration(initialDuration) {}

};
class Place_dict {
public:
    std::map<std::string, int> arcs_in;  // Arcs coming into the place
    std::map<std::string, int> arcs_out; // Arcs going out from the place
    int duration;                       // Duration of the place
    std::string name;                   // Name of the place
    std::vector<std::vector<int>> state;            // State values

    // Constructor
    ~Place_dict() {};
    Place_dict(const std::string& placeName,
          const std::map<std::string, int>& inputArcs = {},
          const std::map<std::string, int>& outputArcs = {},
          const std::vector<std::vector<int>>& initialState = {},
          int initialDuration = 0)
        : name(placeName), arcs_in(inputArcs), arcs_out(outputArcs), state(initialState), duration(initialDuration) {}
};
class Transition {
public:
    std::vector<std::pair<int, int>> arcs_in_indices;
    std::vector<std::pair<int, int>> arcs_out_indices;
    int duration;                       // Duration of the place
    int name;                   // Name of the place
    // Constructor
    ~Transition() {};
    Transition(int Name, int Duration) : name(Name), duration(Duration) {}

    bool operator==(const Transition& other) const {
        return name == other.name &&
               arcs_in_indices == other.arcs_in_indices &&
               arcs_out_indices == other.arcs_out_indices &&
               duration == other.duration;
    }
};
int getTransitionDuration(const std::vector<Transition>& transitions, const int& name) {
    auto it = std::find_if(transitions.begin(), transitions.end(),
                           [&name](const Transition& t) { return t.name == name; });

    if (it != transitions.end()) {
        return it->duration;  // Return duration if found
    }
    return -1;  // Return -1 or any other value to indicate not found
}
short getTransitionDuration2(const std::vector<std::pair<short, short>>& activeTransitions, int transitionId) {
    for (const auto& [idx, duration] : activeTransitions) {
        if (idx == transitionId) {
            return duration;
        }
    }
    return -1; // Not found
}



class Transition_dict {
public:
    std::map<std::string, int> arcs_in;  // Arcs coming into the place
    std::map<std::string, int> arcs_out; // Arcs going out from the place
    int duration;                       // Duration of the place
    std::string name;                   // Name of the place

    // Constructor
    ~Transition_dict() {};
    Transition_dict(const std::string& Transition_dictName,
          const std::map<std::string, int>& inputArcs = {},
          const std::map<std::string, int>& outputArcs = {},
          int initialDuration = 0)
        : name(Transition_dictName), arcs_in(inputArcs), arcs_out(outputArcs), duration(initialDuration) {}
};
class PetriExample {
public:
  ~PetriExample() = default;
  PetriExample(): place_len(0) {
  };

    std::vector<Place> places;  // Vector of Place objects
   // std::vector<Transition_dict> Transitions_dict;  // Vector of Place objects
    int place_len;
    //std::vector<Place_dict> places_dict;  // Vector of Place objects
    std::vector<Transition> Transitions;  // Vector of Place objects
    std::unordered_map<std::string, int> place_name_to_id;



    // Add a place to the Petri example
    void addPlace(const Place& place) {
        places.push_back(place);
    }


    // void addPlace_dict(const Place_dict& place_dict) {
    //     places_dict.push_back(place_dict);
    // }
    void addTransition(const Transition& transition) {
        Transitions.push_back(transition);
    }
    // void addTransition_dist(const Transition_dict& transition_dict) {
    //     Transitions_dict.push_back(transition_dict);
    // }
    void reset() {
        places.clear();
        //places_dict.clear();
        Transitions.clear();
    //    Transitions_dict.clear();
        place_len = 0;
    }

};


} // <--- END NAMESPACE

#endif // <--- 4. END GUARD (ADD THIS LINE AT THE VERY BOTTOM)