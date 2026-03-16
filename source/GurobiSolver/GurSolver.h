#ifndef GURALGO_H
#define GURALGO_H

#include "BaseAlgo.h"
#include "IHTP_Validator.h"
#include "gurobi_c++.h"
#include <list>
#include <random>
#include <shared_mutex>
#include <limits.h>
#include "TheBigSwapper.h"

// A very small number (rounding errors: RC_EPS is considered as 0)
#define EPS 1.0e-6

extern thread_local std::mt19937 rng;
extern void setSeed(int seed);

struct PartialSolution {

	vector<vector<int>>CapacitiesOTs;
	vector<vector<int>>CapacitiesSurgeons;
	vector<vector<vector<int>>>RoomsDaysPatients;
	// Capacity room day follows from the no. of patients in it
	vector<vector<int>>RoomsShiftsNurse;
	vector<vector<vector<bool>>>SurgeonDaysOTs;

};

class GurSolver : public virtual BaseAlgo
{

public:
	/**
	 * @brief
	 * 1 if: Patient `i` admitted on day `d`.
	 */
	vector<vector<GRBVar>> x_id;
	/**
	 * @brief
	 * 1 if: Patient `i` assigned to room `r`.
	 */
	vector<vector<GRBVar>> y_ir;
	/**
	 * @brief
	 * 1 if: Patient `i` who was admitted on day `d`, still present on day `k`.
	 */
	vector<vector<vector<GRBVar>>> z_idk;
	/**
	 * @brief
	 * 1 if: patient `i` assigned to room `r` on day `d`.
	 */
	vector<vector<vector<GRBVar>>> q_ird;
	/**
	 * @brief
	 * 1 if: Gender::A assigned to room `r` on day `d`
	 */
	vector<vector<GRBVar>> g_rd;
	/**
	 * @brief
	 * Equals the maximum difference in age between two patients in the same room.
	 */
	vector<vector<GRBVar>> delta_dr;
	/**
	 * @brief
	 * 1 if: not mandatory patient `i` not assigned in the current solution
	 */
	vector<GRBVar> delta_iP;
	/**
	 * @brief
	 * 1 if: patient `i` is treated in OT `t`
	 */
	vector<vector<GRBVar>> w_it;
	/**
	 * @brief 1 if: patient `i` is assigned to OT `t` on day `d`.
	 *
	 */
	vector<vector<vector<GRBVar>>> v_itd;
	/**
	 * @brief
	 * 1 if: theater `t` open on day `d`
	 */
	vector<vector<GRBVar>> delta_td;
	/**
	 * @brief
	 * 1 if: surgeon `c` does surgery in OT `t` on day `d`
	 */
	vector<vector<vector<GRBVar>>> delta_ctd;
	/**
	 * @brief
	 * Equals the total number of 'room transfers' ```(Σ[#OT's for surgeon `c` on day `d`] - 1)```.
	 */
	vector<vector<GRBVar>> delta_cd;
	/**
	 * @brief
	 * 1 if: nurse `n` is assigned to room `r` on shift `s`.
	 */
	vector<vector<vector<GRBVar>>> u_nrs;
	/**
	 * @brief
	 * 1 if: nurse `n` treats patient `i`
	 */
	vector<vector<GRBVar>> b_in;
	/**
	 * @brief
	 * 1 if: nurse `n` treats occupant `j`
	 */
	vector<vector<GRBVar>> b_jn;
	/**
	 * @brief
	 * 1 if: nurse `n` treats patient `i` on its `k`th day in shift `s`
	 */
	vector<vector<vector<vector<GRBVar>>>> u_nsik;
	/**
	 * @brief
	 * Equals the amount of workload exceeding the capacity for nurse `n` in shift `s`
	 */
	vector<vector<GRBVar>> delta_ns;
	/**
	 * @brief
	 * 1 if: patient i assigned to theatre t on day d
	 */
	vector<vector<vector<GRBVar>>> w_itd;

	std::vector<std::vector<std::vector<bool>>> q_exists; // to make life easier
	std::vector<std::vector<std::vector<bool>>> w_exists;

	const GRBEnv env;
	GRBModel model = GRBModel(env);

	// Free operators
	//void releaseVars(std::list<GRBVar*> &vars);

	GurSolver(const IHTP_Input &in, Solution &out);
	/**
	 * @brief 
	 * Loads in solution into the MIP-solver.
	 * 
	 * Currently, this does not work yet
	 * @todo I am assuming that NOT supplying a start value for a variable will result in that variable starting from 0.
	 */
	//void loadSolution(const Solution &sol);
	virtual ~GurSolver();
	void Constraints();
	Solution solve(int timeLimitSeconds);

	void saveSolution(Solution &sol);

	//void setNoThreads (const int noThreads);
	//void setTimeLimit(const int noSeconds);

	vector<int>PatientsId;
	vector<int>RoomsId;
	vector<int>DaysId;
	int StartDay;
	int EndDay;
	int MaxFutureDay;
	vector<int>ShiftsId;

	GRBLinExpr Objective;

	int UnassignNurses(vector<int>& RoomsId, vector<int>& ShiftsId, std::vector<std::array<int,3>>& assignments_nurses);

	void Patient_formulation(vector<int>& PatientsId, vector<int>& DaysId, vector<int>& OperatingTheatersId, PartialSolution& solPart);
	void OT_formulation(const int d_, Solution& sol, GRBModel& model);
	void setPatients(vector<int>& PatientsId);
	void setRooms(vector<int>& RoomsId);
	void setDays(vector<int>& DaysId);
	void setShifts(vector<int>& ShiftsId);
	void setMaxFutureDay(int& MaxFutureDay);
	void saveSolutionOTs(const int newObj);
	void Nurse_formulation(vector<int>& RoomsId, vector<int>& ShiftsId, vector<vector<bool>>& beta_in, vector<vector<bool>>& beta_jn,  vector<vector<int>>& CapacitiesNurses, GRBModel &model);
	void Nurse_formulation2(vector<int>& RoomsId, vector<int>& ShiftsId, vector<vector<bool>>& beta_in, vector<vector<bool>>& beta_jn, GRBModel& model, GRBLinExpr& Objective, bool& IsInSlotModel, vector<vector<GRBLinExpr>>& NurseWorkload, std::vector<std::array<int,3>> &assignments_nurses);
	void saveSolutionNurses(const int newObj, vector<int>& RoomsId, vector<int>& ShiftsId);

	int optimizeModel(const int choice, const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime);

	void PatientConstraints(const bool NurseFixed, GRBModel& Fixedmodel, GRBLinExpr& Objective, vector<int>& PatientsId, vector<int>& DaysId, vector<int>& SurgeonsId, int& firstDay, int& maxLos, std::vector<std::array<int,4>> &assignments, bool addObj, vector<bool>& RoomFixed, vector<vector<GRBLinExpr>>& NurseWorkload);
	void PatientConstraints(const bool NurseFixed, GRBModel& Fixedmodel, GRBLinExpr& Objective, vector<int>& PatientsId, vector<int>& DaysId, vector<int>& SurgeonsId, int& firstDay, int& maxLos, std::vector<std::array<int,4>> &assignments);
	void FreeNurseConstraints(GRBModel& model, GRBLinExpr& Objective, vector<int>& PatientsId, vector<int>& DaysId, vector<int>& RoomsId, vector<vector<GRBLinExpr>>& NurseWorkload);
	void SavePatients(vector<int>& PatientsId, vector<int>& DaysId, vector<int>& RoomsId, std::vector<std::array<int,4>>& newAssignments, const bool NurseFixed);


	vector<int> SubsetRooms(double& FracFreeRooms);
	void SubsetShifts(vector<int>& ShiftsId, int& StartDay, int& EndDay, double& FracFreeShifts);
	vector<int> SubsetSurgeons(double& FracFreeSurgeons, std::vector<int> &days);
	vector<int> SubsetDays(double& FracFreeDays);
	int RebuildOptimizeNurses(const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime);
	int RebuildOptimizeOT(double& ModelRunTime, double maxRunTime);
	int RebuildOptimizePatientsDays(const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime, const bool NurseFixed);
	int MandatoryPatientsOnly(const int noThreads, const int timeLimit);

	double getMIPgap();
	int getObjValue();

	Solution gursol;
};

class Worker : public GurSolver, public Swap {
	// Type here all public functions
	public:
		// Default constructor
		Worker(
			// Unique thread id
			const int threadId,
			const IHTP_Input &in, 
			Solution &out,
			const int histLength = 100,
			const double perturbValue = 1,
			const int minIdle = 4000
		);

		// "in" is ambiguous, but constant so does not matter
		//using GurSolver::in;
		//
		// Parameters of late acceptancy hill climbing
		int worker_historyLenght; 	// The length of the history
		std::vector<int> worker_history; 	// Objective functions found in the past
		int worker_histPos; 			// Current position in the history length
		int worker_idle = 0; 			// Number of idle iterations
		int worker_minIdle; 	 		// Mininum number of iterations to perform
		double perturbValue;
		long worker_iter = 0; 	 		// Total number of iterations	
		void initList(const int obj);

		// Needs function solve to inherit from base algo
		Solution solve(int timeLimitSeconds);

		// Default destructor
		~Worker() {};

		// Fix opartors
		std::list<GRBVar*> fixPatientToDay(const Solution &out);
		std::list<GRBVar*> fixPatientToOT(const Solution &out);
		std::list<GRBVar*> fixPatientToRoom(const Solution &out, const double fractionFree);
		std::list<GRBVar*> fixNurseShiftToRoom(const Solution &out, const double fractionFree);
		std::list<GRBVar*> fixPatient(const Solution &out, const double fractionFree);
		std::list<GRBVar*> fixSlot(const Solution &out, const double fractionFree);


		// Fix patient to day, OT, and room. Optimize nurse to room.
		//bool optimizeNurses(const Solution &sol, std::shared_mutex &solMutex, double destrSize);

		// Fix nurse to room, optimize paient to day, OT, and room.
		//bool optimizePatient(const Solution &sol, std::shared_mutex &solMutex, double destrSize);

		// Nurse + patient free
		//bool optimizeSlot(const Solution &sol, std::shared_mutex &solMutex, double destrSize);

		// Fix nurse to room, patient to room, patient to day. Optimize patient to OT, each day independently.
		//bool optimizeOT(const Solution &sol, std::shared_mutex &solMutex);

		// Getters and setters
		int getThreadId() const { return threadId; }

		// perturbation occured, so solution should be updated
		bool perturbationOccured = false;
		int previousBestObjValue;

	private:
		// Attributes
		const int threadId;
};


//class Controller {
//	public:
//		Controller(const int noThreads, const IHTP_Input &in, Solution &out);
//		~Controller();
//
//		std::shared_mutex solMutex, destrMutex; 
//
//	private:
//		std::vector<Worker*> workers;
//		const int noThreads;
//};


#endif /* GURALGO_H */
