#include "lahc.h"
#include <cstdlib>
#include <mutex>

// Only accept solutions that are strictly better
inline bool LAHC::acceptSolHCStrict(const int delta) { 
	return delta < 0; 
}

inline bool LAHC::acceptSolHist(const int obj, const int delta, int &idle, long& iter, std::vector<int> &history) { 
	
	std::unique_lock<std::shared_mutex> lock(solMutex);
	
	if(delta >= 0){
		idle++;
	} else {
		idle = 0;
	}

	// Calculate the virtual beginning
	long v = iter % history.size();
	bool accept = (obj < history[v] || delta <= 0) ? true : false;
	if(obj < history[v]){
		history[v] = obj;
	}

	iter++;

	return accept; 
}


// Are we runnig out of time?
bool LAHC::stopTime() {
  std::shared_lock<std::shared_mutex> lock(solMutex);
  return elapsedTime >= timeLimitSeconds;
}

inline bool LAHC::stopNoImpTimeLAHC(const int iter, const int idle, const int minIdle) {
  std::shared_lock<std::shared_mutex> lock(solMutex);

  return (elapsedTime >= timeLimitSeconds) || (idle > minIdle && idle > iter*0.02);
}


void LAHC::localSearch(const int threadId){

	// Easy access of worker element
	Worker *worker = workers[threadId];

	// Copy the newly found solution from gursol to sol
	int initBest = -1;
	int thisBest = -1;
	{
		std::shared_lock<std::shared_mutex> lock(solMutex);
		initBest = bestGurSol;
		if(worker->sol.getObjValue() > bestSol.getObjValue()){
			worker->sol = bestSol;
		}
		thisBest = bestSol.getObjValue();
	}

	// Initialize search params
	worker->worker_idle = 0;
	worker->worker_iter = 0;
	worker->initList(worker->sol.getObjValue() * worker->perturbValue);

	std::cout << "Thread " << threadId << " Start new search with solution " << worker->sol.getObjValue() << " and history " << worker->worker_historyLenght << " and perturb " << worker->perturbValue << std::endl;

	int choiceInt, oldObjValue, objValue;
	bool newSol;
	// First a local search phase
	bool stop = false;
	do {			
		// Pick a classic (non-ip) neighborhood
		// Cliques cannot run at multiple threads at the same time... (Coding issue in clique software)
		choiceInt = (threadId == 0 ? selectSwapOperator() : selectSwapOperatorNoClique());

		// Generate a new solution...
		oldObjValue = worker->sol.getObjValue();
		newSol = worker->executeNeighborhood(choiceInt, 1);
		objValue = worker->sol.getObjValue();

		if(objValue < 0){
			std::cout << "WARNING " << swapOperatorStr[choiceInt] << std::setw(20)
					<< worker->sol.getObjValue() << std::endl;
			worker->reverse();
			worker->sol.setObjValue(oldObjValue);
			newSol = false;
		}

		// Did the operator produce a new sol? If not, no need to check whether we accept... 
		// E.g. swapper did not found a feasible candidate neighborhood
		if (newSol) {

			// Set the total time elapsed for acceptFunc
			{
				std::unique_lock<std::shared_mutex> lock(solMutex);
				elapsedTime = timeLimitSeconds - std::chrono::duration_cast<std::chrono::seconds>(endTime - std::chrono::steady_clock::now()).count();
			}
			if (acceptSolHist(objValue, objValue - oldObjValue, worker->worker_idle, worker->worker_iter, worker->worker_history)
					) { 
				std::unique_lock<std::shared_mutex> lock(solMutex);
				if (objValue < bestSol.getObjValue()) {
					
					if (objValue < thisBest){
						thisBest = objValue;
					}

					// Save best sol 
					bestSol = worker->sol;

#ifndef MINIMALPRINT
					// Green
					std::cout << "\033[32m";
					std::cout << std::left << std::setw(10) << elapsedTime
						<< std::setw(10) << threadId << std::setw(25)
						<< swapOperatorStr[choiceInt] << std::setw(20)
						<< worker->sol.getObjValue() << std::endl;
					std::cout << "\033[0m";
#endif
				}
				else if (objValue < oldObjValue) {
					
					if (objValue < thisBest){
						thisBest = objValue;
					}

					// Blue
#ifndef MINIMALPRINT
					std::cout << "\033[34m";
					std::cout << std::left << std::setw(10) << elapsedTime
						<< std::setw(10) << threadId << std::setw(25)
						<< swapOperatorStr[choiceInt] << std::setw(20)
						<< worker->sol.getObjValue() << std::endl;
					std::cout << "\033[0m";
#endif
				}
				else if (objValue > oldObjValue) {
					// Red
#ifndef MINIMALPRINT
					std::cout << "\033[31m";
					std::cout << std::left << std::setw(10) << elapsedTime
						<< std::setw(10) << threadId << std::setw(25)
						<< swapOperatorStr[choiceInt] << std::setw(20)
						<< worker->sol.getObjValue() << std::endl;
					std::cout << "\033[0m";
#endif
				}
			}
			else {
				// Reset the solution
				worker->reverse();
				worker->sol.setObjValue(oldObjValue);
			}
		}

		{
			std::shared_lock<std::shared_mutex> lock(solMutex);
			if(threadId == 0  && bestSol.getObjValue() < thisBest*maxWorse){
				std::cout << "UPDATE " << threadId << " FROM " << thisBest << " TO " << bestSol.getObjValue() << std::endl;
				worker->sol = bestSol;
				thisBest = bestSol.getObjValue();
				worker->worker_historyLenght = 1;
				worker->perturbValue = 1;
				stop = true;
			}
		}

		double currentBestGur = -1;
		{
			std::shared_lock<std::shared_mutex> lock(solMutex);
			currentBestGur = bestGurSol;
		}
		
		//if(currentBestGur < initBest){
		//	// A new best solution was found by IP: reset since hopefully it is very different
		//	// and hence we are not stuck yet
		//	worker->worker_historyLenght = 1;
		//	worker->perturbValue = 1;
		//	stop = true;
		//}
		if(!stop){
			// No reasons to stop
			if(stopNoImpTimeLAHC(worker->worker_iter, worker->worker_idle, worker->worker_minIdle)){
				// Increase the history length and pertubation value
				stop = true;
				worker->worker_historyLenght = std::min((double) maxHist,std::ceil(worker->worker_historyLenght*histMult));
				worker->perturbValue += perturbIncrease;
			}
		}
	} while (!stop);
	std::cout << "Thread " << threadId << " local search done (" << worker->sol.getObjValue() << ")" << std::endl;
}

// Core of the algorithm. Repeatedly select a neighborhood and solve it until stopFunc returns true
void LAHC::optimizeThread(int i, std::function<bool(int, int)> acceptFunc, std::function<bool ()> stopFunc){

	// Easy access of worker element
	Worker *worker = workers[i];

	// Define outside loop for speed
	int objValue, oldObjValue, choiceInt;
	bool newBestFound;
	while (!stopFunc()){
		// Check if another thread found a new best solution
		{
			std::shared_lock<std::shared_mutex> lock(solMutex);
			if(bestSol.getObjValue() < worker->gursol.getObjValue()){
				worker->gursol = bestSol;
			}
		}

		if((i > 0 && useIP) || !useSwap){
			// Choose an IP operator
			choiceInt = selectIPOperator();

			// Store old objective value
			oldObjValue = worker->gursol.getObjValue();

			// Destr sizes of the operator in choice
			std::vector<double> destrSizesDummy;

			if (choiceInt < 3) { // OT's do not have a destruction size: always free
					     // all patients on one day
				{
					std::shared_lock<std::shared_mutex> lock(destrMutex);
					for (auto &i : destrSizes[choiceInt]) {
						destrSizesDummy.push_back(i);
					}
				}
			}

			// How long did the IP operator ran? Needed for setting destr sizes
			double ModelRunTime = 0.0;

			double budget;
			{
				std::shared_lock<std::shared_mutex> lock(solMutex);
				budget = timeLimitSeconds - elapsedTime;
			}
			budget = std::min(budget, maxRunTimeIP[choiceInt]);

			objValue = worker->optimizeModel(choiceInt, destrSizesDummy, ModelRunTime,
					budget);

			// Update destriction sizes
			if (choiceInt < 3) {
				std::unique_lock<std::shared_mutex> lock(destrMutex);
				#ifndef NDEBUG
				std::cout << "Change destruction sizes " << ipOperatorStr[choiceInt] << " from ";
				for (auto &d : destrSizes[choiceInt]) {
					std::cout << d << "\t";
				}
				#endif
				int rn = std::rand() % destrSizes[choiceInt].size();
				assert(rn < destrSizes[choiceInt].size());
				if (ModelRunTime < timeLimitIP[choiceInt]) {
					#ifndef NDEBUG
					std::cout << " INCREASE ";
					#endif
					// We used less time than our target --> increase the destruction size
					if (destrSizes[choiceInt][rn] < 0.99) {
						destrSizes[choiceInt][rn] += 0.01;
					} else {
						rn = (rn + 1) % destrSizes[choiceInt].size();
						assert(rn < destrSizes[choiceInt].size());
						if (destrSizes[choiceInt][rn] < 0.99) {
							destrSizes[choiceInt][rn] += 0.01;
						}
					}
				} else {
					// We used more time than our target --> decrease the destruction size
					#ifndef NDEBUG
					std::cout << " DECREASE ";
					#endif
					if (destrSizes[choiceInt][rn] > 0.01) {
						destrSizes[choiceInt][rn] -= 0.01;
					} else {
						rn = (rn + 1) % destrSizes[choiceInt].size();
						assert(rn < destrSizes[choiceInt].size());
						if (destrSizes[choiceInt][rn] > 0.01) {
							destrSizes[choiceInt][rn] -= 0.01;
						}
					}
				}
				#ifndef NDEBUG
				std::cout << " to ";
				for (auto &d : destrSizes[choiceInt]) {
					std::cout << d << "\t";
				}
				std::cout << std::endl;
				#endif
			}


			// Set the total time elapsed for acceptFunc
			{
				std::shared_lock<std::shared_mutex> lock(solMutex);
				elapsedTime = timeLimitSeconds - std::chrono::duration_cast<std::chrono::seconds>(endTime - std::chrono::steady_clock::now()).count();
			}

			// Determine if IP found a new best solution
			newBestFound = false;
			if (acceptFunc(objValue, objValue - oldObjValue)) {
				std::unique_lock<std::shared_mutex> lock(solMutex);
				if (objValue < bestSol.getObjValue()) {
					// A new best sol has been found by IP!
					newBestFound = true;
					bestSol = worker->gursol;

					{
						std::unique_lock<std::shared_mutex> lock(noImpIPMutex);
						noImpIP[choiceInt] = 0;
					}
					
					bestGurSol = bestSol.getObjValue();
#ifndef MINIMALPRINT
					// Print new best solutions in green
					std::cout << "\033[36m";
					std::cout << std::left << std::setw(10) << elapsedTime
						<< std::setw(10) << worker->getThreadId() << std::setw(25)
						<< ipOperatorStr[choiceInt] << std::setw(20)
						<< worker->gursol.getObjValue() << std::endl;
					std::cout << "\033[0m";
#endif
				} else {
					std::unique_lock<std::shared_mutex> lock(noImpIPMutex);
					noImpIP[choiceInt]++;
				}
			} else {
				std::unique_lock<std::shared_mutex> lock(noImpIPMutex);
				noImpIP[choiceInt]++;
			}
	
			// Update time limit IP
			{
				std::unique_lock<std::shared_mutex> lock(noImpIPMutex);
				if (noImpIP[choiceInt] > noImpLimitIP[choiceInt]) {
#ifndef MINIMALPRINT
					cout << "Update time limit " << ipOperatorStr[choiceInt] << " from " << timeLimitIP[choiceInt];
#endif
					if(timeLimitIP[choiceInt] * targetMultIP <= upperLimitIP){
						timeLimitIP[choiceInt] *= targetMultIP;
						maxRunTimeIP[choiceInt] = std::min((double) upperLimitIP, timeLimitIP[choiceInt]*2);
					}
					noImpIP[choiceInt] = 0;
#ifndef MINIMALPRINT
					std::cout << " to " << timeLimitIP[choiceInt] << std::endl;
#endif
				}
			}

			if(useSwap && newBestFound) {
				localSearch(i);
				worker->worker_historyLenght = 1;
				worker->perturbValue = 1;
			}
		} else {
			// Try to further improve using local search...
			// We do this out of the loop to free the lock on solMutex
			localSearch(i);
		}
	}
}

LAHC::~LAHC() {
  for (int i = 0; i < noThreads; ++i) {
    delete workers[i];
  }
}


LAHC::LAHC(const int noThreads, const int timeLimitSeconds, const IHTP_Input &in, Solution &out, const std::vector<double> &IPOperatorWeights,
	const::std::vector<double> &SwapOperatorWeights, const std::vector<int> noImpLimitIP,
	const std::vector<double> timeLimitIP, const std::vector<double> maxRunTimeIP, const int upperLimitIP, const double targetMultIP, const double histMult, const double perturbIncrease, const int maxHist, const double maxWorse):
	noThreads(noThreads), timeLimitSeconds(timeLimitSeconds), bestSol(out),
	noImpLimitIP(noImpLimitIP),
        timeLimitIP(timeLimitIP), maxRunTimeIP(maxRunTimeIP), upperLimitIP(upperLimitIP), targetMultIP(targetMultIP),
	useIP(std::accumulate(IPOperatorWeights.begin(), IPOperatorWeights.end(), 0.0) == 0.0 ? false : true),
	useSwap(std::accumulate(SwapOperatorWeights.begin(), SwapOperatorWeights.end(), 0.0) == 0.0 ? false : true),
	histMult(histMult), perturbIncrease(perturbIncrease),
	maxHist(maxHist), maxWorse(maxWorse)
{


  // Some debugging asserts
  assert(SwapOperatorWeights.size() == swapOperatorStr.size());
  assert(IPOperatorWeights.size() == ipOperatorStr.size());
  assert(noImpLimitIP.size() == ipOperatorStr.size());

  // Start the timer...
  startTime = std::chrono::steady_clock::now();

  // A feasible starting solution is required
  if (out.getInfValue() > 0) {
    std::cout << "LAHC Algorithm expects a feasible starting solutution!"
              << std::endl;
    std::cout << "Aborting..." << std::endl;
    std::abort();
  }


  // Intialize the neighborhood selection weights
  swapOperatorGenerator = std::discrete_distribution<int>(SwapOperatorWeights.begin(), SwapOperatorWeights.end());
  swapOperatorGenerator_NoClique = std::discrete_distribution<int>(std::next(SwapOperatorWeights.begin()), SwapOperatorWeights.end());
  ipOperatorGenerator = std::discrete_distribution<int>(IPOperatorWeights.begin(), IPOperatorWeights.end());
  perturbGenerator = std::discrete_distribution<int>(SwapOperatorWeights.begin(), std::next(SwapOperatorWeights.begin(), 2));
  perturbGenerator_NoClique = std::discrete_distribution<int>(std::next(SwapOperatorWeights.begin()), std::next(SwapOperatorWeights.begin(), 2));


  // Construct all the worker objects
  workers.resize(noThreads, nullptr);
#pragma omp parallel for
  for (int i = 0; i < noThreads; ++i) {
    workers[i] = new Worker(i, in, out, 1, 1, (i == 0 ? 100000 : 4000));
    // workers[i]->setNoThreads(1);
    workers[i]->model.set(GRB_IntParam_LogToConsole, 0);
    // workers[i]->setTimeLimit(30);
  }

  // Overall time limit for the optimization process
  endTime = startTime + std::chrono::seconds(timeLimitSeconds);
  auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(
                         startTime - std::chrono::steady_clock::now())
                         .count();

  // Overall best solution found
  double bestObjValue = out.getObjValue();
  double destrSizeNurse = 0.05, destrSizePatient = 0.05, destrSizeSlots = 0.01;

  // Construct all the thread objects: each thread corresponds to one worker
  std::vector<std::thread> threads;
  threads.reserve(noThreads);


  std::cout << "\n\n====== ITERATED LOCAL SEARCH PHASE ======\n" << std::endl;
          std::cout << std::left << std::setw(10) << "Time"
                    << std::setw(10) << "Thread" << std::setw(25)
                    << "Operator" << std::setw(20)
                    << "IP Sol Value" << std::setw(20) << "Metaheur Value\n" << std::endl;



  for (int i = 0; i < noThreads; ++i) {
      threads.emplace_back(
          &LAHC::optimizeThread, this, i,
          std::bind(&LAHC::acceptSolHCStrict, this, std::placeholders::_2),
          std::bind(&LAHC::stopTime, this));
  }

  // Wait for all threads to finish
  for (auto &thread : threads) {
    thread.join();
  }

  // Print final solution found
  std::cout << "====================================" << std::endl;
  std::cout << "  Best sol found: " << out.getInfValue() << " - "
            << out.getObjValue() << std::endl;
  std::cout << "====================================" << std::endl;
}
