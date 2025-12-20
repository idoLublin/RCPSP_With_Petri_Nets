//
// Created by idol on 29/12/2024.
//
// Your First C++ Program

#include <iostream>
 #include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"

#include "RCPSP.h"
//****importent i changed GLUtil.h with recVec == operator abit****//
 //PetriExample petri;
 //RCPSP_example RCPSP1;
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
// #include <windows.h>
// #include <omp.h> // Include this at the top of Driver.cpp
#include <fstream>
#include <vector>
#include <climits>
void runBenchmark();
void runSolvedProblems();
void sortCSV(const std::string& filename);
std::atomic<bool> cancel_requested(false);

std::atomic<bool> stop_printing1(false); // Flag to stop the printing thread

// void printNetworkSize1() {
//     while (!stop_printing1) {
//         std::this_thread::sleep_for(std::chrono::seconds(60*5)); // Wait for a second
//     }
// }
int solveRCPSP();
int solveRCPSP_TT();
int solveRCPSP_Bi();
#include <iostream>
#include <fstream>
#include <future>
#include <chrono>
#include <vector>
#include <thread>
#include <thread>
#include <atomic>
namespace fs = std::filesystem;

int solveRCPSP(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;

    generateTIME= std::chrono::duration<double>(0);
    avelableTIME= std::chrono::duration<double>(0);
    HTIME= std::chrono::duration<double>(0);
    hashTIME= std::chrono::duration<double>(0);
    comperTime= std::chrono::duration<double>(0);
    secssesorTIME= std::chrono::duration<double>(0);
    clock_t setupTIME = clock();

    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPex.computeAndStoreDeepDependencies();

    RCPSPState first;
    RCPSPState last = first;
    last.h = 0;

    for (int i = 0; i < last.marking.size(); ++i) {
        if (last.marking[i] == 1) {
            last.marking[i] = 0;
        }
    }

    // 2. Set the Goal State
    // Use the map we built to translate "FinalStateName" -> Integer ID
    // Then set that specific index to 1.
    int finalID = petri.place_name_to_id.at(finalstatename);
    last.marking[finalID] = 1;

    RCPSP as1;
    TemplateAStar<RCPSPState, int, RCPSP> astar;
    std::vector<RCPSPState> path;


    bool finished = false;
    bool timeout_occurred = false;
    std::chrono::duration<double> elapsed;


    clock_t setupend = clock();





    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        for (const auto& state : path) {
            std::cout << "g: " << state.g << std::endl;

            std::cout << "active: ";
            for (const auto& [transIdx, duration] : state.activeTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl;

            // std::cout << "available: ";
            // for (int transIdx : state.avilableTransitionIndices)
            //     std::cout << " " << transIdx;
            // std::cout << std::endl << std::endl;

            makespan = state.g;
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TP"<< ","
         << problemType<< ","
         << (useCS ? "True" : "False")<< ","
       //  << "\n";
         << 100 * generateTIME.count() / elapsed.count() << ","
         << generateTIME.count() / astar.GetNodesTouched() << ","
         << 100 * avelableTIME.count() / elapsed.count() << ","
         << avelableTIME.count() / astar.GetNodesTouched() << ","
         << 100 * hashTIME.count() / elapsed.count() << ","
         << hashTIME.count() / astar.GetNodesTouched() << ","
         << 100 * HTIME.count() / elapsed.count() << ","
         << HTIME.count() / count<< ","
        << 100 * comperTime.count() / elapsed.count() << ","
         << comperTime.count() / astar.GetNodesTouched() << ","
         << 100 * secssesorTIME.count() / elapsed.count() << ","
         << secssesorTIME.count() / count<< ","
         << "\n";





    return 0;
}
    int solveRCPSP_TT(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;
    count=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPState_TT first;
    RCPSPState_TT last = first;



    RCPSP_TT as1;

    TemplateAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    std::vector<RCPSPState_TT> path;


    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);

    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;

        //for (const auto& state : path) {
        RCPSPState_TT state=path.back();
            //std::cout << "g: " << state.g;

            for (const auto& [actId, startTime] : state.startedActivitiys) {
                std::cout << actId << ":" << startTime << " ";
            }

            std::cout << std::endl;
            makespan = state.g;
        //}

        std::cout << "\nFinal makespan: " << makespan << std::endl;
    }
     else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
        << "TT"<< ","
        << problemType<< ","
         << (useCS ? "True" : "False")<< ","
    << 100 * generateTIME.count() / elapsed.count() << ","
<< generateTIME.count() / astar.GetNodesTouched() << ","
<< 100 * avelableTIME.count() / elapsed.count() << ","
<< avelableTIME.count() / astar.GetNodesTouched() << ","
<< 100 * hashTIME.count() / elapsed.count() << ","
<< hashTIME.count() / astar.GetNodesTouched() << ","
<< 100 * HTIME.count() / elapsed.count() << ","
<< HTIME.count() / count
<< 100 * comperTime.count() / elapsed.count() << ","
<< comperTime.count() / astar.GetNodesTouched() << ","
<< 100 * secssesorTIME.count() / elapsed.count() << ","
<< secssesorTIME.count() / count
         << "\n";

    return 0;
}
//not working
int solveRCPSP_Bi(int group, int exam, const std::string& filename) {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;

  //  generateTIME= std::chrono::duration<double>(0);
    //avelableTIME= std::chrono::duration<double>(0);
   // hashTIME= std::chrono::duration<double>(0);
  //  comperTime= std::chrono::duration<double>(0);
    //secssesorTIME= std::chrono::duration<double>(0);
    count=0;

    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    RCPSPState_bi first;
    first.direction=true;
    count=2;
    RCPSPState_bi last = first;
    last.direction=false;
    last.h_b = first.h_f;
    last.h_f = 0;
last.name=1;
    for (auto& pair : last.marking) {
        //set only to to 4 diffrent resources
        if (pair.first=="R1"){continue;}
        if (pair.first=="R2"){continue;}
        if (pair.first=="R3"){continue;}
        if (pair.first=="R4"){continue;}
        if (pair.second == 1) { pair.second = 0; }
        if (pair.first == finalstatename) { pair.second = 1; }
    }
    last.avilableDeTransitionIndices=getAvilableDetransitionIndices(last.marking);
    //last.avilableTransitionIndices=getAvilableTransitionIndices(last.marking);
    for (int i=1;i<petri.Transitions.size()+1;i++) {
        last.finishedActivitiys.insert(i);
        last.startedActivitiys.insert(i);
    }
    last.unstartedTransitions.clear();
    std::vector<RCPSPState_bi> path;
    ForwardRCPSPHeuristic H_F;
    BackwardRCPSPHeuristic H_B;
    RCPSP_BiGreedy bs1;


    BAE<RCPSPState_bi, int, RCPSP_BiGreedy> Bi_RCPSP;



    bool finished = false;
    bool timeout_occurred = false;
    std::chrono::duration<double> elapsed;

    // Create a flag for thread completion

    auto start = std::chrono::high_resolution_clock::now();
    Bi_RCPSP.GetPath(&bs1, first, last,&H_F,&H_B ,path);



    // Record end time and calculate elapsed time
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;


    // Output results
    double max_f = 0;
    double max_b = 0;

    if (!path.empty()) {
        finished=true;
        std::cout << "Path found!" << std::endl;
        for (const auto& state : path) {
            std::cout << "g_f: " << state.g_f << std::endl;
            std::cout << "g_b: " << state.g_b << std::endl;

            std::cout << "active: ";
            for (const auto& [transIdx, duration] : state.activeTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl;

            std::cout << "available: ";
            for (int transIdx : state.avilableTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl << std::endl;

            max_f = std::max(max_f,state.g_f);
            max_b = std::max(max_b,state.g_b);
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << Bi_RCPSP.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << Bi_RCPSP.GetNodesTouched() << std::endl;
    // Save to file
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (finished ? "True" : "False") << ","
         << max_f+max_b << ","
         << Bi_RCPSP.GetNodesExpanded() << ","
        //  << ","<<100*generateTIME.count()/elapsed.count()<< ","<<generateTIME.count()
         //    << ","<<100*avelableTIME.count()/elapsed.count()<< ","<<avelableTIME.count()
          //       << ","<<100*hashTIME.count()/elapsed.count()<< ","<<hashTIME.count()<<
                    // ","<<100*comperTime.count()/elapsed.count()<< ","<<comperTime.count()/astar.GetNodesTouched()<<
             "\n";

    return 0;
}



std::string getNextFilename(const std::string& folder, const std::string& baseName, const std::string& extension) {
    // Ensure folder exists
    if (!fs::exists(folder)) {
        fs::create_directories(folder);
    }

    int count = 1;
    std::string newFilename;

    do {
        newFilename = folder + "/" + baseName + std::to_string(count) + extension;
        count++;
    } while (fs::exists(newFilename)); // Ensure unique filename

    return newFilename;
}

// --- DELETE the entire parallel/sequential loop from your main() ---
// --- Replace main() with this simplified structure ---

int main(int argc, char *argv[]) {
    // Expects one argument: The starting Group ID for this batch (e.g., 1, 3, 5, etc.)
    if (argc != 3) {
        std::cerr << "Usage: ./Driver <START_GROUP_ID>" << std::endl;
        return 1;
    }

    int startGroup = std::stoi(argv[1]);
    std::string outputFolder = argv[2]; // Get folder from script
    // This single execution will process TWO groups sequentially: G, G+1.
    // G = startGroup (e.g., 1, 3, 5...)
    // G+1 = next group (e.g., 2, 4, 6...)

    // FIX: Use the Group ID directly in the filename
    // This guarantees Job 14 writes to "..._14.csv" and Job 15 writes to "..._15.csv"
    std::string filename = "results/batch_group_" + std::to_string(startGroup) + ".csv";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Error opening file: " << filename << std::endl;
        return 1;
    }
    // Process Group 'G'
    for (int j = 1; j < 11; j++) {
        solveRCPSP(startGroup, j, filename, "j30");

        // Same for TT
        solveRCPSP_TT(startGroup, j, filename, "j30");    }

    // Process Group 'G+1'

    return 0;
}

//  int main() {
//      runBenchmark();
//     return 0;
// }


void runBenchmark() {
    std::string folder = "results";
    std::string baseName = "output_";
    std::string extension = ".csv";

    std::string filename = getNextFilename(folder, baseName, extension);

    // Create and write to file
    std::ofstream file(filename);
    // Open file stream

    // Check if file is open
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    // Write header
    //file << "group,exam,time,finished,makespan,expand number,generated number,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave)" << std::endl;
    file << "group,exam,time,finished,makespan,expand number,generated number,depth,PetriType,SetType,Use CS,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave),hashTime(ave),comperTime%,comperTime(ave),succsesroTime%,sucssesorTime(ave)" << std::endl;
    //file << "group,exam,initialHcost" << std::endl;
 //omp_set_num_threads(10);
     //omp_set_num_threads(2); // 1. Set the core count.
     //#pragma omp parallel for collapse(2) schedule(dynamic)
    for(int i = 1; i < 2; i++) {
        for(int j = 1; j < 11; j++) {

            // 1. CLEAN THE SLATE (Crucial for thread_local variables)
            petri.reset();
            RCPSPex.reset();

            // 2. SOLVE
            solveRCPSP(i, j, filename, "j30");
            //solveRCPSP_TT(i, j, filename, "j30");
        }
    }
    sortCSV(filename);


    // solveRCPSP(-1,-1,filename,"j30");
    //
    // for(int i=16;i<17;i++) {
    //      for(int j=5;j<11;j++) {
    //      solveRCPSP(i,j,filename,"j30");
    //      solveRCPSP_TT(i,j,filename,"j30");
    //   //   getinitialHcost(i,j,filename);
    //      }
    //  }

     //solveRCPSP(34, 9, filename);
  //   solveRCPSP(34, 10, filename);



    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);
    // useCS=false;
    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);
    // useCS=true;
    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);

    useCS=false;
    //solveRCPSP(33,9,filename,"j60");
    //solveRCPSP(33,10,filename,"j60");
     //for(int i=34;i<49;i++) {
       // for(int j=1;j<11;j++) {
      //  solveRCPSP(i,j,filename,"j60");
       // }
   // }
   // solveRCPSP(48,5,filename,"j90");
    //solveRCPSP(48,6,filename,"j90");
    //solveRCPSP(48,7,filename,"j90");
    //solveRCPSP(48,8,filename,"j90");
    //solveRCPSP(48,9,filename,"j90");
    //solveRCPSP(48,10,filename,"j90");

    //for(int i=26;i<49;i++) {
       // for(int j=1;j<11;j++) {
          //  solveRCPSP(i,j,filename,"j90");
       // }
    //}


    // J120 לא רץ

    // for(int i=1;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP(i,j,filename,"j120");
    //     }
    // }

   // for(int i=1;i<49;i++) {
   //     for(int j=1;j<11;j++) {
   //         solveRCPSP_TT(i,j,filename,"j120");
   //     }
    //}


    useCS=true;

//    for(int i=1;i<49;i++) {
  //      for(int j=1;j<11;j++) {
    //        solveRCPSP(i,j,filename,"j90");
      //  }
   // }

   // for(int i=1;i<49;i++) {
       // for(int j=1;j<11;j++) {
           // solveRCPSP(i,j,filename,"j120");
      //  }
   // }

   // for(int i=1;i<49;i++) {
 //       for(int j=1;j<11;j++) {
   //         solveRCPSP_TT(i,j,filename,"j30");
 //       }
   // }

    // for(int i=17;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP_TT(i,j,filename,"j60");
    //     }
    // }

    // for(int i=1;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP_TT(i,j,filename,"j90");
    //     }
    // }

    //for(int i=1;i<49;i++) {
       //for(int j=1;j<11;j++) {
           // solveRCPSP_TT(i,j,filename,"j120");
      //  }
   // }


   //  for(int i=1;i<49;i++) {
   //     for(int j=1;j<11;j++) {
   //     solveRCPSP(i,j,filename);
   //     //getinitialHcost(i,j,filename);
   //     }
   // }


    // for(int i=1;i<49;i++) {
    //      for(int j=1;j<11;j++) {
    //      solveRCPSP_TT(i,j,filename);
    //   //   getinitialHcost(i,j,filename);
    //      }
    //  }

}

struct ResultRow {
    std::string fullLine;
    // Sorting Keys
    std::string petriType; // TP/TT
    std::string setType;   // j30
    int setSize;           // 30 (for sorting)
    int group;
    int exam;
};

void sortCSV(const std::string& filename) {
    std::cout << "🔄 Sorting results..." << std::endl;

    std::ifstream inFile(filename);
    if (!inFile.is_open()) return;

    std::vector<ResultRow> rows;
    std::string line, header;

    // 1. Read Header
    if (getline(inFile, header)) {
        // Ensure we keep the header!
    }

    // 2. Read and Parse Rows
    while (getline(inFile, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string segment;
        ResultRow row;
        row.fullLine = line;
        int colIndex = 0;

        // Parse columns by comma
        while (getline(ss, segment, ',')) {
            // Column 0: Group
            if (colIndex == 0) try { row.group = std::stoi(segment); } catch (...) { row.group = 0; }
            // Column 1: Exam
            else if (colIndex == 1) try { row.exam = std::stoi(segment); } catch (...) { row.exam = 0; }
            // Column 8: PetriType (TP/TT)
            else if (colIndex == 8) row.petriType = segment;
            // Column 9: SetType (j30)
            else if (colIndex == 9) {
                row.setType = segment;
                // Extract integer for sorting (j30 -> 30)
                std::string nums = segment;
                nums.erase(std::remove_if(nums.begin(), nums.end(), [](char c){ return !isdigit(c); }), nums.end());
                try { row.setSize = std::stoi(nums); } catch (...) { row.setSize = 0; }
            }
            colIndex++;
        }
        rows.push_back(row);
    }
    inFile.close();

    // 3. Sort (Priority: PetriType -> SetSize -> Group -> Exam)
    std::sort(rows.begin(), rows.end(), [](const ResultRow& a, const ResultRow& b) {
        if (a.petriType != b.petriType) return a.petriType < b.petriType; // TP vs TT
        if (a.setSize != b.setSize) return a.setSize < b.setSize;         // j30 vs j60
        if (a.group != b.group) return a.group < b.group;                 // 1 vs 2
        return a.exam < b.exam;                                           // 1 vs 2
    });

    // 4. Write Back
    std::ofstream outFile(filename);
    outFile << header << "\n"; // Write original header
    for (const auto& row : rows) {
        outFile << row.fullLine << "\n";
    }

    std::cout << "✅ Sorted " << rows.size() << " rows." << std::endl;
}




void getinitialHcost(int i, int i1, const std::string & string);
void getinitialHcost(int group, int exam, const std::string &filename) {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;
    count=0;
    double initalHcost=0;
    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    RCPSPState first;
    initalHcost=getForwardHcost(first.unstartedTransitions,first.activeTransitionIndices);

    std::cout << "initalHcost\n";



    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << initalHcost<<std::endl;

}

