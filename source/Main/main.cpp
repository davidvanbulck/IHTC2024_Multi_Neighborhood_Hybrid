// Include the validator project
#include "IHTP_Validator.h"

// Include your algorithm code
#include "GurSolver.h"
#include "InitialSolution.h"
#include "lahc.h"
#include "Solution.hpp"
#include "TheBigSwapper.h"
#include <filesystem>

int main(int argc, const char *argv[]) {

  // Default arguments
  string instance_file_name = "Instances/ihtc2024_competition_instances/i30.json";
  string solution_file_name = ""; // OVerwritten later
  string startSol_file_name =
      ""; // Leave empty to fill in auto with basename and sol quality
  bool verbose = false;

  double ipNursesW = 0.306;
  double ipPatientW = 0.7972;
  double ipOTW = 0.6062;
  double ipSlotW = 0.9069; 
  double cliqueW = 0.7677;
  double shortestPathW = 0.1922;
  double swapNursesW = 0.0985;
  double swapPatientsW = 0.5345;
  double swapOTW = 0.4954;
  double nurseChangeW = 0.8056;
  double kickW = 0.7325;
  double movePatientW = 0.0717;
  double maxRunTimeIPnurse = 0.75;
  double maxRunTimeIPpatient = 0.75;
  double maxRunTimeIPslot = 0.75;
  double maxRunTimeIPoperatingT = 0.75;

  int noImpLimitIPnurse = 331;
  int noImpLimitIPpatient = 50;
  int noImpLimitIPslot = 389;
  int noImpLimitIPoperatingT = 120;
  int upperLimitIP = 71;
  double targetMultIP = 7.5296;

  double timeLimitIPnurse = 0.25;
  double timeLimitIPpatient = 0.25;
  double timeLimitIPslot = 0.25;
  double timeLimitIPoperatingT = 0.25;

  // LAHC params
  double histMult = 10;
  int maxHist = 100000;
  double maxWorse = 0.995;
  double perturbIncrease = 0.01; 


#ifndef NDEBUG
  int noThreads = 1;
#else
  int noThreads = 4;
#endif

  int timeLimitSeconds = 600;
  int seed = 0;

  // Parse command-line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--instance" && (i + 1) < argc) {
      instance_file_name = argv[++i]; // Get the next argument as the instance
    } else if (arg == "--startSol" && (i + 1) < argc) {
      startSol_file_name = argv[++i]; // Get the next argument as the solution
    } else if (arg == "--solution" && (i + 1) < argc) {
      solution_file_name = argv[++i]; // Get the next argument as the solution
    } else if (arg == "--verbose") {
      verbose = atoi(argv[++i]); // Set verbose flag
    } else if (arg == "--timeLimit") {
      timeLimitSeconds = atoi(argv[++i]);
    } else if (arg == "--noThreads") {
      noThreads = atoi(argv[++i]);
    } else if (arg == "--noImpLimitIPnurse") {
        noImpLimitIPnurse = atoi(argv[++i]);
    }
    else if (arg == "--noImpLimitIPpatient") {
        noImpLimitIPpatient = atoi(argv[++i]);
    }
    else if (arg == "--noImpLimitIPslot") {
        noImpLimitIPslot = atoi(argv[++i]);
    }
    else if (arg == "--noImpLimitIPoperatingT") {
        noImpLimitIPoperatingT = atoi(argv[++i]);
    }
    else if (arg == "--timeLimitIPnurse") {
        timeLimitIPnurse = atof(argv[++i]);
    }
    else if (arg == "--timeLimitIPpatient") {
        timeLimitIPpatient = atof(argv[++i]);
    }
    else if (arg == "--timeLimitIPslot") {
        timeLimitIPslot = atof(argv[++i]);
    }
    else if (arg == "--timeLimitIPoperatingT") {
        timeLimitIPoperatingT = atof(argv[++i]);
    }
    else if (arg == "--maxRunTimeIPnurse") {
        maxRunTimeIPnurse = atof(argv[++i]);
    }
    else if (arg == "--maxRunTimeIPpatient") {
        maxRunTimeIPpatient = atof(argv[++i]);
    }
    else if (arg == "--maxRunTimeIPslot") {
        maxRunTimeIPslot = atof(argv[++i]);
    }
    else if (arg == "--maxRunTimeIPoperatingT") {
        maxRunTimeIPoperatingT = atof(argv[++i]);
    }
    else if (arg == "--upperLimitIP") {
        upperLimitIP = atoi(argv[++i]);
    }
    else if (arg == "--targetMultIP") {
        targetMultIP = atof(argv[++i]);
    } else if (arg == "--seed") {
      // remark; I am not certain if this sets the seed for all threads. It
      // could be that this function needs to be called for every thread.
      seed = atoi(argv[++i]);
    } else if (arg == "--cliqueW") {
      cliqueW = atof(argv[++i]);
    } else if (arg == "--shortestPathW") {
      shortestPathW = atof(argv[++i]);
    } else if (arg == "--swapNursesW") {
      swapNursesW = atof(argv[++i]);
    } else if (arg == "--swapPatientsW") {
      swapPatientsW = atof(argv[++i]);
    } else if (arg == "--swapOTW") {
      swapOTW = atof(argv[++i]);
    } else if (arg == "--nurseChangeW") {
      nurseChangeW = atof(argv[++i]);
    } else if (arg == "--kickW") {
      kickW = atof(argv[++i]);
    } else if (arg == "--movePatientW") {
      movePatientW = atof(argv[++i]);
    } else if (arg == "--ipNursesW") {
      ipNursesW = atof(argv[++i]);
    } else if (arg == "--ipPatientW") {
      ipPatientW = atof(argv[++i]);
    } else if (arg == "--ipOTW") {
      ipOTW = atof(argv[++i]);
    } else if (arg == "--ipSlotW") {
      ipSlotW = atof(argv[++i]);
    } else if (arg == "--histMult") {
      histMult = atof(argv[++i]);
    } else if (arg == "--maxHist") {
      maxHist = atoi(argv[++i]);
    } else if (arg == "--maxWorse") {
      maxWorse = atof(argv[++i]);
    } else if (arg == "--perturbIncrease") {
      perturbIncrease = atof(argv[++i]);
    } else if (arg == "--help") {
      // std::cout << "Usage: " << argv[0]
      //           << " --instance <instance_file> --solution <solution_file> "
      //              "--verbose <true/false> --algorithm <constructive,ip,vns>"
      //           << std::endl;
      std::cout << "Usage: " << argv[0]
                << " --instance <instance_file> --solution <solution_file> "
                   "--verbose <true/false> "
                   "--timeLimit <int> --noThreads <1/2/3/4> --seed"
                << std::endl;
      return 1;
    } else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return 1; // Exit with an error code
    }
  }
  std::cout << "Time limit: " << timeLimitSeconds << std::endl;
  setSeed(seed);

  std::vector<double> SwapOperatorWeights = {
      cliqueW, shortestPathW, swapNursesW, swapPatientsW, swapOTW, nurseChangeW,
      kickW, movePatientW};

  std::vector<double> IPOperatorWeights = {ipNursesW,     ipPatientW,  ipSlotW,       ipOTW};
  std::vector<int> noImpLimitIP = { noImpLimitIPnurse, noImpLimitIPpatient, noImpLimitIPslot, noImpLimitIPoperatingT };
  std::vector<double> timeLimitIP = { timeLimitIPnurse, timeLimitIPpatient, timeLimitIPslot, timeLimitIPoperatingT };
  std::vector<double> maxRunTimeIP = { maxRunTimeIPnurse, maxRunTimeIPpatient, maxRunTimeIPslot, maxRunTimeIPoperatingT };

  for (int i = 0; i < maxRunTimeIP.size(); i++)
  {
      if (maxRunTimeIP[i] < timeLimitIP[i]) {
          std::cout << "Warning: updating maxRunTimeIP to TimeLimitIP since the "
              "latter is higher!"
              << std::endl;
          maxRunTimeIP[i] = timeLimitIP[i];
      }
  }

    std::cout << "=== PARAMS ===" << std::endl;
    std::cout << "Instance = " << instance_file_name << "\n";
    std::cout << "noThreads = " << noThreads << "\n";
    std::cout << "timeLimit = " << timeLimitSeconds << "\n";
    std::cout << "ipNursesW = " << ipNursesW << "\n";
    std::cout << "ipPatientW = " << ipPatientW << "\n";
    std::cout << "ipOTW = " << ipOTW << "\n";
    std::cout << "ipSlotW = " << ipSlotW << "\n";
    std::cout << "cliqueW = " << cliqueW << "\n";
    std::cout << "shortestPathW = " << shortestPathW << "\n";
    std::cout << "swapNursesW = " << swapNursesW << "\n";
    std::cout << "swapPatientsW = " << swapPatientsW << "\n";
    std::cout << "swapOTW = " << swapOTW << "\n";
    std::cout << "nurseChangeW = " << nurseChangeW << "\n";
    std::cout << "kickW = " << kickW << "\n";
    std::cout << "movePatientW = " << movePatientW << "\n";
    std::cout << "maxRunTimeIPnurse = " << maxRunTimeIPnurse << "\n";
    std::cout << "maxRunTimeIPpatient = " << maxRunTimeIPpatient << "\n";
    std::cout << "maxRunTimeIPslot = " << maxRunTimeIPslot << "\n";
    std::cout << "maxRunTimeIPoperatingT = " << maxRunTimeIPoperatingT << "\n";
    std::cout << "noImpLimitIPnurse = " << noImpLimitIPnurse << "\n";
    std::cout << "noImpLimitIPpatient = " << noImpLimitIPpatient << "\n";
    std::cout << "noImpLimitIPslot = " << noImpLimitIPslot << "\n";
    std::cout << "noImpLimitIPoperatingT = " << noImpLimitIPoperatingT << "\n";
    std::cout << "upperLimitIP = " << upperLimitIP << "\n";
    std::cout << "targetMultIP = " << targetMultIP << "\n";
    std::cout << "histMult = " << histMult << "\n";
    std::cout << "maxHist = " << maxHist << "\n";
    std::cout << "maxWorse = " << maxWorse << "\n";
    std::cout << "perturbIncrease = " << perturbIncrease << "\n";


  // Create input object
  IHTP_Input in(instance_file_name);

  // Create solution object. If initial solution provided, validate it
  Solution sol(startSol_file_name.empty()
                   ? Solution(in, verbose)
                   : Solution(in, startSol_file_name, verbose));
                   
                   
if (!startSol_file_name.empty()) {
    std::cout << "\n\n========= INITIAL SOLUTION =========" << std::endl;
    sol.PrintCosts();    
    int original_cost = sol.getObjValue();
    for (int r = 0; r < in.Rooms(); r++)
    {
        for (int s = 0; s < in.Shifts(); s++)
        {
            if (sol.getRoomShiftNurse(r, s) == -1)
            {
                int n = std::rand() % in.AvailableNurses(s);
                sol.AssignNurse(in.AvailableNurse(s, n), r, s);
            }
        }
    }
    assert(sol.getObjValue() == original_cost);
    std::cout << std::endl << std::endl;
} else {
    std::cout << "\n\n========= CONSTRUCTIVE SOLUTION =========" << std::endl;
    InSol insol(in, sol);
    // TODO TODO
    // Choose one of 6 starting methods
    insol.solve(0);
    // sol.PrintCosts();
  }

  // If insol is not feasible: use ip
  if (sol.getInfValue() > 0) {
    sol.Reset();
    GurSolver gur(in, sol);
    if (gur.MandatoryPatientsOnly(4, 600)) {
      sol = gur.gursol;
    } else {
      std::cout
          << "No feasible solution was found within the given time limit..."
          << std::endl;
      return 0;
    }

    InSol insol2(in, sol);
    insol2.addNonMandatoryPatients();
    insol2.addNurses();
    sol.PrintCosts();
    assert(sol.getInfValue() == 0);
  }

  std::cout << "\n\n\n======= STARTING ILS ====\n\n\n"
            << std::endl;


  LAHC(noThreads, timeLimitSeconds, in, sol, IPOperatorWeights, SwapOperatorWeights, 
     noImpLimitIP, timeLimitIP, maxRunTimeIP, upperLimitIP, targetMultIP, histMult, perturbIncrease, maxHist, maxWorse);
  std::cout << "\n\n========= FINAL SOL =========" << std::endl;

  sol.PrintCosts();
  if (solution_file_name.empty()) {
    std::filesystem::path instancePath = instance_file_name;
    solution_file_name =
        instancePath.stem().string() + "_" +
        std::to_string((int)std::round(sol.getInfValue())) + "_" +
        std::to_string((int)std::round(sol.getObjValue())) + ".sol";
  }
  std::cout << "Write solution to: " << solution_file_name << std::endl;
  sol.to_json(solution_file_name);
  std::cout << std::endl << std::endl;
}
