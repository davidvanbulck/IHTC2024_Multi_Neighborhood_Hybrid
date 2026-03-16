#ifndef THEBIGSWAPPER_H
#define THEBIGSWAPPER_H

#include "BaseAlgo.h"
#include "Solution.hpp"
#include "dlib/optimization/max_cost_assignment.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dag_shortest_paths.hpp>
#include <boost/graph/graphviz.hpp>
#include <climits>
#include <cstdint>
#ifdef CLIQUE
#include "cliquer.h"
#endif

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                              boost::no_property,
                              boost::property<boost::edge_weight_t, int>>
    Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;

class Swap : public virtual BaseAlgo {

public:
  Swap(const IHTP_Input &in, Solution &out);
  virtual ~Swap();
  Solution solve(int timeLimitSeconds);
  Solution sol;
  void loadSolutionSwap(const Solution &out);
  void saveSolutionSwap(Solution &solution);
  bool executeNeighborhood(const int choice, const int perturbationValue);

  // Remember which changes are made, so we don't have to copy the complete solution
  vector<vector<int>> unassignPatients;
  vector<int> assignPatients;
  vector<vector<int>> unassignNurses;
  vector<vector<int>> assignNurses;

  void reverse();

  bool moveOnePatient(int p, int r, int d, int t, bool newD, bool newR, bool newT);
  bool movePatient();

	// Deprecated. Use solution = out instead.
	//void loadSolutionSwap(const Solution& out);
	//void saveSolutionSwap(Solution& solution);
	bool swapPatients();
	/**
	 * @brief Replace a nurse on a given shift in a given room by another (random) nurse
	 */
	bool changeNurse();
	void swapNursesAssignment(int edgeCost, int s = -1);
  	void constructCostMatrix(int s, vector<int> &roomsToClear,
                           vector<vector<int>> &shiftsToClear, 
							int roomSpecialEdge, int nurseSpecialEdge, int costSpecialEdge);
	bool swapOperatingTheaters();
	dlib::matrix<int> costMatrix;
	void shortestPathPatients(const int perturbationValue);
	void clique(const int perturbationValue);
	// get a list of patients with the same gender and feasible admission window for each patient	 
	set<int> getCompatiblePatients(int p);
  bool kick();

private:
  /**
   * @brief Tries to kick a patient forward
   * 
   * @param p - The patient to kick
   * @param num_pos - The number of positions to try and kick the patient to. Defaults to -1. This means `as far as possible`
   * @return true - The kick succeeded
   * @return false - The kick failed
   */
  bool kickForward(int p, int num_pos=-1);
  /**
   * @brief kick a patient to a different room
   * 
   * @param p - patient to kick
   * @param room - room to kick the patient towards
   * @return true - kick succeeded
   * @return false - kick failed
   */
  bool kickToRoom(int p, int room);
  /**
   * @brief Kick a patient at cost of a different patient
   * 
   * @details Assumes that the gender of the room has already been checked
   *
   * @param p1 
   * @param p2 
   * @param room 
   * @return true - if both patients were relocated
   * @return false - false otherwise
   */
  bool kickOtherPatient(int p1, int p2, int room = -1);
  bool swapTwoPatients(int p, int p2);
  int costInfeasiblePatientSwap(int p, int p2);
  int CostEvaluationNurseSwap(int n, int r, int s);
  int CostEvaluationPatientSwap(int p, int p2);
  bool isPatientSwapFeasible(int p, int p2);
  bool isPatientMoveFeasible(int p, int r, int d, int t, bool newD, bool newR, bool newT);

  // Free a bed within [startTime, requestedEndTime[
  void freeBed(const int &r, const int &startTime, const int &requestedEndTime,
               int &endTime, int &currentCostBed,
               std::vector<std::pair<int, int>> &patients);

  // Use cliquer software to find maximum weight clique
  std::vector<int> solveClique(std::vector<int> &vertexWeights,
                               std::vector<std::list<int>> &adjacency_list);

  vector<int> FindNewOperatingTheater(int p, int p2);
};

#endif /* THEBIGSWAPPER_H */
