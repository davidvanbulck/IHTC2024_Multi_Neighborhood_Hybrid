#ifndef LAHC_H 			// True if at least one IP operator is activated
#define LAHC_H

#include "BaseAlgo.h"
#include "IHTP_Validator.h"
#include "Solution.hpp"
#include "gurobi_c++.h"
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <limits.h>
#include "TheBigSwapper.h"
#include "GurSolver.h"
#include <queue>


class LAHC
{
 	public:
		LAHC(const int noThreads, const int timeLimitSeconds, const IHTP_Input &in, Solution &out, const std::vector<double> &IPOperatorWeights, const std::vector<double> &SwapOperatorWeights, const std::vector<int> noImpLimitIP, const std::vector<double> TimeLimitIP, const std::vector<double> maxRunTimeIP, const int upperLimitIP, const double targetMultIP, const double histMult, const double perturbIncrease, const int maxHist, const double maxWorse);
		~LAHC();
		void optimizeThread(int i, std::function<bool(int, int)> acceptFunc, std::function<bool ()> stopFunc);

		void localSearch(const int threadId);

	private:
		// All time best solution
		Solution& bestSol;

		// Value of best solution found by Gurobi
		double bestGurSol = INT_MAX;

		// Start and end time
		std::chrono::steady_clock::time_point startTime,endTime;

		// Thread-safe accesses
		std::shared_mutex solMutex;

		// Overall number of threads
		const int noThreads;

		// Available threads for exploring neighborhoods
		std::vector<Worker*> workers;

		// Overall time limit
		const int timeLimitSeconds;

		// Random number generator
  		std::discrete_distribution<int> swapOperatorGenerator;
  		std::discrete_distribution<int> swapOperatorGenerator_NoClique;
  		std::discrete_distribution<int> ipOperatorGenerator;
		std::discrete_distribution<int> perturbGenerator;
		std::discrete_distribution<int> perturbGenerator_NoClique;
  		std::uniform_real_distribution<> random_double = std::uniform_real_distribution<>(0, 1);

		// Neighborhood selection
		int selectIPOperator() { return ipOperatorGenerator(rng); };
		int selectSwapOperator() { return swapOperatorGenerator(rng); };
		int selectSwapOperatorNoClique() {return swapOperatorGenerator_NoClique(rng) + 1; };
		int selectPerturber() { return perturbGenerator(rng); };
		int selectPerturberNoClique() { return perturbGenerator_NoClique(rng) + 1; };

		// Pretty printing
		std::vector<std::string> swapOperatorStr = {"Clique", "Shortest path", "Nurse assignment", "Patient Swap", "OT Swap", "Nurse Change", "Patient Kick", "Patient Move"};
		std::vector<std::string> ipOperatorStr = {"IPNurse", "IPPatient", "IPSlot", "IPOperatingT"};

		// Destruction sizes of IP operators
		// Nurses-FreeRooms, Nurses-FreeShifts, Patients-FreeDays, Patients-FreeSurgeons
		std::vector<std::vector<double>> destrSizes = {{0.05, 0.05}, {0.10, 0.10}, {0.05, 0.05, 0.20}}; 
		std::shared_mutex destrMutex;

		// Acceptance criterion
		bool acceptSolHCStrict(const int delta); 	
		bool acceptSolHist(const int obj, const int delta, int &idle, long& iter, std::vector<int> &history);

		// Stopping criterion
		bool stopNoImpTimeLAHC(const int iter, const int idle, const int minIdle);
		bool stopTime();

		// Search params
		double elapsedTime = 0; 	
		std::vector<int> noImpLimitIP;
		std::vector<int> noImpIP = { 0, 0, 0, 0 }; // Iter without improvement for all IP operators
		std::vector<double> timeLimitIP; // Target rime limit for all IP operators in seconds
		std::vector<double> maxRunTimeIP; // Max runtime for all IP operators that we allow
		const int upperLimitIP; 		// Overall limit any IP can take
		const double targetMultIP; 		
		const bool useIP; 			// True if at least one IP operator is activated
		const bool useSwap; 			// True if at least one Swap operator is activated

		// History params
		double histMult = 2; 			// Multiplicative factor for history length of worker
		double perturbIncrease = 0.01; 		// Amount added to initial history init values
		int maxHist = 10000; 			// Max history length
		double maxWorse = 0.97; 		// Max % HC is behind than best

		std::shared_mutex noImpIPMutex;
};

#endif /* LAHC_H */
