#include "GurSolver.h"
#include "TheBigSwapper.h"
#include <algorithm>
#include <mutex>
#include <cstdlib>
#include <omp.h>
#include <random>
#include <shared_mutex>
#include <thread>
#include <mutex>

thread_local std::mt19937 rng(42069);

void setSeed(int seed) {
	rng.seed(seed);
	srand(seed);
}

int GurSolver::optimizeModel(const int choice, const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime){
	/*
	switch (choice) {
		case 0:
			// Save the solution if strictly better only
			return RebuildOptimizeNurses(destrSize, ModelRunTime, maxRunTime);
		case 1:
			const bool NurseFixed = true;
			return RebuildOptimizePatientsDays(destrSize, ModelRunTime, maxRunTime, NurseFixed);
		case 2:
			const bool NurseFixed = false;
			return RebuildOptimizePatientsDays(destrSize, ModelRunTime, maxRunTime, NurseFixed);
		case 3:
			return RebuildOptimizeOT(ModelRunTime, maxRunTime);
		default:
			std::cout << "Operator choice " << choice << " not implemented" << std::endl;		
			std::abort();
	}
			*/
	if (choice == 0){
		return RebuildOptimizeNurses(destrSize, ModelRunTime, maxRunTime);
	}
	else if (choice == 1){
		const bool NurseFixed = true;
		return RebuildOptimizePatientsDays(destrSize, ModelRunTime, maxRunTime, NurseFixed);
	}	
	else if (choice == 2){
		const bool NurseFixed = false;
		return RebuildOptimizePatientsDays(destrSize, ModelRunTime, maxRunTime, NurseFixed);
	}	
	else if (choice == 3){
		return RebuildOptimizeOT(ModelRunTime, maxRunTime);
	}
	else{
		std::cout << "Operator choice " << choice << " not implemented" << std::endl;		
		std::abort();
	}
}


// A very small number (rounding errors: RC_EPS is considered as 0)
#define EPS 1.0e-6

//void GurSolver::setNoThreads(const int noThreads){
//	model.set(GRB_IntParam_Threads, noThreads);
//}

//void GurSolver::setTimeLimit(const int noSeconds){
//        model.set(GRB_DoubleParam_TimeLimit, noSeconds);
//}

GurSolver::GurSolver(const IHTP_Input &in, Solution &out) : gursol(in,false), BaseAlgo(in, gursol, INT_MAX)
{
	gursol = out;
}

//void GurSolver::loadSolution(const Solution &sol)
//
//{
//	// From the Gurobi manual:
//	// If you solve a sequence of models, where one is built by modifying the previous one, and if you don’t provide a MIP start, 
//	// then Gurobi will try to construct one automatically from the solution of the previous model. 
//	// If you don’t want it to try this, you should reset the model before starting the subsequent solve. 
//	// If you provided a MIP start but would prefer to use the previous solution as the start instead, you should clear your start 
//	// (by setting the Start attribute to undefined for all variables).
//	//
//	// Remove old solutions: otherwise gur with first try to improve based upon the old ones
//	model.reset();
//
//	// Fix all patient to day assignments
//	for (int i = 0; i < in.Patients(); i++) {
//		// H6: only loop over admissable days
//		for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++) {
//			if(d == sol.AdmissionDay(i)){
//				x_id[i][d].set(GRB_DoubleAttr_Start, 1);
//			} else {
//				x_id[i][d].set(GRB_DoubleAttr_Start, 0);
//			}
//		}
//	}
//
//	// Patient to OT
//	for (int i = 0; i < in.Patients(); i++)
//	{
//		for (int t = 0; t < in.OperatingTheaters(); t++)
//		{
//			if (sol.PatientOperatingTheater(i) == t) {
//				w_it[i][t].set(GRB_DoubleAttr_Start, 1);
//			} else {
//				w_it[i][t].set(GRB_DoubleAttr_Start, 0);
//			}
//		}
//	}
//
//	// Patient to room
//	for (int i = 0; i < in.Patients(); i++)
//	{
//		for (int r = 0; r < in.Rooms(); r++)
//		{
//			if (!in.IncompatibleRoom(i, r)){
//				if(sol.PatientRoom(i) == r){
//					y_ir[i][r].set(GRB_DoubleAttr_Start, 1);
//				} else {
//					y_ir[i][r].set(GRB_DoubleAttr_Start, 0);
//				}
//			}
//		}
//	}
//
//	// Nurse to room
//	for (int n = 0; n < in.Nurses(); n++)
//	{
//		for (int r = 0; r < in.Rooms(); r++)
//		{
//			for (int s = 0; s < in.Shifts(); s++)
//			{
//				if (in.IsNurseWorkingInShift(n, s))
//				{
//					if(sol.getRoomShiftNurse(r,s) == n){
//						u_nrs.at(n).at(r).at(s).set(GRB_DoubleAttr_Start, 1);
//					} else {
//						u_nrs.at(n).at(r).at(s).set(GRB_DoubleAttr_Start, 0);
//					}
//				}
//			}
//		}
//	}
//}

void GurSolver::Constraints()
{
	std::cout << "Constructing the IP model..." << std::endl;
	/*
	Global constraints
	Scheduling of patients to a) days of surgery, b) rooms and c) shifts in which it is in the hospital (c is implied)
	*/
	try
	{
		// x_id = 1 if patient i is assigned to day d => patients only!!!
		this->x_id = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.Days()));
		for (int i = 0; i < in.Patients(); i++)
		{
			// H6: only loop over admissable days
			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
			{
				std::string varName = "x_" + std::to_string(i) + "_" + std::to_string(d);
				x_id.at(i).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}

		// y_ir = 1 if patient i is assigned to room r => patients only! (occupants already have a room)
		this->y_ir = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.Rooms()));
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				// H2
				if (!in.IncompatibleRoom(i, r))
				{
					std::string varName = "y_" + std::to_string(i) + "_" + std::to_string(r);
					y_ir.at(i).at(r) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				}
			}
		}

		// z_idk = 1 if it is i's k'th day in the hospital on day d => for patients: implied by x_id
		// Additional index k is needed for the workload and skill level per patient in constraints S2 and S4
		// needed for q_ird = 1 if patient i is assigned to room r on day d
		// Do not need this for the occupants: no room assignment decisions

		this->z_idk = vector<vector<vector<GRBVar>>>(in.Patients());

		for (int i = 0; i < in.Patients(); i++)
		{
			z_idk.at(i) = vector<vector<GRBVar>>(in.Days());

			for (int d = in.PatientSurgeryReleaseDay(i); d < std::min(in.Days(), in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i)); d++)
			{
				z_idk.at(i).at(d) = vector<GRBVar>(in.PatientLengthOfStay(i));

				for (int k = 0; k < in.PatientLengthOfStay(i); k++)
				{
					std::string varName = "z_" + std::to_string(i) + "_" + std::to_string(d) + "_" + std::to_string(k);
					z_idk.at(i).at(d).at(k) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				}
			}
		}

		// Assign each admitted patient to exactly one room

		for (int i = 0; i < in.Patients(); i++)
		{
			GRBLinExpr sum_r = 0;
			for (int r = 0; r < in.Rooms(); r++)
			{
				if (!in.IncompatibleRoom(i, r))
				{
					sum_r += y_ir.at(i).at(r);
				}
			}

			GRBLinExpr sum_x = 0;
			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); ++d)
			{
				sum_x += x_id.at(i).at(d);
			}

			model.addConstr(sum_r == sum_x, "Base_PatientToRoom");
		}

		// H5: Admission of patients
		for (int i = 0; i < in.Patients(); i++)
		{
			GRBLinExpr sum_d = 0;

			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
			{
				sum_d += x_id.at(i).at(d);
			}

			if (in.PatientMandatory(i))
			{
				// Mandatory patients must be admitted
				model.addConstr(sum_d == 1, "H5_PatientToDay");
			}
			else
			{
				// Non-mandatory patients can be postponed
				model.addConstr(sum_d <= 1, "H5_PatientToDay");
			}
		}

		// The z_idk are implied by the value of x_id (for Patients)
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
			{
				/*
				Without index k in z_idk:

				GRBLinExpr sum_x = 0;
				for (int e = std::max(d-in.PatientLengthOfStay(i)+1, in.PatientSurgeryReleaseDay(i)); e <= d; e++){
					sum_x += x_id.at(i).at(e);
				}
				model.addConstr(z_id.at(i).at(d) == sum_x, "Linking_x_z");
				*/

				// With index k:
				for (int e = d; e < std::min(in.Days(), d + in.PatientLengthOfStay(i)); e++)
				{
					int k = e - d;
					model.addConstr(x_id.at(i).at(d) == z_idk.at(i).at(e).at(k), "Linking_x_z");
				}
			}
		}

		// Patient admission scheduling
		//  q_ird = 1 if patient i is assigned to room r on day d
		//  Not needed for occupants: assignment of occupants to rooms is already knwon

		this->q_ird = vector<vector<vector<GRBVar>>>(in.Patients(), vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(in.Days())));
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				if (!in.IncompatibleRoom(i, r))
				{
					for (int d = in.PatientSurgeryReleaseDay(i); d < std::min(in.Days(), in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i)); d++)
					{
						std::string varName = "q_" + std::to_string(i) + "_" + std::to_string(r) + "_" + std::to_string(d);
						q_ird.at(i).at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
					}
				}
			}
		}

		// Link these variables:
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				if (in.IncompatibleRoom(i, r))
				{
					continue;
				}
				for (int d = in.PatientSurgeryReleaseDay(i); d < std::min(in.Days(), in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i)); d++)
				{
					GRBLinExpr sum_z_idk = 0;
					for (int k = 0; k < in.PatientLengthOfStay(i); k++)
					{
						sum_z_idk += z_idk.at(i).at(d).at(k);
					}
					model.addConstr(q_ird.at(i).at(r).at(d) >= y_ir.at(i).at(r) + sum_z_idk - 1, "Linking_q_y_z");
					model.addConstr(q_ird.at(i).at(r).at(d) <= y_ir.at(i).at(r), "Linking_q_y");
					model.addConstr(q_ird.at(i).at(r).at(d) <= sum_z_idk, "Linking_q_z");
				}
			}
		}

		// H1 No Gender Mix
		// g_rd = 1 if room r gender A in day d, 0 otherwise
		this->g_rd = vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(in.Days()));
		for (int r = 0; r < in.Rooms(); r++)
		{
			for (int d = 0; d < in.Days(); ++d)
			{
				std::string varName = "g_" + std::to_string(r) + "_" + std::to_string(d);
				g_rd.at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}

		// Immediately consider the occupants
		for (int o = 0; o < in.Occupants(); o++)
		{
			int r = in.OccupantRoom(o);
			for (int d = 0; d < in.OccupantLengthOfStay(o); d++)
			{
				if (in.OccupantGender(o) == Gender::A)
				{
					g_rd.at(r).at(d).set(GRB_DoubleAttr_LB, 1);
				}
				else
				{
					g_rd.at(r).at(d).set(GRB_DoubleAttr_UB, 0);
				}
			}
		}

		// H1 No gender mix: other patients
		// H7 Room Capacity
		for (int r = 0; r < in.Rooms(); r++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				int cap = in.RoomCapacity(r);
				for (int j = 0; j < in.Occupants(); j++)
				{
					if (in.OccupantRoom(j) == r && d < in.OccupantLengthOfStay(j))
					{
						cap--;
					}
				}

				GRBLinExpr sumA;
				GRBLinExpr sumB;
				for (int i = 0; i < in.Patients(); i++)
				{
					if ((!in.IncompatibleRoom(i, r)) && d >= in.PatientSurgeryReleaseDay(i) && d <= in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i) - 1)
					{
						if (in.PatientGender(i) == Gender::A)
						{
							sumA += q_ird.at(i).at(r).at(d);
						}
						else
						{
							sumB += q_ird.at(i).at(r).at(d);
						}
					}
				}
				model.addConstr(sumA <= cap * g_rd.at(r).at(d), "H7_H1_GenderMix_RoomCap");
				model.addConstr(sumB <= cap * (1 - g_rd.at(r).at(d)), "H7_H1_GenderMix_RoomCap");
			}
		}

		/*
		S1: Age groups
		Create a new variable for this soft constraint: delta_rd = max.difference in age between 2 patients in room r on day d
		*/

		this->delta_dr = vector<vector<GRBVar>>(in.Days(), vector<GRBVar>(in.Rooms()));
		for (int d = 0; d < in.Days(); d++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				std::string varName = "d_" + std::to_string(d) + "_" + std::to_string(r);
				delta_dr.at(d).at(r) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
			}
		}

		for (int i = 0; i < in.Patients(); i++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				if (in.IncompatibleRoom(i, r))
				{
					continue;
				}
				for (int d = in.PatientSurgeryReleaseDay(i); d < std::min(in.Days(), in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i)); d++)
				{
					// Mix: Patients - Patients
					for (int j = i + 1; j < in.Patients(); j++)
					{
						if (in.IncompatibleRoom(j, r) || in.PatientSurgeryReleaseDay(j) > d || in.PatientLastPossibleDay(j) + in.PatientLengthOfStay(j) <= d)
						{
							continue;
						}

						if (in.PatientGender(i) == in.PatientGender(j))
						{
							int a_i = in.PatientAgeGroup(i);
							int a_j = in.PatientAgeGroup(j);
							model.addConstr(abs(a_i - a_j) * (q_ird.at(i).at(r).at(d) + q_ird.at(j).at(r).at(d) - 1) <= delta_dr.at(d).at(r), "S1_patient_patient");
						}
					}
					// Mix: Patients - Occupants
					for (int j = 0; j < in.Occupants(); j++)
					{
						if (d >= in.OccupantLengthOfStay(j) || r != in.OccupantRoom(j))
						{
							continue;
						}
						if (in.PatientGender(i) == in.OccupantGender(j))
						{
							int a_i = in.PatientAgeGroup(i);
							int a_j = in.OccupantAgeGroup(j);
							model.addConstr(abs(a_i - a_j) * q_ird.at(i).at(r).at(d) <= delta_dr.at(d).at(r), "S1_patient_occupant");
						}
					}
				}
			}
		}

		// Mix: Occupants - Occupants
		for (int o = 0; o < in.Occupants(); o++)
		{
			for (int d = 0; d < in.OccupantLengthOfStay(o); d++)
			{
				for (int j = o + 1; j < in.Occupants(); j++)
				{
					// Occupants need to be in the same room at the same time as well

					if (d >= in.OccupantLengthOfStay(j))
					{
						continue;
					}

					for (int r = 0; r < in.Rooms(); r++)
					{
						if (in.OccupantRoom(o) == r && in.OccupantRoom(j) == r)
						{
							int a_o = in.OccupantAgeGroup(o);
							int a_j = in.OccupantAgeGroup(j);
							model.addConstr(abs(a_o - a_j) <= delta_dr.at(d).at(r), "S1_occupant_occupant");
						}
					}
				}
			}
		}

		/*
		S8: Unscheduled optional patients
		delta_iP = 1 if optional patient is not scheduled
		*/
		this->delta_iP = std::vector<GRBVar>(in.Patients());
		for (int i = 0; i < in.Patients(); i++)
		{
			if (!in.PatientMandatory(i))
			{
				std::string varName = "delta_" + std::to_string(i);
				delta_iP.at(i) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}

		for (int i = 0; i < in.Patients(); i++)
		{
			if (!in.PatientMandatory(i))
			{
				GRBLinExpr sum_d = 0;

				for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
				{
					sum_d += x_id.at(i).at(d);
				}
				model.addConstr(1 - sum_d <= delta_iP.at(i));
			}
		}

		////// Constraints surgeons and opertaing theaters => occupants again do not matter here!

		// H3: surgeon overtime
		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				GRBLinExpr sum_i = 0;

				for (int i = 0; i < in.Patients(); i++)
				{
					if (in.PatientSurgeon(i) != c || d < in.PatientSurgeryReleaseDay(i) || d > in.PatientLastPossibleDay(i))
					{
						continue;
					}
					else
					{
						sum_i += in.PatientSurgeryDuration(i) * x_id.at(i).at(d);
					}
				}
				// x_id will be set to 0 if in.SurgeonMaxSurgeryTime(c, d) == 0
				model.addConstr(sum_i <= in.SurgeonMaxSurgeryTime(c, d), "H3_SurgeonTime");
			}
		}

		// w_it = 1 if patient i is treated in OT t
		this->w_it = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.OperatingTheaters()));
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				std::string varName = "w_" + std::to_string(i) + "_" + std::to_string(t);
				w_it.at(i).at(t) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}

		// Each patient is assigned to exactly one OT, if it is admitted (but one OT can host multiple patients on the same day)
		for (int i = 0; i < in.Patients(); i++)
		{
			GRBLinExpr sum_t = 0;

			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				sum_t += w_it.at(i).at(t);
			}

			// Note: for mandatory patients, x_id = 1
			GRBLinExpr sum_d = 0;
			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
			{
				sum_d += x_id.at(i).at(d);
			}
			model.addConstr(sum_t == sum_d, "Base_PatientToTheathre");
		}

		// Now, define the variable v_itd = 1 if patient i is assigned to theater t on day d
		// This variable is implied by x_id and w_it!
		this->v_itd = vector<vector<vector<GRBVar>>>(in.Patients(), vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(in.Days())));
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
				{
					// if (in.OperatingTheaterAvailability(t,d) > 0 && in.SurgeonMaxSurgeryTime(in.PatientSurgeon(i), d) > 0)
					// {
					std::string varName = "v_" + std::to_string(i) + "_" + std::to_string(t) + "_" + std::to_string(d);
					v_itd.at(i).at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
					// }
				}
			}
		}

		// Link the variables z_itd + use z_itd and delta_td to model S5 and use z_itd and delta_ctd to model S6
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
				{
					// if (in.OperatingTheaterAvailability(t,d) > 0 && in.SurgeonMaxSurgeryTime(in.PatientSurgeon(i), d) > 0)
					//{
					model.addConstr(v_itd.at(i).at(t).at(d) >= x_id.at(i).at(d) + w_it.at(i).at(t) - 1, "Linking_v_x_w");
					model.addConstr(v_itd.at(i).at(t).at(d) <= x_id.at(i).at(d), "Linking_v_x");
					model.addConstr(v_itd.at(i).at(t).at(d) <= w_it.at(i).at(t), "Linking_v_w");
					//}
				}
			}
		}

		/*
		S5: Number of open OTs should be minimized
		Introduce a new variable delta_td = 1 if theater t is open on day d
		Whether delta_td is 1 is implied by v_itd: if there exists at least 1 i for which v_itd = 1, then delta_td = 1
		*/

		this->delta_td = vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(in.Days()));
		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				// if (in.OperatingTheaterAvailability(t,d) > 0)
				// {
				std::string varName = "o_" + std::to_string(t) + "_" + std::to_string(d);
				delta_td.at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				// }
			}
		}

		/*
		S6: Surgeon transfers should be minimized
		Introduce the variable delta_ctd = 1 if surgeon c does a surgery in OT t on day d
		*/

		this->delta_ctd = vector<vector<vector<GRBVar>>>(in.Surgeons(), vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(in.Days())));
		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				for (int d = 0; d < in.Days(); d++)
				{
					// if (in.OperatingTheaterAvailability(t,d) > 0 && in.SurgeonMaxSurgeryTime(c,d) > 0)
					// {
					std::string varName = "v_" + std::to_string(c) + "_" + std::to_string(t) + "_" + std::to_string(d);
					delta_ctd.at(c).at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
					// }
				}
			}
		}

		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int i = 0; i < in.Patients(); i++)
			{
				for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
				{
					int c = in.PatientSurgeon(i);
					// if (in.OperatingTheaterAvailability(t,d) == 0 || in.SurgeonMaxSurgeryTime(c,d) == 0)
					// {
					//	continue;
					// }
					// S5: minimize the number of open OT's
					model.addConstr(v_itd.at(i).at(t).at(d) <= delta_td.at(t).at(d), "S5");

					// S6: minimize the number of different OT's surgeon c is assigned to on day d
					model.addConstr(v_itd.at(i).at(t).at(d) <= delta_ctd.at(c).at(t).at(d), "S5");
				}
			}
		}

		// S6: Surgeon transfers should be minimized, so only if the number of theaters that a surgeon c is assigned to on day d is > 1, do w eincur a cost
		this->delta_cd = vector<vector<GRBVar>>(in.Surgeons(), vector<GRBVar>(in.Days()));
		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				// if (in.SurgeonMaxSurgeryTime(c,d) > 0)
				// {
				std::string varName = "delta_" + std::to_string(c) + "_" + std::to_string(d);
				delta_cd.at(c).at(d) = model.addVar(0, GRB_INFINITY, 0.0, GRB_INTEGER, varName);
				// }
			}
		}

		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				// if (in.SurgeonMaxSurgeryTime(c,d) == 0)
				// {
				//	continue;
				// }

				GRBLinExpr sum_t = 0;

				for (int t = 0; t < in.OperatingTheaters(); t++)
				{
					// if (in.OperatingTheaterAvailability(t,d) > 0)
					// {
					sum_t += delta_ctd.at(c).at(t).at(d);
					// }
				}

				model.addConstr(sum_t - 1 <= delta_cd.at(c).at(d));
			}
		}

		// H4: do not exceed maximum capacity of OT t on day d
		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				// if (in.OperatingTheaterAvailability(t,d) > 0)
				// {
				GRBLinExpr sum_ci = 0;
				for (int i = 0; i < in.Patients(); i++)
				{
					if (/*in.SurgeonMaxSurgeryTime(in.PatientSurgeon(i), d) == 0
							||*/
						d < in.PatientSurgeryReleaseDay(i) ||
						d > in.PatientLastPossibleDay(i))
					{
						continue;
					}
					sum_ci += in.PatientSurgeryDuration(i) * v_itd.at(i).at(t).at(d);
				}
				model.addConstr(sum_ci <= in.OperatingTheaterAvailability(t, d), "H4_TheatreCap");
				//}
			}
		}

		// u_nrs = 1 if nurse n is assigned to room r on shift s
		this->u_nrs = vector<vector<vector<GRBVar>>>(in.Nurses(), vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(in.Shifts())));
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				for (int s = 0; s < in.Shifts(); s++)
				{
					std::string varName = "u_" + std::to_string(n) + "_" + std::to_string(r) + "_" + std::to_string(s);
					u_nrs.at(n).at(r).at(s) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);

					if (!in.IsNurseWorkingInShift(n, s))
					{
						u_nrs.at(n).at(r).at(s).set(GRB_DoubleAttr_UB, 0);
					}
				}
			}
		}

		// Assign for each room, each slot exactly 1 nurse
		// Assigning nurses to empty rooms does not incur additional costs (page 4)
		for (int r = 0; r < in.Rooms(); r++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				GRBLinExpr sum_n = 0;

				for (int n = 0; n < in.Nurses(); n++)
				{
					if (in.IsNurseWorkingInShift(n, s))
					{
						sum_n += u_nrs.at(n).at(r).at(s);
					}
				}
				model.addConstr(sum_n == 1, "Base_NurseShiftToRoom");
			}
		}

		// S3: Continuity of care: total number of distinct nurses taking care of a patient should be minimized
		// For this: additional variable b_in = 1 if nurse n treats patient i

		this->b_in = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.Nurses()));
		this->b_jn = vector<vector<GRBVar>>(in.Occupants(), vector<GRBVar>(in.Nurses()));

		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int i = 0; i < in.Patients(); i++)
			{
				std::string varName = "b_" + std::to_string(i) + "_" + std::to_string(n);
				b_in.at(i).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}

			// Same for occupants

			for (int j = 0; j < in.Occupants(); j++)
			{
				std::string varName = "b_" + std::to_string(j) + "_" + std::to_string(n);
				b_jn.at(j).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}

		// S4: workload nurses, create variable u_niks = 1 if nurse n treats patient i on it's k'th day in shift s
		// u_nik is also possible for S2, but for S4 it is more convenient to have u_niks
		// For the occupants we can keep u_njs because their skill level required and workload procuded starts from 0
		this->u_nsik = vector<vector<vector<vector<GRBVar>>>>(in.Nurses());

		for (int n = 0; n < in.Nurses(); n++)
		{
			u_nsik.at(n) = vector<vector<vector<GRBVar>>>(in.Shifts());

			for (int s = 0; s < in.Shifts(); s++)
			{
				u_nsik.at(n).at(s) = vector<vector<GRBVar>>(in.Patients());

				for (int i = 0; i < in.Patients(); i++)
				{
					u_nsik.at(n).at(s).at(i) = vector<GRBVar>(in.PatientLengthOfStay(i));
				}
			}
		}

		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				for (int s = 3 * d; s < 3 * (d + 1); s++)
				{
					for (int i = 0; i < in.Patients(); i++)
					{
						for (int k = 0; k < in.PatientLengthOfStay(i); k++)
						{
							std::string varName = "u_" + std::to_string(n) + "_" + std::to_string(s) + "_" + std::to_string(i) + "_" + std::to_string(k);
							u_nsik.at(n).at(s).at(i).at(k) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);

							if (!in.IsNurseWorkingInShift(n, s) || d < in.PatientSurgeryReleaseDay(i) || d >= in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i))
							{
								u_nsik.at(n).at(s).at(i).at(k).set(GRB_DoubleAttr_UB, 0);
							}
						}
					}
				}
			}
		}

		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				for (int s = in.ShiftsPerDay() * d; s < in.ShiftsPerDay() * (d + 1); s++)
				{
					if (!in.IsNurseWorkingInShift(n, s))
					{
						continue;
					}

					for (int r = 0; r < in.Rooms(); r++)
					{
						for (int i = 0; i < in.Patients(); i++)
						{
							if (in.IncompatibleRoom(i, r) || d < in.PatientSurgeryReleaseDay(i) || d >= in.PatientLastPossibleDay(i) + in.PatientLengthOfStay(i))
							{
								continue;
							}

							model.addConstr(u_nrs.at(n).at(r).at(s) + q_ird.at(i).at(r).at(d) - 1 <= b_in.at(i).at(n), "Linking_u_b");

							for (int k = 0; k < in.PatientLengthOfStay(i); k++)
							{
								model.addConstr(u_nrs.at(n).at(r).at(s) + q_ird.at(i).at(r).at(d) + z_idk.at(i).at(d).at(k) - 2 <= u_nsik.at(n).at(s).at(i).at(k), "Linking_u*nr_u*ni_z_id");
							}
						}

						for (int j = 0; j < in.Occupants(); j++)
						{
							if (r != in.OccupantRoom(j) || d >= in.OccupantLengthOfStay(j))
							{
								continue;
							}
							model.addConstr(u_nrs.at(n).at(r).at(s) <= b_jn.at(j).at(n), "Linking_u_b");
						}
					}
				}
			}
		}

		// S4: Maximum workload
		// delta_ns = extra workload for nurse n in shifts s
		this->delta_ns = vector<vector<GRBVar>>(in.Nurses(), vector<GRBVar>(in.Shifts()));
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				std::string varName = "delta_" + std::to_string(n) + "_" + std::to_string(s);
				delta_ns.at(n).at(s) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);

				if (!in.IsNurseWorkingInShift(n, s))
				{
					delta_ns.at(n).at(s).set(GRB_DoubleAttr_UB, 0);
				}
			}
		}

		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				if (!in.IsNurseWorkingInShift(n, s))
				{
					continue;
				}
				GRBLinExpr sum_ij = 0;

				for (int i = 0; i < in.Patients(); i++)
				{
					for (int k = 0; k < in.PatientLengthOfStay(i); k++)
					{
						int _k = 3 * k + (s % 3);
						sum_ij += (in.PatientWorkloadProduced(i, _k) * u_nsik.at(n).at(s).at(i).at(k));
					}
				}
				for (int j = 0; j < in.Occupants(); j++)
				{
					if (in.OccupantLengthOfStay(j) > std::floor(s / 3))
					{
						int r = in.OccupantRoom(j);
						sum_ij += (in.OccupantWorkloadProduced(j, s) * u_nrs.at(n).at(r).at(s));
					}
				}

				model.addConstr(sum_ij <= in.NurseMaxLoad(n, s) + delta_ns.at(n).at(s), "delta_ns");
			}
		}

		GRBLinExpr Objective = 0;

		// S1: Minimize age difference between patients in the same room: OK

		for (int d = 0; d < in.Days(); d++)
		{
			for (int r = 0; r < in.Rooms(); r++)
			{
				Objective += in.Weight(0) * delta_dr.at(d).at(r);
			}
		}

		// S2: Minimize difference between skill level patients and nurses: OK

		GRBLinExpr sumS2 = 0;
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				for (int i = 0; i < in.Patients(); i++)
				{
					for (int k = 0; k < in.PatientLengthOfStay(i); k++)
					{
						int _k = 3 * k + (s % 3);
						// Minimum skill level must be met => so if a nurse has a higher level than the patient: no cost
						if (in.PatientSkillLevelRequired(i, _k) > in.NurseSkillLevel(n))
						{
							sumS2 += (in.PatientSkillLevelRequired(i, _k) - in.NurseSkillLevel(n)) * u_nsik.at(n).at(s).at(i).at(k);
						}
					}
				}

				for (int j = 0; j < in.Occupants(); j++)
				{
					if (in.OccupantLengthOfStay(j) > std::floor(s / 3))
					{
						int r = in.OccupantRoom(j);
						if (in.OccupantSkillLevelRequired(j, s) > in.NurseSkillLevel(n))
						{
							sumS2 += (in.OccupantSkillLevelRequired(j, s) - in.NurseSkillLevel(n)) * u_nrs.at(n).at(r).at(s);
						}
					}
				}
			}
		}
		Objective += in.Weight(1) * sumS2;

		// S3: dramatisch traag
		for (int i = 0; i < in.Patients(); i++)
		{
			// S3: Minimize total number of nurses assigned to patient i
			GRBLinExpr sum_n = 0;
			for (int n = 0; n < in.Nurses(); n++)
			{
				sum_n += b_in.at(i).at(n);
			}
			Objective += in.Weight(2) * sum_n;
		}

		for (int j = 0; j < in.Occupants(); j++)
		{
			// S3: Minimize total number of nurses assigned to occupant j
			GRBLinExpr sum_n = 0;
			for (int n = 0; n < in.Nurses(); n++)
			{
				sum_n += b_jn.at(j).at(n);
			}
			Objective += in.Weight(2) * sum_n;
		}

		// S4: Minimize extra workload of nurse n in shift s: OK
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				Objective += in.Weight(3) * delta_ns.at(n).at(s);
			}
		}

		// S5: minimize the number of open theaters on day d: OK
		GRBLinExpr open_OTs_d = 0;
		for (int d = 0; d < in.Days(); d++)
		{
			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				open_OTs_d += in.Weight(4) * delta_td.at(t).at(d);
			}
		}
		Objective += open_OTs_d;

		// S6: Surgeon transfers should be minimized: OK
		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				Objective += in.Weight(5) * delta_cd.at(c).at(d);
			}
		}

		// S7: Minimize time between admission and release day: OK
		for (int i = 0; i < in.Patients(); i++)
		{
			for (int d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
			{
				Objective += in.Weight(6) * (d - in.PatientSurgeryReleaseDay(i)) * x_id.at(i).at(d);
			}
		}

		// S8: Minimize number of unscheduled optional patients: OK
		for (int i = 0; i < in.Patients(); i++)
		{
			if (!in.PatientMandatory(i))
			{
				Objective += in.Weight(7) * delta_iP.at(i);
			}
		}

		model.setObjective(Objective, GRB_MINIMIZE);

		//model.write("model.lp");

		/*
		for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < in.Days(); d++)
			{
				int sum_t = 0;
				for (int t = 0; t < in.OperatingTheaters(); t++)
				{
					if (std::abs(delta_ctd.at(c).at(t).at(d).get(GRB_DoubleAttr_X) - 1) <= EPS){
						sum_t++;
					}
				}
				if (sum_t > 0){
					std::cout << "surgeon " << c << " is assigned to " << sum_t << " rooms on day" << d << std::endl;
				}
			}
		}
		*/

		/*
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				for (int i = 0; i < in.Patients(); i++)
				{
					for (int k = 0; k < in.PatientLengthOfStay(i); k++)
					{
						if (std::abs(u_nsik.at(n).at(s).at(i).at(k).get(GRB_DoubleAttr_X) - 1) <= EPS)
						{
							std::cout << "nurse " << n << " treats patient " << i << " on it's " << k << "'th day in shift " << s << std::endl;
						}
					}
				}
			}
		}
		*/
	}
	catch (GRBException e)
	{
		std::cout << "Exception during building model" << std::endl;
		std::cout << "Error code = " << e.getErrorCode() << std::endl;
		std::cout << e.getMessage() << std::endl;
		std::abort();
	}
	catch (...)
	{
		std::cout << "Unknown exception during building model" << std::endl;
		std::abort();
	}
	std::cout << "Constructing the IP model done..." << std::endl;
}

void GurSolver::setPatients(vector<int>& PatientsId){
	this->PatientsId = PatientsId;
}

void GurSolver::setRooms(vector<int>& RoomsId){
	this->RoomsId = RoomsId;
}

void GurSolver::setDays(vector<int>& DaysId){
	this->DaysId = DaysId;
	this->StartDay = DaysId.at(0);
	this->EndDay = DaysId.at(DaysId.size()-1);
}

void GurSolver::setShifts(vector<int>& ShiftsId){
	this->ShiftsId = ShiftsId;
}

void GurSolver::setMaxFutureDay(int& MaxFutureDay){
	this->MaxFutureDay = MaxFutureDay;
}


/*
void GurSolver::PatientVariablesHard(){

	// BASE VARIABLES

	this->x_id = vector<vector<GRBVar>>(PatientsId.size(), vector<GRBVar>(DaysId.size()));
	for (int i = 0; i < PatientsId.size(); i++)
	{
		// H6: only loop over admissable days
		int i_ = PatientsId.at(i);
		for (int d = 0; d < DaysId.size(); d++)
		{
			int d_ = DaysId.at(d);

			if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}
			else{
				std::string varName = "x_" + std::to_string(i) + "_" + std::to_string(d);
				x_id.at(i).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}
	}

	this->y_ir = vector<vector<GRBVar>>(PatientsId.size(), vector<GRBVar>(in.Rooms()));
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId.at(i);

		for (int r = 0; r < in.Rooms(); r++)
		{
			// H2
			if (!in.IncompatibleRoom(i_, r))
			{
				std::string varName = "y_" + std::to_string(i) + "_" + std::to_string(r);
				y_ir.at(i).at(r) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}
	}

	init_w_it();

	// LINKING VARIABLES

	// PATIENTS DAYS SHIFTS

	std::cout << "z" << std::endl;
	this->z_idk = vector<vector<vector<GRBVar>>>(PatientsId.size());

	// z_idk is now defined differently: z_idk = 1 if patient i is 

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId.at(i);

		// We have to define this variable for the time after EndDay as well!

		z_idk.at(i) = vector<vector<GRBVar>>(in.Days()-StartDay);

		for (int d = 0; d < in.Days()-StartDay; d++)
		{
			z_idk.at(i).at(d) = vector<GRBVar>(std::min(in.PatientLengthOfStay(i_), in.Days()-StartDay-d));

			for (int k = 0; k < std::min(in.PatientLengthOfStay(i_), in.Days()-StartDay-d); k++)
			{
				std::string varName = "z_" + std::to_string(i) + "_" + std::to_string(d) + "_" + std::to_string(k);
				z_idk.at(i).at(d).at(k) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			}
		}
	}

	std::cout << "link_z" << std::endl;

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId.at(i);

		for (int d = 0; d < DaysId.size(); d++)
		{
			for (int e = d; e < std::min(in.PatientLengthOfStay(i_), in.Days()-StartDay-d); e++)
			{
				int k = e - d;
				model.addConstr(x_id.at(i).at(d) == z_idk.at(i).at(e).at(k), "Linking_x_z");
			}
		}
	}

	// PATIENTS ROOMS DAYS

	std::cout << "q" << std::endl;

	this->q_ird = vector<vector<vector<GRBVar>>>(PatientsId.size(), vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(in.Days()-StartDay)));
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		for (int r = 0; r < in.Rooms(); r++)
		{
			if (!in.IncompatibleRoom(i_, r))
			{
				for (int d = 0; d < in.Days()-StartDay; d++)
				{
					std::string varName = "q_" + std::to_string(i) + "_" + std::to_string(r) + "_" + std::to_string(d);
					q_ird.at(i).at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				}
			}
		}
	}

	std::cout << "link_q" << std::endl;

	// Link these variables:
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		for (int r = 0; r < in.Rooms(); r++)
		{
			if (in.IncompatibleRoom(i_, r))
			{
				continue;
			}
			for (int d = 0; d < in.Days()-StartDay; d++)
			{
				GRBLinExpr sum_z_idk = 0;
				for (int k = 0; k < std::min(in.PatientLengthOfStay(i_), in.Days()-StartDay-d); k++)
				{
					sum_z_idk += z_idk.at(i).at(d).at(k);
				}
				model.addConstr(q_ird.at(i).at(r).at(d) >= y_ir.at(i).at(r) + sum_z_idk - 1, "Linking_q_y_z");
				model.addConstr(q_ird.at(i).at(r).at(d) <= y_ir.at(i).at(r), "Linking_q_y");
				model.addConstr(q_ird.at(i).at(r).at(d) <= sum_z_idk, "Linking_q_z");
			}
		}
	}

	// PATIENTS OT's DAYS

	std::cout << "v" << std::endl;

	this->v_itd = vector<vector<vector<GRBVar>>>(PatientsId.size(), vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(DaysId.size())));
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int d = 0; d < DaysId.size(); d++)
			{
				int d_ = DaysId.at(d);

				if (d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_))
				{
					continue;
				}

				std::string varName = "v_" + std::to_string(i) + "_" + std::to_string(t) + "_" + std::to_string(d);
				v_itd.at(i).at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				// }
			}
		}
	}

	std::cout << "link_v" << std::endl;

	// Link the variables z_itd + use z_itd and delta_td to model S5 and use z_itd and delta_ctd to model S6
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId.at(i);

		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int d = 0; d < DaysId.size(); d++)
			{
				int d_ = DaysId.at(d);

				if (d < in.PatientSurgeryReleaseDay(i_) || d > in.PatientLastPossibleDay(i_))
				{
					continue;
				}
				else{
					model.addConstr(v_itd.at(i).at(t).at(d) >= x_id.at(i).at(d) + w_it.at(i).at(t) - 1, "Linking_v_x_w");
					model.addConstr(v_itd.at(i).at(t).at(d) <= x_id.at(i).at(d), "Linking_v_x");
					model.addConstr(v_itd.at(i).at(t).at(d) <= w_it.at(i).at(t), "Linking_v_w");
				}
			}
		}
	}

}

void GurSolver::PatientToRoom(){

	// Implicit HARD constraint

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		GRBLinExpr sum_r = 0;
		for (int r = 0; r < in.Rooms(); r++)
		{
			if (!in.IncompatibleRoom(i_, r))
			{
				sum_r += y_ir.at(i).at(r);
			}
		}

		GRBLinExpr sum_x = 0;
		for (int d = 0; d < DaysId.size(); ++d)
		{
			int d_ = DaysId.at(d);

			if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}
			else{
				sum_x += x_id.at(i).at(d);
			}
		}

		model.addConstr(sum_r == sum_x, "Base_PatientToRoom");
	}

}

void GurSolver::PatientToOT(){

	// Implicit HARD constraint

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId.at(i);

		GRBLinExpr sum_t = 0;

		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			sum_t += w_it.at(i).at(t);
		}

		// Note: for mandatory patients, x_id = 1
		GRBLinExpr sum_x = 0;
		for (int d = 0; d < DaysId.size(); ++d)
		{
			int d_ = DaysId.at(d);

			if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}
			else{
				sum_x += x_id.at(i).at(d);
			}
		}
		model.addConstr(sum_t == sum_x, "Base_PatientToTheatre");
	}
}
*/

/*
void GurSolver::S1_AgeGroups(){

	this->delta_dr = vector<vector<GRBVar>>(MaxFutureDay, vector<GRBVar>(in.Rooms()));
	for (int d = 0; d < MaxFutureDay; d++)
	{
		for (int r = 0; r < in.Rooms(); r++)
		{
			std::string varName = "d_" + std::to_string(d) + "_" + std::to_string(r);
			delta_dr.at(d).at(r) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
		}
	}

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		for (int r = 0; r < in.Rooms(); r++)
		{
			if (in.IncompatibleRoom(i_, r))
			{
				continue;
			}
			for (int d = std::max(0, in.PatientSurgeryReleaseDay(i_)-StartDay); d < std::min(in.Days()-StartDay, in.PatientLastPossibleDay(i_) + in.PatientLengthOfStay(i_)-StartDay); d++)
			{
				int d_ = StartDay + d;
				// Mix: Patients - Patients
				for (int j = i + 1; j < PatientsId.size(); j++)
				{
					int j_ = PatientsId[j];

					if (in.IncompatibleRoom(j_, r) || in.PatientSurgeryReleaseDay(j_) > d_ || in.PatientLastPossibleDay(j_) + in.PatientLengthOfStay(j_) <= d_)
					{
						continue;
					}

					if (in.PatientGender(i_) == in.PatientGender(j_))
					{
						int a_i = in.PatientAgeGroup(i_);
						int a_j = in.PatientAgeGroup(j_);
						model.addConstr(abs(a_i - a_j) * (q_ird.at(i).at(r).at(d) + q_ird.at(j).at(r).at(d) - 1) <= delta_dr.at(d).at(r), "S1_patient_patient");
					}
				}
				// Mix: Patients - Occupants
				for (auto& j: PendingPatients)
				{
					if (d > j.ReleaseDay || r != j.Room)
					{
						continue;
					}
					if (in.PatientGender(i) == j.Gender)
					{
						int a_i = in.PatientAgeGroup(i);
						int a_j = j.AgeGroup;
						model.addConstr(abs(a_i - a_j) * q_ird.at(i).at(r).at(d) <= delta_dr.at(d).at(r), "S1_patient_occupant");
					}
				}
			}
		}
	}

	// Mix: Occupants - Occupants
	for (auto& o: PendingPatients)
	{
		for (int d = o.AdmissionDay; d < o.ReleaseDay; d++)
		{
			for (auto& j: PendingPatients)
			{
				// Occupants need to be in the same room at the same time as well

				if (d >= j.ReleaseDay)
				{
					continue;
				}

				for (int r = 0; r < in.Rooms(); r++)
				{
					if (o.Room == r && j.Room == r)
					{
						int a_o = o.AgeGroup;
						int a_j = j.AgeGroup;
						model.addConstr(abs(a_o - a_j) <= delta_dr.at(d).at(r), "S1_occupant_occupant");
					}
				}
			}
		}
	}

	for (int d = 0; d < MaxFutureDay; d++)
	{
		for (int r = 0; r < in.Rooms(); r++)
		{
			Objective += in.Weight(0) * delta_dr.at(d).at(r);
		}
	}
}
*/

/*
void GurSolver::H1_NoGenderMix_H7_RoomCapacity(PartialSolution& solPart){

	// These constraints: also over entire horizon: should be compatible with future admitted patients
	// TODO

	this->g_rd = vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(DaysId.size()));
	for (int r = 0; r < in.Rooms(); r++)
	{
		for (int d = 0; d < in.Days(); ++d)
		{
			std::string varName = "g_" + std::to_string(r) + "_" + std::to_string(d);
			g_rd.at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	for (int r = 0; r < in.Rooms(); r++)
	{
		for (int d = 0; d < DaysId.size(); d++)
		{
			int d_ = DaysId.at(d);

			if (solPart.RoomsDaysPatients.at(r).at(d).size() > 0){

				int p0 = solPart.RoomsDaysPatients.at(r).at(d).at(0);

				if (p0 < in.Patients()){
					if (in.PatientGender(p0) == Gender::A){
						g_rd.at(r).at(d).set(GRB_DoubleAttr_LB, 1);
					}
					else{
						g_rd.at(r).at(d).set(GRB_DoubleAttr_UB, 0);
					}
				}
				else{
					int j_ = p0 - in.Patients();
					if (in.OccupantGender(p0) == Gender::A){
						g_rd.at(r).at(d).set(GRB_DoubleAttr_LB, 1);
					}
					else{
						g_rd.at(r).at(d).set(GRB_DoubleAttr_UB, 0);
					}
				}
			}
		}
	}

	// Define RoomCapacity for a certain time period, for a certain number of patients

	for (int r = 0; r < in.Rooms(); r++)
		{
			for (int d = 0; d < DaysId.size(); d++)
			{
				int d_ = DaysId.at(d);
				int cap = in.RoomCapacity(r);
				cap -= solPart.RoomsDaysPatients.at(r).at(d).size();

				GRBLinExpr sumA;
				GRBLinExpr sumB;
				for (int i = 0; i < PatientsId.size(); i++)
				{
					int i_ = PatientsId[i];

					if ((!in.IncompatibleRoom(i_, r)) && d_ >= in.PatientSurgeryReleaseDay(i_) && d_ <= in.PatientLastPossibleDay(i_) + in.PatientLengthOfStay(i_) - 1)
					{
						if (in.PatientGender(i_) == Gender::A)
						{
							sumA += q_ird.at(i).at(r).at(d);
						}
						else
						{
							sumB += q_ird.at(i).at(r).at(d);
						}
					}
				}
				model.addConstr(sumA <= cap * g_rd.at(r).at(d), "H7_H1_GenderMix_RoomCap");
				model.addConstr(sumB <= cap * (1 - g_rd.at(r).at(d)), "H7_H1_GenderMix_RoomCap");
			}
		}
}
*/

/*
void GurSolver::S2_MinimumSkillLevel(){

	// Variable u_nsik = 1 if nurse n treats patient i on it's k'th day in shift s
	this->u_nsik = vector<vector<vector<vector<GRBVar>>>>(in.Nurses());

	for (int n = 0; n < in.Nurses(); n++)
	{
		u_nsik.at(n) = vector<vector<vector<GRBVar>>>(in.ShiftsPerDay()*MaxFutureDay);

		for (int s = 0; s < in.ShiftsPerDay()*MaxFutureDay; s++)
		{
			u_nsik.at(n).at(s) = vector<vector<GRBVar>>(in.Patients());

			for (int i = 0; i < PatientsId.size(); i++)
			{
				int i_ = PatientsId[i];

				u_nsik.at(n).at(s).at(i) = vector<GRBVar>(in.PatientLengthOfStay(i_));
			}
		}
	}

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int d = 0; d < MaxFutureDay; d++)
		{
			int d_ = StartDay + d;

			for (int s = in.ShiftsPerDay() * d; s < in.ShiftsPerDay() * (d + 1); s++)
			{
				for (int i = 0; i < PatientsId.size(); i++)
				{
					int i_ = PatientsId[i];

					for (int k = 0; k < in.PatientLengthOfStay(i_); k++)
					{
						std::string varName = "u_" + std::to_string(n) + "_" + std::to_string(s) + "_" + std::to_string(i) + "_" + std::to_string(k);
						u_nsik.at(n).at(s).at(i).at(k) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);

						int s_ = in.ShiftsPerDay()*StartDay + s;

						if (!in.IsNurseWorkingInShift(n, s_) || d_ < in.PatientSurgeryReleaseDay(i_) || d_ >= in.PatientLastPossibleDay(i_) + in.PatientLengthOfStay(i_))
						{
							u_nsik.at(n).at(s).at(i).at(k).set(GRB_DoubleAttr_UB, 0);
						}
					}
				}
			}
		}
	}

	GRBLinExpr sumS2 = 0;
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < in.ShiftsPerDay()*MaxFutureDay; s++)
		{
			for (int i = 0; i < PatientsId.size(); i++)
			{
				int i_ = PatientsId[i];

				for (int k = 0; k < in.PatientLengthOfStay(i_); k++)
				{
					int _k = in.ShiftsPerDay()*StartDay + in.ShiftsPerDay() * k + (s % in.ShiftsPerDay());
					// Minimum skill level must be met => so if a nurse has a higher level than the patient: no cost
					if (in.PatientSkillLevelRequired(i_, _k) > in.NurseSkillLevel(n))
					{
						sumS2 += (in.PatientSkillLevelRequired(i_, _k) - in.NurseSkillLevel(n)) * u_nsik.at(n).at(s).at(i).at(k);
					}
				}
			}
		}
	}
	Objective += in.Weight(1) * sumS2;
}

void GurSolver::S3_ContinuityOfCare(){
	// S3: Continuity of care: total number of distinct nurses taking care of a patient should be minimized
	// For this: additional variable b_in = 1 if nurse n treats patient i
	// Only need to minimize the nr of new patients that nurse n sees

	this->b_in = vector<vector<GRBVar>>(PatientsId.size(), vector<GRBVar>(in.Nurses()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int i = 0; i < PatientsId.size(); i++)
		{
			std::string varName = "b_" + std::to_string(i) + "_" + std::to_string(n);
			b_in.at(i).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int d = 0; d < MaxFutureDay; d++)
		{
			int d_ = StartDay + d;

			for (int s = in.ShiftsPerDay() * d; s < in.ShiftsPerDay() * (d + 1); s++)
			{
				int s_ = in.ShiftsPerDay()*StartDay + s;

				if (!in.IsNurseWorkingInShift(n, s_))
				{
					continue;
				}

				for (int r = 0; r < in.Rooms(); r++)
				{
					for (int i = 0; i < PatientsId.size(); i++)
					{
						int i_ = PatientsId[i];

						if (in.IncompatibleRoom(i_, r) || d_ < in.PatientSurgeryReleaseDay(i_) || d_ >= in.PatientLastPossibleDay(i_) + in.PatientLengthOfStay(i_))
						{
							continue;
						}

						model.addConstr(u_nrs.at(n).at(r).at(s) + q_ird.at(i).at(r).at(d) - 1 <= b_in.at(i).at(n), "Linking_u_b");

						for (int k = 0; k < in.PatientLengthOfStay(i_); k++)
						{
							model.addConstr(u_nrs.at(n).at(r).at(s) + q_ird.at(i).at(r).at(d) + z_idk.at(i).at(d).at(k) - 2 <= u_nsik.at(n).at(s).at(i).at(k), "Linking_u*nr_u*ni_z_id");
						}
					}
				}
			}
		}
	}

	// S3: dramatisch traag
	for (int i = 0; i < PatientsId.size(); i++)
	{
		GRBLinExpr sum_n = 0;
		for (int n = 0; n < in.Nurses(); n++)
		{
			sum_n += b_in.at(i).at(n);
		}
		Objective += in.Weight(2) * sum_n;
	}
}

void GurSolver::S4_MaximumWorkload(){
	// S4: Maximum workload
	// delta_ns = extra workload for nurse n in shifts s
	this->delta_ns = vector<vector<GRBVar>>(in.Nurses(), vector<GRBVar>(in.ShiftsPerDay()*MaxFutureDay));
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < in.ShiftsPerDay()*MaxFutureDay; s++)
		{
			std::string varName = "delta_" + std::to_string(n) + "_" + std::to_string(s);
			delta_ns.at(n).at(s) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);

			int s_ = in.ShiftsPerDay()*StartDay + s;

			if (!in.IsNurseWorkingInShift(n, s_))
			{
				delta_ns.at(n).at(s).set(GRB_DoubleAttr_UB, 0);
			}
		}
	}

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < in.ShiftsPerDay()*MaxFutureDay; s++)
		{
			int s_ = in.ShiftsPerDay()*StartDay + s;

			if (!in.IsNurseWorkingInShift(n, s_))
			{
				continue;
			}
			GRBLinExpr sum_ij = 0;

			for (int i = 0; i < PatientsId.size(); i++)
			{
				int i_ = PatientsId[i];

				for (int k = 0; k < in.PatientLengthOfStay(i_); k++)
				{
					int _k = in.ShiftsPerDay()*StartDay + in.ShiftsPerDay() * k + (s % in.ShiftsPerDay());
					sum_ij += (in.PatientWorkloadProduced(i_, _k) * u_nsik.at(n).at(s).at(i).at(k));
				}
			}
			for (auto& j: PendingPatients)
			{
				if (j.ReleaseDay > std::floor(s / 3))
				{
					int r = j.Room;
					sum_ij += (j.Workload.at(s) * u_nrs.at(n).at(r).at(s));
				}
			}

			model.addConstr(sum_ij <= in.NurseMaxLoad(n, s_) + delta_ns.at(n).at(s), "delta_ns");
		}
	}

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < in.ShiftsPerDay()*MaxFutureDay; s++)
		{
			Objective += in.Weight(3) * delta_ns.at(n).at(s);
		}
	}
}

*/

/*
void GurSolver::H3_SurgeonOvertime(PartialSolution& solPart){

	for (int c = 0; c < in.Surgeons(); c++)
		{
			for (int d = 0; d < DaysId.size(); d++)
			{
				int d_ = DaysId.at(d);

				GRBLinExpr sum_i = 0;

				for (int i = 0; i < PatientsId.size(); i++)
				{
					int i_ = PatientsId[i];

					if (in.PatientSurgeon(i_) != c || d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_))
					{
						continue;
					}
					else
					{
						sum_i += in.PatientSurgeryDuration(i_) * x_id.at(i).at(d);
					}
				}

				// x_id will be set to 0 if in.SurgeonMaxSurgeryTime(c, d) == 0
				model.addConstr(sum_i <= solPart.CapacitiesSurgeons.at(c).at(d_), "H3_SurgeonTime");
			}
		}
}

void GurSolver::H4_OT_Overtime(PartialSolution& solPart){

	for (int t = 0; t < in.OperatingTheaters(); t++)
	{
		for (int d = 0; d < DaysId.size(); d++)
		{
			// if (in.OperatingTheaterAvailability(t,d) > 0)
			// {
			int d_ = DaysId.at(d);

			GRBLinExpr sum_ci = 0;

			for (int i = 0; i < PatientsId.size(); i++)
			{
				int i_ = PatientsId[i];
				// if (in.SurgeonMaxSurgeryTime(in.PatientSurgeon(i), d) == 0 ||
				if (d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_))
				{
					continue;
				}
				sum_ci += in.PatientSurgeryDuration(i_) * v_itd.at(i).at(t).at(d);
			}
			model.addConstr(sum_ci <= solPart.CapacitiesOTs.at(t).at(d_), "H4_TheatreCap");
			//}
		}
	}
}
*/

/*
void GurSolver::S5_OpenOTs_and_S6_SurgeonTransfer(){

	// S5: Number of open OTs should be minimized
	// Introduce a new variable delta_td = 1 if theater t is open on day d
	// Whether delta_td is 1 is implied by v_itd: if there exists at least 1 i for which v_itd = 1, then delta_td = 1

	this->delta_td = vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(in.Days()));
	for (int t = 0; t < in.OperatingTheaters(); t++)
	{
		for (int d = 0; d < Days; d++)
		{
			// if (in.OperatingTheaterAvailability(t,d) > 0)
			// {
			std::string varName = "o_" + std::to_string(t) + "_" + std::to_string(d);
			delta_td.at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
			// }
		}
	}

	// S6: Surgeon transfers should be minimized
	// Introduce the variable delta_ctd = 1 if surgeon c does a surgery in OT t on day d

	this->delta_ctd = vector<vector<vector<GRBVar>>>(in.Surgeons(), vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(in.Days())));
	for (int c = 0; c < in.Surgeons(); c++)
	{
		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			for (int d = 0; d < Days; d++)
			{
				// if (in.OperatingTheaterAvailability(t,d) > 0 && in.SurgeonMaxSurgeryTime(c,d) > 0)
				// {
				std::string varName = "v_" + std::to_string(c) + "_" + std::to_string(t) + "_" + std::to_string(d);
				delta_ctd.at(c).at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				// }
			}
		}
	}

	for (int t = 0; t < in.OperatingTheaters(); t++)
	{
		for (int i = 0; i < PatientsId.size(); i++)
		{
			int i_ = PatientsId[i];

			for (int d = 0; d < Days; d++){

				int d_ = StartDay + d;

				if (d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_)){
					continue;
				}
				else{
					int c = in.PatientSurgeon(i_);
					// if (in.OperatingTheaterAvailability(t,d) == 0 || in.SurgeonMaxSurgeryTime(c,d) == 0)
					// {
					//	continue;
					// }
					// S5: minimize the number of open OT's
					model.addConstr(v_itd.at(i).at(t).at(d) <= delta_td.at(t).at(d), "S5_td");

					// S6: minimize the number of different OT's surgeon c is assigned to on day d
					model.addConstr(v_itd.at(i).at(t).at(d) <= delta_ctd.at(c).at(t).at(d), "S5_ctd");
				}
			}
		}
	}

	GRBLinExpr open_OTs_d = 0;
	for (int d = 0; d < Days; d++)
	{
		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			open_OTs_d += in.Weight(4) * delta_td.at(t).at(d);
		}
	}
	Objective += open_OTs_d;

	// S6: Surgeon transfers should be minimized, so only if the number of theaters that a surgeon c is assigned to on day d is > 1, do w eincur a cost
	this->delta_cd = vector<vector<GRBVar>>(in.Surgeons(), vector<GRBVar>(Days()));
	for (int c = 0; c < in.Surgeons(); c++)
	{
		for (int d = 0; d < Days(); d++)
		{
			// if (in.SurgeonMaxSurgeryTime(c,d) > 0)
			// {
			std::string varName = "delta_" + std::to_string(c) + "_" + std::to_string(d);
			delta_cd.at(c).at(d) = model.addVar(0, GRB_INFINITY, 0.0, GRB_INTEGER, varName);
			// }
		}
	}

	for (int c = 0; c < in.Surgeons(); c++)
	{
		for (int d = 0; d < Days; d++)
		{
			// if (in.SurgeonMaxSurgeryTime(c,d) == 0)
			// {
			//	continue;
			// }

			GRBLinExpr sum_t = 0;

			for (int t = 0; t < in.OperatingTheaters(); t++)
			{
				// if (in.OperatingTheaterAvailability(t,d) > 0)
				// {
				sum_t += delta_ctd.at(c).at(t).at(d);
				// }
			}

			model.addConstr(sum_t - 1 <= delta_cd.at(c).at(d));
		}
	}

	for (int c = 0; c < in.Surgeons(); c++)
	{
		for (int d = 0; d < Days; d++)
		{
			Objective += in.Weight(5) * delta_cd.at(c).at(d);
		}
	}
}
*/

/*
void GurSolver::H5_H6_Admission(){

	for (int i = 0; i < PatientsId.size(); i++)
		{
			int i_ = PatientsId[i];

			GRBLinExpr sum_x = 0;
			for (int d = 0; d < DaysId.size(); ++d)
			{
				int d_ = DaysId.at(d);

				if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
					continue;
				}
				else{
					sum_x += x_id.at(i).at(d);
				}
			}

			if (in.PatientMandatory(i))
			{
				// Mandatory patients must be admitted
				model.addConstr(sum_x == 1, "H5_MandatoryPatientToDay");
			}
			else
			{
				// Non-mandatory patients can be postponed
				model.addConstr(sum_x <= 1, "H5_NonMandatoryPatientToDay");
			}
		}
}

void GurSolver::S7_AdmissionDelay(){

	// S7: Minimize time between admission and release day

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];
		for (int d = 0; d <= DaysId.size(); d++)
		{
			int d_ = DaysId.at(d);

			if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}
			else{
				Objective += in.Weight(6) * (d_ - in.PatientSurgeryReleaseDay(i_)) * x_id.at(i).at(d);
			}
		}
	}
}
*/

/*
void GurSolver::S8_UnscheduledPatients(){
	this->delta_iP = std::vector<GRBVar>(PatientsId.size());
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];
		if (!in.PatientMandatory(i_))
		{
			std::string varName = "delta_" + std::to_string(i);
			delta_iP.at(i) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];

		if (!in.PatientMandatory(i_))
		{
			GRBLinExpr sum_x = 0;
			for (int d = 0; d < Days; ++d)
			{
				int d_ = StartDay + d;

				if (in.PatientSurgeryReleaseDay(i_) < d_ || d_ > in.PatientLastPossibleDay(i_)){
					continue;
				}
				else{
					sum_x += x_id.at(i).at(d);
				}
			}

			model.addConstr(1 - sum_d <= delta_iP.at(i));
		}
	}

	// S8: Minimize number of unscheduled optional patients: OK
	for (int i = 0; i < PatientsId.size(); i++)
	{
		int i_ = PatientsId[i];
		if (!in.PatientMandatory(i_))
		{
			Objective += in.Weight(7) * delta_iP.at(i);
		}
	}
}

void GurSolver::HardConstraints(){

	// Implicit constraints
	PatientToRoom();
	PatientToOT();
	NurseToRoomShift();

	// Explicit constraints
	H1_NoGenderMix_H7_RoomCapacity();
	H3_SurgeonOvertime();
	H4_OT_Overtime();
	H5_H6_Admission();
}

void GurSolver::SoftConstraints(){

	S1_AgeGroups();
	S2_MinimumSkillLevel();
	S3_ContinuityOfCare();
	S4_MaximumWorkload();
	S5_OpenOTs_and_S6_SurgeonTransfer();
	S7_AdmissionDelay();
	S8_UnscheduledPatients();
}

*/


void GurSolver::OT_formulation(const int d_, Solution &sol, GRBModel &model){

	Objective = 0;

	// GOAL: Given day d_, together with set of admitted patients, we know that H3 is okay. Minimize S5 and S6.
	// Patients: all patients admitted on day d_

	vector<int>PatientsId;
	for (int i = 0; i < in.Patients(); i++){
		if (sol.AdmissionDay(i) == d_){
			// std::cout << "Patient " << i << " is admitted on day = " << d_ << std::endl;
			PatientsId.push_back(i);
		}
	}

	setPatients(PatientsId);

	// PatientsId = set of admitted patients on day d
	// y_t = 1 if OT t is opened

	std::vector<GRBVar>y_t(in.OperatingTheaters());

	for (int t = 0; t < in.OperatingTheaters(); t++){
		std::string varName = "y_" + std::to_string(t);
		y_t.at(t) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);

		// Warm start
		if(gursol.OperatingTheaterDayLoad(t, d_)) y_t[t].set(GRB_DoubleAttr_Start, 1);
	}

	// Symmetry? Rank based on capacity?
	/*
	for (int t = 1; t < in.OperatingTheaters(); t++){
		model.addConstr(y_t.at(t) <= y.at(t-1));
	}
	*/

	std::vector<GRBVar>delta_c(in.Surgeons());

	for (int c = 0; c < in.Surgeons(); c++){
		std::string varName = "delta_" + std::to_string(c);
		delta_c.at(c) = model.addVar(0, in.OperatingTheaters()-1, 0.0, GRB_INTEGER, varName);	
	}

	this->w_it = vector<vector<GRBVar>>(PatientsId.size(), vector<GRBVar>(in.OperatingTheaters()));

	for (int i = 0; i < PatientsId.size(); i++)
	{
		for (int t = 0; t < in.OperatingTheaters(); t++)
		{
			std::string varName = "w_" + std::to_string(i) + "_" + std::to_string(t);
			w_it.at(i).at(t) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	// delta_ct = 1 if surgeon c operates in OT t
	vector<vector<GRBVar>>delta_ct(in.Surgeons());
	for (int c = 0; c < in.Surgeons(); c++){
		delta_ct.at(c) = vector<GRBVar>(in.OperatingTheaters());
		for (int t = 0; t < in.OperatingTheaters(); t++){
			std::string varName = "delta_" + std::to_string(c) + " _ " + std::to_string(t);
			delta_ct.at(c).at(t) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	// Assign each patient to exactly 1 OT
	for (int i = 0; i < PatientsId.size(); i++){
		GRBLinExpr sum_t = 0;
		for (int t = 0; t < in.OperatingTheaters(); t++){
			sum_t += w_it.at(i).at(t);
		}
		model.addConstr(sum_t == 1);

		// Warm start
		w_it[i][gursol.PatientOperatingTheater(PatientsId[i])].set(GRB_DoubleAttr_Start, 1);
	}

	// Respect the capacity of the OTs
	for (int t = 0; t < in.OperatingTheaters(); t++){
		GRBLinExpr sum_i = 0;
		for (int i = 0; i < PatientsId.size(); i++){
			int i_ = PatientsId.at(i);
			sum_i += in.PatientSurgeryDuration(i_)*w_it.at(i).at(t);
		}
		model.addConstr(sum_i <=  in.OperatingTheaterAvailability(t, d_) * y_t.at(t));
	}

	// Link w_it to delta_ct
	for (int i = 0; i < PatientsId.size(); i++){
		int i_ = PatientsId.at(i);
		int c_ = in.PatientSurgeon(i_);
		for (int t = 0; t < in.OperatingTheaters(); t++){
			model.addConstr(w_it.at(i).at(t) <= delta_ct.at(c_).at(t));
		}
	}

	for (int c = 0; c < in.Surgeons(); c++){
		GRBLinExpr sum_t = 0;
		for (int t = 0; t < in.OperatingTheaters(); t++){
			sum_t += delta_ct.at(c).at(t);
		}
		model.addConstr(delta_c.at(c) >= sum_t - 1);
	}

	for (int t = 0; t < in.OperatingTheaters(); t++){
		Objective += in.Weight(4)*y_t.at(t); // S5
	}

	for (int c = 0; c < in.Surgeons(); c++){
		Objective += in.Weight(5)*delta_c.at(c); // S6
	}

#ifndef NDEBUG
	model.write("OT.lp");
#endif

	model.setObjective(Objective, GRB_MINIMIZE);
}

void GurSolver::saveSolutionOTs(const int newObj){
	// std::cout << "SAVE SOLUTION" << std::endl;
	for (int i = 0; i < PatientsId.size(); i++){								
		int i_ = PatientsId.at(i);
		// std::cout << "Unassign patient " << std::endl;
		gursol.UnassignPatientOperatingTheater(i_);
		for (int t = 0; t < in.OperatingTheaters(); t++){
			// std::cout << "w_it.at(" << i << ").at(" << t << ") = " << w_it.at(i).at(t).get(GRB_DoubleAttr_X) << std::endl;
			if (w_it.at(i).at(t).get(GRB_DoubleAttr_X)  > 0.9)
			{
				// std::cout << "Assign patient " << i_ << "(surgeon " << in.PatientSurgeon(i_) << ")" << " to theater " << t << std::endl;
				gursol.AssignPatientOperatingTheater(i_, t);
			}
		}
	}
	gursol.setObjValue(newObj);
}

void GurSolver::Nurse_formulation2(vector<int>& RoomsId, vector<int>& ShiftsId, vector<vector<bool>>& beta_in, vector<vector<bool>>& beta_jn, GRBModel& model, GRBLinExpr& Objective, bool& IsInSlotModel, vector<vector<GRBLinExpr>>& NurseWorkload, std::vector<std::array<int,3>> &assignments_nurses){

	// NEW

	// Rooms: only the necessary rooms, but over the whole horizon

	// Initialize u_nrs, b_in and delta_ns
	this->u_nrs = vector<vector<vector<GRBVar>>>(in.Nurses(), vector<vector<GRBVar>>(RoomsId.size(), vector<GRBVar>(ShiftsId.size())));
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int r = 0; r < RoomsId.size(); r++)
		{
			for (int s = 0; s < ShiftsId.size(); s++)
			{
				int s_ = ShiftsId.at(s);

				if (in.IsNurseWorkingInShift(n, s_))
				{
					std::string varName = "u_" + std::to_string(n) + "_" + std::to_string(r) + "_" + std::to_string(s);
					u_nrs.at(n).at(r).at(s) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				}
			}
		}
	}
	

	this->b_in = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.Nurses()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int i = 0; i < in.Patients(); i++)
		{
			std::string varName = "b_" + std::to_string(i) + "_" + std::to_string(n);
			b_in.at(i).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	this->b_jn = vector<vector<GRBVar>>(in.Occupants(), vector<GRBVar>(in.Nurses()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int j = 0; j < in.Occupants(); j++)
		{
			std::string varName = "b_" + std::to_string(j) + "_" + std::to_string(n);
			b_jn.at(j).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	this->delta_ns = vector<vector<GRBVar>>(in.Nurses(), vector<GRBVar>(ShiftsId.size()));
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < ShiftsId.size(); s++)
		{
			int s_ = ShiftsId.at(s);

			if (in.IsNurseWorkingInShift(n, s_))
			{
				std::string varName = "delta_" + std::to_string(n) + "_" + std::to_string(s);
				delta_ns.at(n).at(s) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
			}
		}
	}

	// Provide a warm start
	{
		int n, r, s;
		for(auto &a : assignments_nurses){
			n = a[0];
			r = a[1];
			s = a[2];

			// Sanity check on the domains
			assert(r >= 0);
			assert(r < u_nrs[n].size());
			assert(s >= 0);
			assert(s < u_nrs[n][r].size());

			// Set warm start
			u_nrs[n][r][s].set(GRB_DoubleAttr_Start, 1);
		}
	}

	/*
	for (int s = 0; s < ShiftsId.size(); s++){
		int s_ = ShiftsId[s];
		for (int r = 0; r < RoomsId.size(); r++){
			int r_ = RoomsId[r];
			int nurse = gursol.getRoomShiftNurse(r_, s_);
			u_nrs[nurse][r][s].set(GRB_DoubleAttr_Start, 1);
			for(auto &i : gursol.getPatientsRoomDay(r_, s_/in.ShiftsPerDay())){
				if(i < in.Patients()){
					b_in[i][nurse].set(GRB_DoubleAttr_Start, 1);
				} else {
					b_jn[i-in.Patients()][nurse].set(GRB_DoubleAttr_Start, 1);
				}
			}
		}
	}
		*/


	// Each room in each slot should have 1 nurse assigned

	// In each room in each shift, exactly one nurse is assigned
	// But: a nurse can be assigned to multiple rooms in one shift!!

	for (int r = 0; r < RoomsId.size(); r++)
	{
		for (int s = 0; s < ShiftsId.size(); s++)
		{
			GRBLinExpr sum_n = 0;

			for (int n = 0; n < in.Nurses(); n++)
			{
				int s_ = ShiftsId.at(s);

				if (in.IsNurseWorkingInShift(n, s_))
				{
					sum_n += u_nrs.at(n).at(r).at(s);
				}
			}
			model.addConstr(sum_n == 1, "Base_NurseShiftToRoom");
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){ 

			int s_ = ShiftsId.at(s);

			assert(gursol.NurseShiftWorkload(n, s_) > -1);

			if (!in.IsNurseWorkingInShift(n, s_))
			{
				continue;
			}

			int d = s_ / in.ShiftsPerDay();
			GRBLinExpr load_ns = 0;

			for (int r = 0; r < RoomsId.size(); r++){
				int r_ = RoomsId.at(r);
				for (int j = 0; j < gursol.getPatientsRoomDay(r_, d).size(); j++)
				{
					int p = gursol.getPatientsRoomDay(r_, d)[j];
					if (p < in.Patients())
					{
						int s1 = s_ - gursol.AdmissionDay(p) * in.ShiftsPerDay();

						// ExcessiveNurseWorkload
						load_ns += in.PatientWorkloadProduced(p, s1)*u_nrs.at(n).at(r).at(s);

						// RoomSkillLevel
						if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n))
						{
							Objective += in.Weight(1)*(in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}
						if (!beta_in.at(p).at(n)) { 
							model.addConstr(b_in.at(p).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}
					else
					{
						int j_ = p - in.Patients();
						load_ns += in.OccupantWorkloadProduced(j_, s_)*u_nrs.at(n).at(r).at(s);
						if (in.OccupantSkillLevelRequired(j_, s_) > in.NurseSkillLevel(n))
						{
							Objective += in.Weight(1)*(in.OccupantSkillLevelRequired(j_, s_) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}

						// ContinuityOfCare
						if (!beta_jn.at(j_).at(n)) { 
							model.addConstr(b_jn.at(j_).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}

				}
			}

			if (!IsInSlotModel){
				if (gursol.NurseShiftWorkload(n, s_) <= in.NurseMaxLoad(n, s_)){
					model.addConstr(load_ns + gursol.NurseShiftWorkload(n, s_) <= in.NurseMaxLoad(n, s_) + delta_ns.at(n).at(s));
				}
				else{
					model.addConstr(load_ns <= delta_ns.at(n).at(s)); // anders tel je dubbel
				}
				Objective += in.Weight(3)*delta_ns.at(n).at(s);
			}
			else{
				NurseWorkload.at(n).at(s) += load_ns;
			}
		}
		for (int i = 0; i < in.Patients(); i++){
			if (!beta_in.at(i).at(n)){
				Objective += in.Weight(2)*b_in.at(i).at(n);
			}
		}
		for (int j = 0; j < in.Occupants(); j++){
			if (!beta_jn.at(j).at(n)){
				Objective += in.Weight(2)*b_jn.at(j).at(n);
			}
		}
	}
}

void GurSolver::Nurse_formulation(vector<int>& RoomsId, vector<int>& ShiftsId, vector<vector<bool>>& beta_in, vector<vector<bool>>& beta_jn, vector<vector<int>>& CapacitiesNurses, GRBModel& model){

	Objective = 0;

	// Rooms: only the necessary rooms, but over the whole horizon

	// Initialize the subset of rooms and shifts

	setRooms(RoomsId);
	setShifts(ShiftsId);

	// Initialize u_nrs, b_in and delta_ns
	this->u_nrs = vector<vector<vector<GRBVar>>>(in.Nurses(), vector<vector<GRBVar>>(RoomsId.size(), vector<GRBVar>(ShiftsId.size())));
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int r = 0; r < RoomsId.size(); r++)
		{
			for (int s = 0; s < ShiftsId.size(); s++)
			{
				std::string varName = "u_" + std::to_string(n) + "_" + std::to_string(r) + "_" + std::to_string(s);
				u_nrs.at(n).at(r).at(s) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);

				int s_ = ShiftsId.at(s);

				if (!in.IsNurseWorkingInShift(n, s_))
				{
					u_nrs.at(n).at(r).at(s).set(GRB_DoubleAttr_UB, 0);
				}
			}
		}
	}
	

	this->b_in = vector<vector<GRBVar>>(in.Patients(), vector<GRBVar>(in.Nurses()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int i = 0; i < in.Patients(); i++)
		{
			std::string varName = "b_" + std::to_string(i) + "_" + std::to_string(n);
			b_in.at(i).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	this->b_jn = vector<vector<GRBVar>>(in.Occupants(), vector<GRBVar>(in.Nurses()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int j = 0; j < in.Occupants(); j++)
		{
			std::string varName = "b_" + std::to_string(j) + "_" + std::to_string(n);
			b_jn.at(j).at(n) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
		}
	}

	this->delta_ns = vector<vector<GRBVar>>(in.Nurses(), vector<GRBVar>(ShiftsId.size()));
	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int s = 0; s < ShiftsId.size(); s++)
		{
			std::string varName = "delta_" + std::to_string(n) + "_" + std::to_string(s);
			delta_ns.at(n).at(s) = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);

			int s_ = ShiftsId.at(s);

			if (!in.IsNurseWorkingInShift(n, s_))
			{
				delta_ns.at(n).at(s).set(GRB_DoubleAttr_UB, 0);
			}
		}
	}


	// Provide a warm start
	for (int s = 0; s < ShiftsId.size(); s++){
		int s_ = ShiftsId[s];
		for (int r = 0; r < RoomsId.size(); r++){
			int r_ = RoomsId[r];
			int nurse = gursol.getRoomShiftNurse(r_, s_);
			u_nrs[nurse][r][s].set(GRB_DoubleAttr_Start, 1);
			for(auto &i : gursol.getPatientsRoomDay(r_, s_/in.ShiftsPerDay())){
				if(i < in.Patients()){
					b_in[i][nurse].set(GRB_DoubleAttr_Start, 1);
				} else {
					b_jn[i-in.Patients()][nurse].set(GRB_DoubleAttr_Start, 1);
				}
			}
		}
	}

	// Each room in each slot should have 1 nurse assigned

	// In each room in each shift, exactly one nurse is assigned
	// But: a nurse can be assigned to multiple rooms in one shift!!

	for (int r = 0; r < RoomsId.size(); r++)
	{
		for (int s = 0; s < ShiftsId.size(); s++)
		{
			GRBLinExpr sum_n = 0;

			for (int n = 0; n < in.Nurses(); n++)
			{
				int s_ = ShiftsId.at(s);

				if (in.IsNurseWorkingInShift(n, s_))
				{
					sum_n += u_nrs.at(n).at(r).at(s);
				}
			}
			model.addConstr(sum_n == 1, "Base_NurseShiftToRoom");
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){

			assert(CapacitiesNurses.at(n).at(s) > -1); 

			int s_ = ShiftsId.at(s);

			if (!in.IsNurseWorkingInShift(n, s_))
			{
				continue;
			}

			int d = s_ / in.ShiftsPerDay();
			GRBLinExpr load_ns = 0;

			for (int r = 0; r < RoomsId.size(); r++){
				int r_ = RoomsId.at(r);
				for (int j = 0; j < gursol.getPatientsRoomDay(r_, d).size(); j++)
				{
					int p = gursol.getPatientsRoomDay(r_, d)[j];
					if (p < in.Patients())
					{
						int s1 = s_ - gursol.AdmissionDay(p) * in.ShiftsPerDay();

						// ExcessiveNurseWorkload
						load_ns += in.PatientWorkloadProduced(p, s1)*u_nrs.at(n).at(r).at(s);

						// RoomSkillLevel
						if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n))
						{
							Objective += in.Weight(1)*(in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}
						if (!beta_in.at(p).at(n)) { 
							model.addConstr(b_in.at(p).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}
					else
					{
						int j_ = p - in.Patients();
						load_ns += in.OccupantWorkloadProduced(j_, s_)*u_nrs.at(n).at(r).at(s);
						if (in.OccupantSkillLevelRequired(j_, s_) > in.NurseSkillLevel(n))
						{
							Objective += in.Weight(1)*(in.OccupantSkillLevelRequired(j_, s_) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}

						// ContinuityOfCare
						if (!beta_jn.at(j_).at(n)) { 
							model.addConstr(b_jn.at(j_).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}

				}
			}
			model.addConstr(load_ns <= CapacitiesNurses.at(n).at(s) + delta_ns.at(n).at(s));

			Objective += in.Weight(3)*delta_ns.at(n).at(s);
		}
		for (int i = 0; i < in.Patients(); i++){
			if (!beta_in.at(i).at(n)){
				Objective += in.Weight(2)*b_in.at(i).at(n);
			}
		}
		for (int j = 0; j < in.Occupants(); j++){
			if (!beta_jn.at(j).at(n)){
				Objective += in.Weight(2)*b_jn.at(j).at(n);
			}
		}
	}

	/*
	vector<int>ShiftsToDays(ShiftsId.size());
	for (int s = 0; s < ShiftsId.size(); s++){
		int s_ = ShiftsId.at(s);
		ShiftsToDays.at(s) = floor(s_ / in.ShiftsPerDay());
	}

	vector<vector<int>>ShiftIdPatientDay(ShiftsId.size(), vector<int>(in.Patients()+in.Occupants()));
	for (int s = 0; s < ShiftsId.size(); s++){
		int s_ = ShiftsId.at(s);
		for (auto& r: RoomsId){
			int d_ = ShiftsToDays.at(s);
			for (int i = 0; i < sol.getPatientsRoomDay(r, d_).size(); i++){
				int i_ = sol.getPatientsRoomDay(r, d_).at(i);
				if (i_ < in.Patients()){
					int AdmissionDay_i = sol.AdmissionDay(i_);
					ShiftIdPatientDay.at(s).at(i) = s_ - (AdmissionDay_i)*in.ShiftsPerDay();
				}
				else{
					ShiftIdPatientDay.at(s).at(i) = s_;
				}
			}
		}
	}

	// int s_ = ShiftsPerDay()*StartDay + s;
	// int d = floor(s / in.ShiftsPerDay());
	// int _k = in.ShiftsPerDay()*(StartDay + d-i.AdmissionDay-1) + s % in.ShiftsPerDay();


	// If a nurse is assigned to room r in s, then it must see all patients that lie in r in s

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){
			for (int r = 0; r < RoomsId.size(); r++){

				int r_ = RoomsId.at(r);
				int d_ = ShiftsToDays.at(s);

				for (int i = 0; i < sol.getPatientsRoomDay(r_, d_).size(); i++){

					int i_ = sol.getPatientsRoomDay(r_, d_).at(i);
					if (i_ < in.Patients()){
						if (!beta_in.at(i_).at(n)){
							// otherwise makes no sense
							model.addConstr(b_in.at(i_).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}
					else{
						int j = i_ % in.Patients();
						if (!beta_jn.at(j).at(n)){
							// otherwise makes no sense
							model.addConstr(b_jn.at(j).at(n) >= u_nrs.at(n).at(r).at(s));
						}
					}
				}
			}
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){

			int s_ = ShiftsId.at(s);
			int d_ = ShiftsToDays.at(s);

			GRBLinExpr sum_i = 0;

			for (int r = 0; r < RoomsId.size(); r++){

				int r_ = RoomsId[r];

				for (int i = 0; i < sol.getPatientsRoomDay(r_, d_).size(); i++){

					int _k = ShiftIdPatientDay.at(s).at(i);
					int i_ = sol.getPatientsRoomDay(r_, d_).at(i);

					if (i_ < in.Patients()){
						sum_i += in.PatientWorkloadProduced(i_, _k)*u_nrs.at(n).at(r).at(s);
					}
					else{
						int j = i_ % in.Patients();
						sum_i += in.OccupantWorkloadProduced(j, _k)*u_nrs.at(n).at(r).at(s);
					}

				}
			}
			model.addConstr(sum_i <= in.NurseMaxLoad(n, s_) + delta_ns.at(n).at(s));
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){
			
			int d_ = ShiftsToDays.at(s);

			for (int r = 0; r < RoomsId.size(); r++){

				int r_ = RoomsId.at(r);

				for (int i = 0; i < sol.getPatientsRoomDay(r_, d_).size(); i++){

					int i_ = sol.getPatientsRoomDay(r_, d_).at(i);
					int _k = ShiftIdPatientDay.at(s).at(i_);

					if (i_ < in.Patients()){
						if (in.PatientSkillLevelRequired(i_, _k) > in.NurseSkillLevel(n)){
							Objective += in.Weight(1)*(in.PatientSkillLevelRequired(i_, _k) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}
					}
					else{
						int j = i_ % in.Patients();
						if (in.OccupantSkillLevelRequired(j, _k) > in.NurseSkillLevel(n)){
							Objective += in.Weight(1)*(in.OccupantSkillLevelRequired(j, _k) - in.NurseSkillLevel(n))*u_nrs.at(n).at(r).at(s);
						}
					}
				}
			}
		}
	}
	*/

	/*
	for (int i = 0; i < in.Patients(); i++){
		for (int n = 0; n < in.Nurses(); n++){
			if (!beta_in.at(i).at(n)){
				Objective += in.Weight(2)*b_in.at(i).at(n);
			}
		}
	}

	for (int j = 0; j < in.Occupants(); j++){
		for (int n = 0; n < in.Nurses(); n++){
			if (!beta_jn.at(j).at(n)){
				Objective += in.Weight(2)*b_jn.at(j).at(n);
			}
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){
			Objective += in.Weight(3)*delta_ns.at(n).at(s);
		}
	}
	*/

	model.setObjective(Objective, GRB_MINIMIZE);
}

void GurSolver::saveSolutionNurses(const int newObj, vector<int>& RoomsId, vector<int>& ShiftsId){

	/*
	int n=-1;
	for (auto& r : RoomsId){
		for (auto& s : ShiftsId){
			n = gursol.getRoomShiftNurse(r,s) ;
			if(n != -1){
				gursol.UnassignNurse(n, r, s);
			}
		}
	}
		*/

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int r = 0; r < RoomsId.size(); r++)
		{
			int r_ = RoomsId.at(r);

			for (int s = 0; s < ShiftsId.size(); s++)
			{
				int s_ = ShiftsId.at(s);

				if (in.IsNurseWorkingInShift(n, s_) && u_nrs.at(n).at(r).at(s).get(GRB_DoubleAttr_X) > 0.9)
				{
					gursol.AssignNurse(n, r_, s_);
				}
			}
		}
	}

	gursol.setObjValue(newObj);

#ifndef NDEBUG
	int sum_b_in = 0;
	for (int n = 0; n < in.Nurses(); n++){
		for (int i = 0; i < in.Patients(); i++){
			if (b_in.at(i).at(n).get(GRB_DoubleAttr_X) > 0.5){
				sum_b_in += b_in.at(i).at(n).get(GRB_DoubleAttr_X);
			}
		}
	}

	int sum_b_jn = 0;
	for (int n = 0; n < in.Nurses(); n++){
		for (int j = 0; j < in.Occupants(); j++){
			if (b_jn.at(j).at(n).get(GRB_DoubleAttr_X) > 0.5){
				sum_b_jn += b_jn.at(j).at(n).get(GRB_DoubleAttr_X);
			}
		}
	}
	std::cout << "New Cost of ContinuityOfCare = " << in.Weight(2) * (sum_b_in + sum_b_jn) << " (" << in.Weight(2) << " X (" << sum_b_in << " + " << sum_b_jn << "))" << std::endl;
#endif

#ifndef NDEBUG
	int sum_delta_ns = 0;
	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < ShiftsId.size(); s++){
			if (in.IsNurseWorkingInShift(n, ShiftsId.at(s))){
				if (delta_ns.at(n).at(s).get(GRB_DoubleAttr_X) > 0.5){
					sum_delta_ns += delta_ns.at(n).at(s).get(GRB_DoubleAttr_X);
				}
			}
		}
	}

	std::cout << "New Cost of ExcessiveNurseWorkload = " << in.Weight(3) * sum_delta_ns << " (" << in.Weight(3) << " X " << sum_delta_ns << ")" << std::endl;
#endif

	return;
}

double GurSolver::getMIPgap(){
	return model.get(GRB_DoubleAttr_MIPGap);
}

int GurSolver::getObjValue(){
	return std::round(model.get(GRB_DoubleAttr_ObjVal));
}

void GurSolver::saveSolution(Solution &sol){
	sol.Reset();
	sol.setInfValue(0);
	sol.setObjValue(model.get(GRB_DoubleAttr_ObjVal));

	for (int i = 0; i < in.Patients(); i++)
	{
		int r, d, t;

		for (r = 0; r < in.Rooms(); r++)
		{
			if (!in.IncompatibleRoom(i, r))
			{
				if (std::abs(y_ir.at(i).at(r).get(GRB_DoubleAttr_X) - 1) <= EPS)
				{
					break;
				}
			}
		}

		for (d = in.PatientSurgeryReleaseDay(i); d <= in.PatientLastPossibleDay(i); d++)
		{
			if (std::abs(x_id.at(i).at(d).get(GRB_DoubleAttr_X) - 1) <= EPS)
			{
				break;
			}
		}

		for (t = 0; t < in.OperatingTheaters(); t++)
		{
			if (std::abs(w_it.at(i).at(t).get(GRB_DoubleAttr_X) - 1) <= EPS)
			{
				break;
			}
		}

		if (r < in.Rooms())
		{
			assert(d <= in.PatientLastPossibleDay(i));
			assert(t < in.OperatingTheaters());
			sol.AssignPatient(i, d, r, t);

			// Check linking constraints
			for (int e = d; e < std::min(in.Days(), d + in.PatientLengthOfStay(i)); e++)
			{
				// assert(std::abs(z_idk.at(i).at(e).get(GRB_DoubleAttr_X) - 1) <= EPS);
				assert(std::abs(q_ird.at(i).at(r).at(e).get(GRB_DoubleAttr_X) - 1) <= EPS);
			}
		}
		else
		{
			assert(!in.PatientMandatory(i));
		}
	}

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int r = 0; r < in.Rooms(); r++)
		{
			for (int s = 0; s < in.Shifts(); s++)
			{
				if (!in.IsNurseWorkingInShift(n, s))
					continue;
				if (std::abs(u_nrs.at(n).at(r).at(s).get(GRB_DoubleAttr_X) - 1) <= EPS)
				{
					sol.AssignNurse(n, r, s);
				}
			}
		}
	}

	return;
}

Solution GurSolver::solve(int timeLimitSeconds)
{
	std::cout << "I am trying so hard to solve" << std::endl;
	model.setObjective(Objective, GRB_MINIMIZE);

	// Empty output timetable with occupants already in
	try
	{
		model.optimize();

		// Check status
		int status = model.get(GRB_IntAttr_Status);
		if (status == GRB_UNBOUNDED)
		{
			cout << "!!!! Model unbounded" << std::endl;
			throw("Unbounded model");
		}
		//if (status == GRB_OPTIMAL)
		//{
		//	cout << "The optimal objective is " << model.get(GRB_DoubleAttr_ObjVal) << endl;
		//}
		if (status == GRB_INFEASIBLE)
		{
			std::cout << "!!!!! Model infeasible" << std::endl;
			throw("Unbounded model");
		}
	}
	catch (GRBException e)
	{
		std::cout << "Exception during building model" << std::endl;
		std::cout << "Error code = " << e.getErrorCode() << std::endl;
		std::cout << e.getMessage() << std::endl;
		std::abort();
	}
	catch (...)
	{
		std::cout << "Unknown exception during building model" << std::endl;
		std::abort();
	}
	return out;
}

GurSolver::~GurSolver()
{
	// Nothing to be released
}

Solution Worker::solve(int timeLimitSeconds) { 
	return GurSolver::out; 
}

Worker::Worker(const int threadId, const IHTP_Input &in, Solution &out, const int histLength, const double histPerturbValue, const int minIdle) : threadId(threadId), GurSolver(in, out), Swap(in, out), BaseAlgo(in, out, INT_MAX), worker_historyLenght(histLength), perturbValue(histPerturbValue), worker_minIdle(minIdle) {
	std::cout << "Initialized thread " << threadId << std::endl;
	//setNoThreads(1);
	model.set(GRB_IntParam_LogToConsole, 0);
}

void Worker::initList(const int obj){
	worker_history = std::vector<int>(worker_historyLenght, obj);
}

std::list<GRBVar*> Worker::Worker::fixPatientToDay(const Solution &out){

	// Fix all patient to day assignments
	// So the admission days of all admitted patients are fixed
	std::list<GRBVar*> fixedVars;
	for (int p = 0; p < in.Patients(); p++) {
		if (out.AdmissionDay(p) >= 0) {
			// Patient is admitted
			x_id[p][out.AdmissionDay(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&x_id[p][out.AdmissionDay(p)]);
		} // Else nothing: allow option to admit additional patients!
	}
	return fixedVars;
}

std::list<GRBVar*> Worker::fixPatientToOT(const Solution &out){
	std::list<GRBVar*> fixedVars;
	for (int p = 0; p < in.Patients(); p++){
		if (out.PatientOperatingTheater(p) >= 0) {
			w_it[p][out.PatientOperatingTheater(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&w_it[p][out.PatientOperatingTheater(p)]);
		}
	}
	return fixedVars;
}

std::list<GRBVar*> Worker::fixSlot(const Solution &out, const double fractionFree){
	assert(fractionFree >= 0 && fractionFree <= 1);

	std::list<GRBVar*> fixedVars;

	// Randomly select freeShifts = 10% consecutive shifts
	// All patients admitted during these time slots are free to be optimized
	const int freeShifts = std::ceil(fractionFree*in.Shifts());
	const int startPeriod = (freeShifts == in.Shifts()) ? 0 : std::rand()%(in.Shifts()-freeShifts);
	const int endPeriod = startPeriod + freeShifts;
	assert(endPeriod <= in.Shifts());
	for (int p = 0; p < in.Patients(); p++){
		// All patients admitted in startPeriod - EndPeriod are free to be admitted
		if (out.AdmissionDay(p) >= 0 && (out.AdmissionDay(p) < startPeriod || out.AdmissionDay(p) >= endPeriod)) {
			y_ir[p][out.PatientRoom(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&y_ir[p][out.PatientRoom(p)]);

			w_it[p][out.PatientOperatingTheater(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&w_it[p][out.PatientOperatingTheater(p)]);

			x_id[p][out.AdmissionDay(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&x_id[p][out.AdmissionDay(p)]);
		}
	}

	for(int s = 0; s < in.Shifts(); ++s){
		if(s >= startPeriod && s < endPeriod) continue;
		for(int r=0; r < in.Rooms(); ++r){
			if(out.getRoomShiftNurse(r,s) >= 0){
				u_nrs[out.getRoomShiftNurse(r,s)][r][s].set(GRB_DoubleAttr_LB, 1);
				fixedVars.push_back(&u_nrs[out.getRoomShiftNurse(r,s)][r][s]);
			}
		}
	}

	return fixedVars;
}

std::list<GRBVar*> Worker::fixPatient(const Solution &out, const double fractionFree){
	assert(fractionFree >= 0 && fractionFree <= 1);

	std::list<GRBVar*> fixedVars;

	// Randomly select freeShifts = 10% consecutive shifts
	// All patients admitted during these time slots are free to be optimized
	const int freeShifts = std::ceil(fractionFree*in.Shifts());
	const int startPeriod = (freeShifts == in.Shifts()) ? 0 : std::rand()%(in.Shifts()-freeShifts);
	const int endPeriod = startPeriod + freeShifts;
	assert(endPeriod <= in.Shifts());
	for (int p = 0; p < in.Patients(); p++){
		// All patients admitted in startPeriod - EndPeriod are free to be admitted
		if (out.AdmissionDay(p) >= 0 && (out.AdmissionDay(p) < startPeriod || out.AdmissionDay(p) >= endPeriod)) {
			y_ir[p][out.PatientRoom(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&y_ir[p][out.PatientRoom(p)]);

			w_it[p][out.PatientOperatingTheater(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&w_it[p][out.PatientOperatingTheater(p)]);

			x_id[p][out.AdmissionDay(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&x_id[p][out.AdmissionDay(p)]);
		}
	}

	return fixedVars;
}

std::list<GRBVar*> Worker::fixPatientToRoom(const Solution &out, const double fractionFree){
	assert(fractionFree >= 0 && fractionFree <= 1);

	std::list<GRBVar*> fixedVars;

	// Randomly select freeShifts = 10% consecutive shifts
	// --> Idea: continuity of care most important for consecutive shifts
	// Nurse to room assignments during this shift are free to be optimized, 
	// everything else is fixed.
	const int freeShifts = std::ceil(fractionFree*in.Shifts());
	// If nr of free shifts = nr of total shifts, start date is always set to 0
	// Otherwise, the start date is somewhere in [0, nr_shifts - nr_free_shifts-1]
	const int startPeriod = (freeShifts == in.Shifts()) ? 0 : std::rand()%(in.Shifts()-freeShifts); 
	const int endPeriod = startPeriod + freeShifts;
	assert(endPeriod <= in.Shifts());
	for (int p = 0; p < in.Patients(); p++){
		// All patients admitted in startPeriod - EndPeriod are free to be admitted
		if (out.PatientRoom(p) >= 0 && (out.AdmissionDay(p) < startPeriod || out.AdmissionDay(p) >= endPeriod)) {
			y_ir[p][out.PatientRoom(p)].set(GRB_DoubleAttr_LB, 1);
			fixedVars.push_back(&y_ir[p][out.PatientRoom(p)]);
		}
	}

	return fixedVars;
}

std::list<GRBVar*> Worker::fixNurseShiftToRoom(const Solution &out, const double fractionFree){
	assert(fractionFree >= 0 && fractionFree <= 1);
	std::list<GRBVar*> fixedVars;

	// Randomly select freeShifts = 10% consecutive shifts
	// --> Idea: continuity of care most important for consecutive shifts
	// Nurse to room assignments during this shift are free to be optimized, 
	// everything else is fixed.
	//const int freeShifts = std::ceil(fractionFree*in.Shifts());
	//const int startPeriod = (freeShifts == in.Shifts()) ? 0 : std::rand()%(in.Shifts()-freeShifts);
	//const int endPeriod = startPeriod + freeShifts;
	//assert(endPeriod <= in.Shifts());
	//for(int s = 0; s < in.Shifts(); ++s){
	//	if(s >= startPeriod && s < endPeriod) continue;
	//	for(int r=0; r < in.Rooms(); ++r){
	//		if(out.getRoomShiftNurse(r,s) >= 0){
	//			u_nrs[out.getRoomShiftNurse(r,s)][r][s].set(GRB_DoubleAttr_LB, 1);
	//			fixedVars.push_back(&u_nrs[out.getRoomShiftNurse(r,s)][r][s]);
	//		}
	//	}
	//}
	

	// Insight: no nurse is available during more than one shift per day
	// For continuity of care: considering at most one shift on first day is enough
	// If nurses always available during the same shift, suffices to take same shift on 
	// all days
	// Sometimes: nureses change --> on each day pick one shift at random
	// Most prob that shift over days is always the same
	
	// Determine first the number of days we want to optimize. Take days consecutively
	const int freeDays = std::ceil(fractionFree*in.Days());
	const int startDay = (freeDays == in.Days()) ? 0 : std::rand()%(in.Days()-freeDays);
	const int endDay = startDay + freeDays;
	assert(endDay <= in.Days());

	// The shift we will mostly optimize over the days
	const int selectedShift = rand() % in.ShiftsPerDay();

	// We will consider a randomly chosen set of 50% of the rooms on each day
	// If the room is in the selecterd shift of one of the days, then it is free to be optimized
	std::vector<int> roomIds(in.Rooms());
	std::iota(roomIds.begin(), roomIds.end(), 0); // populates the range [roomIds.begin(), roomIds.end()-1] with values in range(0, roomIds.end()-roomIds.begin())
	// std::random_device rd;
    // std::mt19937 g(rd());
	std::shuffle(roomIds.begin(), roomIds.end(), rng);
	// std::random_shuffle(roomIds.begin(), roomIds.end(), rand_debiel); // randomly shuffle the rooms
	int noRooms = std::max(1.0,0.5*in.Rooms()); // 50% of the rooms are free, at least 1 room

	for(int d = 0; d < in.Days(); ++d){
		int shiftFree = selectedShift;

		if(d > startDay && d < endDay){
			// For other days, small chance of 20% to pick a different shift
			// Randomly determine the shift to be free
			if(static_cast<double>(std::rand()) / RAND_MAX  > 0.8){
				shiftFree = rand() % (in.ShiftsPerDay() - 1);
				if(shiftFree >= selectedShift){
					shiftFree++; // ??
				}
			} 
		}
		assert(shiftFree <= in.ShiftsPerDay());

		for(int s = 0; s < in.ShiftsPerDay(); ++s){
			int shift = d*in.ShiftsPerDay() + s;
			for(int q=0; q < in.Rooms(); ++q){

				if(s == shiftFree && q <= noRooms) continue; // The var is free to be optimized

				// Determine the shift considered on the day
				int r = roomIds[q];
				if(out.getRoomShiftNurse(r,shift) >= 0){
					u_nrs[out.getRoomShiftNurse(r,shift)][r][shift].set(GRB_DoubleAttr_LB, 1);
					fixedVars.push_back(&u_nrs[out.getRoomShiftNurse(r,shift)][r][shift]);
				}
			}
		}
	}

	return fixedVars;
}

//void GurSolver::releaseVars(std::list<GRBVar*> &fixedVars){
//	// Set all lb's back to 0
//	// Assumes 0/1 binaries!
//	for(auto &v : fixedVars){
//		v->set(GRB_DoubleAttr_LB, 0);
//	}
//
//	return;
//}

vector<int> GurSolver::SubsetRooms(double& FracFreeRooms){
	assert(FracFreeRooms <= 1);

	vector<int>RoomsId(in.Rooms());
	std::iota(RoomsId.begin(), RoomsId.end(), 0);

	std::shuffle(RoomsId.begin(), RoomsId.end(), rng); // randomly shuffle the rooms
	// std::random_shuffle(RoomsId.begin(), RoomsId.end()); // randomly shuffle the rooms
	int noRooms = std::max(1.0, FracFreeRooms*in.Rooms()); // 50% of the rooms are free, at least 1 room
	RoomsId.resize(noRooms); // discards any excess elements after index noRooms

	return RoomsId;
}

vector<int> GurSolver::SubsetSurgeons(double& FracFreeSurgeons, std::vector<int> &days){
	// Select noSurgeons surgeons
	int noSurgeons = std::max(1.0, FracFreeSurgeons*in.Surgeons()); // 50% of the rooms are free, at least 1 room
	std::vector<int>SurgeonIds;
	SurgeonIds.reserve(in.Surgeons());

	// We want the surgeon to be available on at least one day in days
	for(int s = 0; s < in.Surgeons(); ++s){
		for(auto &d: days){
			if(in.SurgeonMaxSurgeryTime(s,d)){
				SurgeonIds.push_back(s);
				break;
			}
		}
	}

	// Shorten array if needed
	if(SurgeonIds.size() > noSurgeons){
		std::shuffle(SurgeonIds.begin(), SurgeonIds.end(), rng); 
		SurgeonIds.resize(noSurgeons); 
	}

	return SurgeonIds;
}

vector<int> GurSolver::SubsetDays(double& FracFreeDays){
	// Select freeDays consective days
	const int freeDays = std::ceil(FracFreeDays*in.Days());
	StartDay = (freeDays == in.Days()) ? 0 : std::rand()%(in.Days()-freeDays);
	vector<int>DaysId(freeDays);
	std::iota(DaysId.begin(), DaysId.end(), StartDay);

	return DaysId;
}


void GurSolver::PatientConstraints(const bool NurseFixed, GRBModel& model, GRBLinExpr& Objective, vector<int>& PatientsId, vector<int>& DaysId, vector<int>& SurgeonsId, int& firstDay, int& maxLos, 
	std::vector<std::array<int,4>> &assignments, bool addObj, vector<bool>& RoomFixed, vector<vector<GRBLinExpr>>& NurseWorkload){

#ifndef NDEBUG
		if (NurseFixed){
			for (int r = 0; r < in.Rooms(); r++){
				assert(RoomFixed[r]);
			}
		}
#endif

	// 1 if patient i is admitted in room r on day d
	q_ird = vector<vector<vector<GRBVar>>>(PatientsId.size(), vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(DaysId.size())));
	// std::vector<std::vector<std::vector<bool>>> q_exists = std::vector<std::vector<std::vector<bool>>>(PatientsId.size(), vector<vector<bool>>(in.Rooms(), vector<bool>(DaysId.size(), false)));
	q_exists = std::vector<std::vector<std::vector<bool>>>(PatientsId.size(), vector<vector<bool>>(in.Rooms(), vector<bool>(DaysId.size(), false)));
	for (int i = 0; i < PatientsId.size(); i++){
		int i_ = PatientsId.at(i);
		int c = in.PatientSurgeon(i_);
		GRBLinExpr sumq = 0;
		for (int r = 0; r < in.Rooms(); r++){
			if (in.IncompatibleRoom(i_, r)){
				continue;
			}
			for (int d = 0; d < DaysId.size(); d++){
				int d_ = DaysId.at(d);
				if (gursol.isPatientFeasibleForRoom(i_,d_,r)){

#ifndef NDEBUG
					string varName = "q_" + std::to_string(i_) + "_" + std::to_string(r) + "_" + std::to_string(d_);
					q_ird.at(i).at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
					assert(!q_exists[i][r][d]);
#else
					q_ird[i][r][d] = model.addVar(0, 1, 0.0, GRB_BINARY);
#endif
					q_exists[i][r][d] = 1;
					sumq += q_ird[i][r][d];
					
					// Patient Admission delay:
					if(addObj){
						const int costAdmissionDelay = in.Weight(6)*(d_ - in.PatientSurgeryReleaseDay(i_));
						Objective += costAdmissionDelay*q_ird.at(i).at(r).at(d);
						if (NurseFixed || RoomFixed[r]){ // New: added || RoomFixed[r]
							// Continuity of care + skill level
							// If Nurses are free: we can still compute COC and skill level for the rooms that are fixed within the horizon, if any
							int costRoomSkillLevel = 0;
							int costCOC = 0;
							std::vector<bool> seenNurses(in.Nurses(), false);
							for (int s = d_ * in.ShiftsPerDay(); s < std::min(in.Shifts(), (d_ + in.PatientLengthOfStay(i_)) * in.ShiftsPerDay()); s++) {
								int n = gursol.getRoomShiftNurse(r,s);
								if (n != -1) {
									int s1 = s - d_ * in.ShiftsPerDay();
									if (in.PatientSkillLevelRequired(i_, s1) > in.NurseSkillLevel(n)) {
										costRoomSkillLevel += in.Weight(1)*(in.PatientSkillLevelRequired(i_, s1) - in.NurseSkillLevel(n));
									}
									if (!seenNurses[n]) {
										seenNurses[n] = true;
										costCOC += in.Weight(2);
									}
								}
							}
							Objective += (costCOC + costRoomSkillLevel)*q_ird.at(i).at(r).at(d);
						}
						else{ // NEW
							// SkillLevel: We can still do this for the part of the horizon where the nurses are fixed 
							// COC: Not anymore, bc nurses to patients is free to optimized during DaysId
							if (d_ + in.PatientLengthOfStay(i_)-1 > DaysId.back()){ // d_ + in.PatientLengthOfStay(i_)-1 = day last shift of i begins if i is admitted on d_
								int costRoomSkillLevel = 0;
								for (int s = (DaysId.back()+1) * in.ShiftsPerDay(); s < std::min(in.Shifts(), (d_ + in.PatientLengthOfStay(i_)) * in.ShiftsPerDay()); s++) {
									int n = gursol.getRoomShiftNurse(r,s);
									assert(n != -1);
									int s1 = s - d_ * in.ShiftsPerDay();
									if (in.PatientSkillLevelRequired(i_, s1) > in.NurseSkillLevel(n)) {
										costRoomSkillLevel += in.Weight(1)*(in.PatientSkillLevelRequired(i_, s1) - in.NurseSkillLevel(n));
									}
								}
								Objective += costRoomSkillLevel*q_ird.at(i).at(r).at(d);
							}
						}
					}
				} 
			}
		}

		// All mandatory patients are assigned
		if(in.PatientMandatory(i_)){
			model.addConstr(sumq == 1);
		} else {
			// Assigned at most once
			model.addConstr(sumq <= 1);
			// Cost for not assigning an optional patient
			if(addObj){
				Objective += in.Weight(7) * (1 - sumq);
			}
		}
	}

          // Patient i is assigned to theatre t on day d
          w_itd = vector<vector<vector<GRBVar>>>(PatientsId.size(), vector<vector<GRBVar>>(in.OperatingTheaters(), vector<GRBVar>(DaysId.size())));
	  // std::vector<std::vector<std::vector<bool>>> w_exists = std::vector<std::vector<std::vector<bool>>>(PatientsId.size(), vector<vector<bool>>(in.OperatingTheaters(), vector<bool>(DaysId.size(), false)));
	  w_exists = std::vector<std::vector<std::vector<bool>>>(PatientsId.size(), vector<vector<bool>>(in.OperatingTheaters(), vector<bool>(DaysId.size(), false)));
          for (int i = 0; i < PatientsId.size(); i++){
                  int i_ = PatientsId.at(i);
                  int c = in.PatientSurgeon(i_);
		  int surgeryDur = in.PatientSurgeryDuration(i_);
                  for (int d = 0; d < DaysId.size(); d++){
                          int d_ = DaysId.at(d);
                          if (d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_)){
                                  continue;
                          }

			  GRBLinExpr sum_t = 0;
                          for (int t = 0; t < in.OperatingTheaters(); t++){
				  if(gursol.SurgeonDayLoad(c,d_) + surgeryDur <= in.SurgeonMaxSurgeryTime(c,d_) && gursol.OperatingTheaterDayLoad(t,d_) + surgeryDur <= in.OperatingTheaterAvailability(t,d_)){
#ifndef DEBUG
                                  	string varName = "w_" + std::to_string(i_) + "_" + std::to_string(t) + "_" + std::to_string(d_);
                                  	w_itd.at(i).at(t).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
#else
                                  	w_itd[i][t][d] = model.addVar(0, 1, 0.0, GRB_BINARY);
#endif
					sum_t += w_itd[i][t][d];
					w_exists[i][t][d] = 1;
				  } 
                          }

			 GRBLinExpr sum_q = 0;
			 for (int r = 0; r < in.Rooms(); r++){
				 if(q_exists[i][r][d]){
					sum_q += q_ird[i][r][d];
				 } 
			 }

			 if(sum_t.size() > 0 || sum_q.size() > 0){ // At least one possibility to assign patient on day
				model.addConstr(sum_t == sum_q);
			 }
                  }
          }

	  // Warm start with the prev solution
	  for(auto &a: assignments){
		int i=a[0], d=a[1], r=a[2], t=a[3];
		assert(q_exists[i][r][d]);
		q_ird[i][r][d].set(GRB_DoubleAttr_Start, 1);
		assert(w_exists[i][t][d]);
		w_itd[i][t][d].set(GRB_DoubleAttr_Start, 1);
	  }


	// OT and surgeon capacity
	for (int d = 0; d < DaysId.size(); d++){
		int d_ = DaysId.at(d);
		for (int t = 0; t < in.OperatingTheaters(); t++){
			GRBLinExpr sum_i = 0;
			for (int i = 0; i < PatientsId.size(); i++){
				if(w_exists[i][t][d]){
					sum_i += in.PatientSurgeryDuration(PatientsId[i])*w_itd.at(i).at(t).at(d);
				}
			}
			// Patients assigned is not more than the remaining capacity
			if(sum_i.size() > 0){
				model.addConstr(sum_i <= in.OperatingTheaterAvailability(t,d_) - gursol.OperatingTheaterDayLoad(t,d_));
			}
		}

		for(auto &c : SurgeonsId){
			GRBLinExpr sum_i = 0;
			for (int i = 0; i < PatientsId.size(); i++){
				int i_ = PatientsId.at(i);
				if (in.PatientSurgeon(i_) != c || d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_)){
					continue;
				}
				for (int t = 0; t < in.OperatingTheaters(); t++){
					if(w_exists[i][t][d]){
						sum_i += in.PatientSurgeryDuration(i_)*w_itd.at(i).at(t).at(d);
					}
				}
			}
			if(sum_i.size() > 0){
				model.addConstr(sum_i <= in.SurgeonMaxSurgeryTime(c, d_) - gursol.SurgeonDayLoad(c, d_));
			}
		}
	}

	// No gender mix + room capacity
	g_rd = vector<vector<GRBVar>>(in.Rooms(), vector<GRBVar>(DaysId.size()));
	for (int r = 0; r < in.Rooms(); r++) {
		for (int d = 0; d < DaysId.size(); d++){
                        int d_ = DaysId.at(d);

			if(gursol.RoomDayLoad(r,d_) == 0){
				// Determine whether it is an A or a B room
#ifndef DEBUG
				std::string varName = "g_" + std::to_string(r) + "_" + std::to_string(d);
				g_rd.at(r).at(d) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
#else
				g_rd[r][d] = model.addVar(0, 1, 0.0, GRB_BINARY);
#endif

				GRBLinExpr genderA = 0;
				GRBLinExpr genderB = 0;
				int i_;
				for (int i = 0; i < PatientsId.size(); i++){
					i_ = PatientsId[i];	
					for(int e = std::max(0, d - in.PatientLengthOfStay(i_) + 1); e <= d; ++e){
						if (q_exists[i][r][e]){
							if(in.PatientGender(i_) == Gender::A){
								genderA += q_ird[i][r][e];
							} else {
								genderB += q_ird[i][r][e];
							}
						}
					}
				}
				if(genderA.size() > 0){
					model.addConstr(genderA <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))*g_rd[r][d]);
				} 
				if(genderB.size() > 0){
					model.addConstr(genderB <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))*(1 - g_rd[r][d]));
				} 
			} else {
				// Room capacity only
				GRBLinExpr patients = 0;
				int i_;
				for (int i = 0; i < PatientsId.size(); i++){
					i_ = PatientsId[i];
					for(int e = std::max(0, d - in.PatientLengthOfStay(i_) + 1); e <= d; ++e){
						if(q_exists[i][r][e]){
							patients += q_ird[i][r][e];
						}
					}
				}
				if(patients.size() > (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))){
					model.addConstr(patients <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_)));
				}
			}
		}

		// Enforce room cap in days not in daysId
		// Note: q_ird not made if gender clash in days not considered, thanks to isPatientFeasibleForRoom
		// LastDay = DaysId.back()
		for(int d_ = DaysId.back()+1; d_ < in.Days(); ++d_){
			// Room capacity only
			GRBLinExpr patientsA = 0;
			GRBLinExpr patientsB = 0;
			int i_;
			for (int i = 0; i < PatientsId.size(); i++){
				i_ = PatientsId[i];

				// Compute last possible day on which we can start such that patient is still in room in d_
				int index = (d_ - in.PatientLengthOfStay(i_) + 1) - firstDay;
				//std::cout << "D: " << d_ << std::endl;
				//std::cout << "First day: " << firstDay << std::endl;
				//std::cout << "Last day: " << lastDay << std::endl;
				//std::cout << "Los: " << in.PatientLengthOfStay(i_) << std::endl;
				//std::cout << "INdex: " << index << std::endl;
				assert(index < 0 || index >= DaysId.size() || DaysId[index] + in.PatientLengthOfStay(i_) - 1 == d_);

				// Look for first day in daysId so that patient starts on that day and is still active on d_
				for(int d = std::max(0,index); d < DaysId.size(); ++d){
					if(q_exists[i][r][d]){
						if(in.PatientGender(i_) == Gender::A){
							patientsA += q_ird[i][r][d];
						} else {
							patientsB += q_ird[i][r][d];
						}

					}
				}
			}

			if(gursol.RoomDayLoad(r, d_) > 0){
				// At most one will be one...
				if(patientsA.size() + patientsB.size() > (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))){
					std::string con = "CapRoom_" + std::to_string(r) + "_in_day_" + std::to_string(d_);
					model.addConstr(patientsA + patientsB <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_)), con);
				}
			} else if(patientsA.size() + patientsB.size() > 0){
				// Determine the gender...
				std::string conA = "CapRoomA_" +std::to_string(r) + "_in_day_" + std::to_string(d_);
				std::string conB = "CapRoomB_" +std::to_string(r) + "_in_day_" + std::to_string(d_);
				std::string varName = "GenderVar_" + std::to_string(r) + "_" + std::to_string(d_);
				GRBVar genderVar = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				model.addConstr(patientsA <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))*genderVar, conA);
				model.addConstr(patientsB <= (in.RoomCapacity(r) - gursol.RoomDayLoad(r,d_))*(1-genderVar), conB);
			}
		}
	}


	// Opening new OT's
	if(addObj){
		for (int t = 0; t < in.OperatingTheaters(); t++) {
			for (int d = 0; d < DaysId.size(); d++){
				int d_ = DaysId.at(d);
				if(gursol.OperatingTheaterDayLoad(t, d_) == 0){
					  GRBLinExpr sum_t = 0;
					  std::string varName = "e_" + std::to_string(t) + "_" + std::to_string(d);
					  GRBVar e_td = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
					  for (int i = 0; i < PatientsId.size(); i++){
						  if(w_exists[i][t][d]){
							model.addConstr(w_itd[i][t][d] <= e_td);
						  } 
					  }
					  Objective += e_td*in.Weight(4);
				}
			}
		}
	}

	// Surgeon transfers
	if(addObj){
		int i_, i, d, t, c, c_;
		// fctd = 1 if surgeon c active in theatre t on day d
		std::vector<std::vector<std::vector<GRBVar>>> fctd(SurgeonsId.size(), std::vector<std::vector<GRBVar>>(in.OperatingTheaters(), std::vector<GRBVar>(DaysId.size())));
		std::vector<std::vector<std::vector<bool>>> f_exists(SurgeonsId.size(), std::vector<std::vector<bool>>(in.OperatingTheaters(), std::vector<bool>(DaysId.size(), false)));
		for(i=0; i < PatientsId.size(); ++i){
			i_ = PatientsId[i];
			c_ = in.PatientSurgeon(i_);
			for(c = 0; c < SurgeonsId.size(); ++c){
				if(c_ == SurgeonsId[c]) break;
			}
			for(t = 0; t < in.OperatingTheaters(); ++t){
				for(d=0; d < DaysId.size(); ++d){
					if(w_exists[i][t][d]){
						if(!f_exists[c][t][d]){
				  			std::string varName = "f_" + std::to_string(c) + "_" + std::to_string(t) + "_" + std::to_string(d);
							fctd[c][t][d] = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
							f_exists[c][t][d] = true;
						}
						model.addConstr(w_itd[i][t][d] <= fctd[c][t][d]);
					}
				}
			}
		}

		for(c = 0; c < SurgeonsId.size(); ++c){
			for(d=0; d < DaysId.size(); ++d){
				GRBLinExpr sum_t = 0;
				for(t = 0; t < in.OperatingTheaters(); ++t){
					if(f_exists[c][t][d]){
						sum_t += fctd[c][t][d];
					}
				}
				if(sum_t.size() > 1){
					std::string varName = "h_" + std::to_string(c) + "_" + std::to_string(d);
					GRBVar hcd = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
					model.addConstr(sum_t - 1 <= hcd);
					Objective += in.Weight(5)*hcd;
				}
			}
		}
	}

	// Excessive nurse workload
	if (addObj){
		if (NurseFixed){
			// p_ns is execessive workload for nurse n in shift s
			const int noShiftsToConsider = in.ShiftsPerDay()*(std::min((int) DaysId.size() + maxLos - 1, in.Days()-firstDay));;

			// u_nis = Workload of patient i in shift s for nurse n
			// Since the nurse to rooms are fixed, we can compute this using a linear expression
			std::vector<std::vector<std::vector<GRBLinExpr>>> u_nis(in.Nurses(), std::vector<std::vector<GRBLinExpr>>(PatientsId.size(), std::vector<GRBLinExpr>(noShiftsToConsider, 0)));
			int d, d_, i, i_, r, n, s, s_, l;
			for(i=0; i < PatientsId.size(); ++i){
				i_ = PatientsId[i];
				for(r = 0; r < in.Rooms(); ++r){
					for(d=0; d < DaysId.size(); ++d){
						d_ = DaysId[d];
						if(q_exists[i][r][d]){
							for(l = 0, s_ = d_*in.ShiftsPerDay(), s = d*in.ShiftsPerDay(); l < in.PatientLengthOfStay(i_)*in.ShiftsPerDay(); ++l, ++s_, ++s){
								if(s_ >= in.Shifts()) break;
								assert(s < noShiftsToConsider);
								u_nis[gursol.getRoomShiftNurse(r, s_)][i][s] += q_ird[i][r][d]*in.PatientWorkloadProduced(i_,l);
							}
						}
					}
				}
			}			

			// Determine the workload
			for(n=0; n < in.Nurses(); ++n){
				for(s=0; s < noShiftsToConsider; ++s){
					s_ = s + firstDay*in.ShiftsPerDay(); 
					assert(s_ < in.Shifts());
					if(in.IsNurseWorkingInShift(n, s_)){
						GRBLinExpr sum = 0;
						for(i = 0; i < PatientsId.size(); ++i){
							sum += u_nis[n][i][s];
						}
						if(sum.size() > 0){
							std::string varName = "p_" + std::to_string(n) + "_" + std::to_string(s);
							GRBVar p_ns = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
							model.addConstr(sum <= std::max(0, in.NurseMaxLoad(n, s_) - gursol.NurseShiftLoad(n, s_)) + p_ns);
							Objective += in.Weight(3)*p_ns;
						}
					}
				}
			}
		}
		else{
			// p_ns is execessive workload for nurse n in shift s
			// maxLos-1 because we at least have already done 1 day in the hospital, i.e. the day of admission

			const int noShiftsToConsider = in.ShiftsPerDay()*(std::min((int) maxLos-1, in.Days()-DaysId.back()-1));;

			// u_nis = Workload of patient i in shift s for nurse n
			// Since the nurse to rooms are fixed, we can compute this using a linear expression
			std::vector<std::vector<std::vector<GRBLinExpr>>> u_nis(in.Nurses(), std::vector<std::vector<GRBLinExpr>>(PatientsId.size(), std::vector<GRBLinExpr>(noShiftsToConsider, 0)));
			int d, d_, i, i_, r, n, s, sp, sn;
			for(i=0; i < PatientsId.size(); ++i){
				i_ = PatientsId[i];
				for(r = 0; r < in.Rooms(); ++r){
					for(d=0; d < DaysId.size(); ++d){
						d_ = DaysId[d];
						if (d_ + in.PatientLengthOfStay(i_)-1 > DaysId.back()){ 
							// Nurses are still fixed after this period!
							if(q_exists[i][r][d]){
								sp = (DaysId.back()+1-d_)* in.ShiftsPerDay();
								for (sn = 0, s = (DaysId.back()+1) * in.ShiftsPerDay(); s < std::min(in.Shifts(), (d_ + in.PatientLengthOfStay(i_)) * in.ShiftsPerDay()); ++s, ++sn) {
									n = gursol.getRoomShiftNurse(r,s);
									assert(n != -1);
									assert(sn < noShiftsToConsider);
									u_nis[n][i][sn] += q_ird[i][r][d]*in.PatientWorkloadProduced(i_, sp);
									++sp;
								}
							}
						}
						if (RoomFixed.at(r) && q_exists[i][r][d]){
							// If the room is fixed, we need to take into account the NurseWorkload of the free patients,
							// because FreenurseConstraints only goes over the free rooms for the free patients!!
							sn = d*in.ShiftsPerDay();
							for (sp = 0, s = d_ * in.ShiftsPerDay(); s < std::min(DaysId.back()+1, (d_ + in.PatientLengthOfStay(i_)))* in.ShiftsPerDay(); ++s, ++sp) {
								n = gursol.getRoomShiftNurse(r,s);
								assert(n != -1);
								assert(sn < DaysId.size()*in.ShiftsPerDay());
								NurseWorkload.at(n).at(sn) += q_ird[i][r][d]*in.PatientWorkloadProduced(i_, sp);
								++sn;
							}
						}
					}
				}
			}	

			// Determine the workload
			for(n=0; n < in.Nurses(); ++n){
				for(sn = (DaysId.back()+1)*in.ShiftsPerDay(), s=0; s < noShiftsToConsider; ++s, ++sn){
					assert(sn < in.Shifts());
					if(in.IsNurseWorkingInShift(n, sn)){
						GRBLinExpr sum = 0;
						for(i = 0; i < PatientsId.size(); ++i){
							sum += u_nis[n][i][s];
						}
						if(sum.size() > 0){
							std::string varName = "p_" + std::to_string(n) + "_" + std::to_string(s);
							GRBVar p_ns = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
							model.addConstr(sum <= std::max(0, in.NurseMaxLoad(n, sn) - gursol.NurseShiftLoad(n, sn)) + p_ns);
							Objective += in.Weight(3)*p_ns;
						}
					}
				}
			}
		}
	}

	// Age differences
		const int daysToConsider = std::min((int) DaysId.size() + maxLos - 1, in.Days()-firstDay);
		std::vector<std::vector<int>> minAge(daysToConsider, std::vector<int>(in.Rooms(), INT_MAX));
		std::vector<std::vector<int>> maxAge(daysToConsider, std::vector<int>(in.Rooms(), -1));
		std::vector<std::vector<GRBVar>> minAgeVar(daysToConsider, std::vector<GRBVar>(in.Rooms()));
		std::vector<std::vector<GRBVar>> maxAgeVar(daysToConsider, std::vector<GRBVar>(in.Rooms()));
	if(addObj){
		// Days themselves, and the days during which someone may still be in the room

		// Compute the min and max age currently in the room
		int d, d_,r, age;
		for(d=0; d < daysToConsider; ++d){
			d_ = firstDay + d;
			assert(d >= DaysId.size() || d_ == DaysId[d]);
			for(r=0; r < in.Rooms(); ++r){
				for(auto &p: gursol.getPatientsRoomDay(r, d_)){
					age = (p < in.Patients() ?  in.PatientAgeGroup(p) : in.OccupantAgeGroup(p - in.Patients()));
					if(age < minAge[d][r]){
						minAge[d][r] = age;
					} 
					if(age > maxAge[d][r]){
						maxAge[d][r] = age;
					}
				}
			}
		}

		// min and max age vars per day per room
		int i, i_, d2;
		int bigM = in.AgeGroups() - 1;
		for(d=0; d < daysToConsider; ++d){
			for(r=0; r < in.Rooms(); ++r){
				if(maxAge[d][r] - minAge[d][r] == in.AgeGroups()){
					// The age difference is already maximal...
					continue;
				}
#ifndef NDEBUG
				std::string varName = "MinAge_" + std::to_string(firstDay + d) + "_" + std::to_string(r);
				minAgeVar[d][r] = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
				varName = "MaxAge_" + std::to_string(firstDay + d) + "_" + std::to_string(r);
				maxAgeVar[d][r] = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, varName);
#else
				minAgeVar[d][r] = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS);
				maxAgeVar[d][r] = model.addVar(0, GRB_INFINITY, 0.0, GRB_CONTINUOUS);
#endif
				bool atLeastOne = false;
				for(i=0; i < PatientsId.size(); ++i){
					GRBLinExpr sum  = 0;
					i_ = PatientsId[i];
					age = in.PatientAgeGroup(i_);
					// Last day on which i can start and still be in the room during d
					for(d2 = std::max(0,d - in.PatientLengthOfStay(i_) + 1); d2 <= std::min((int) DaysId.size()-1, d); ++d2){
						if(q_exists[i][r][d2]){
							sum += q_ird[i][r][d2];
						}
					}
					if(sum.size() > 0){
						atLeastOne = true;
						model.addConstr(minAgeVar[d][r] <= bigM  - (bigM - age)*sum);
						model.addConstr(age*sum <= maxAgeVar[d][r]);
					}
				}

				if(atLeastOne){
					// New cost
					Objective += in.Weight(0)*(maxAgeVar[d][r] - minAgeVar[d][r]);

					// Needed if room is empty
					model.addConstr(minAgeVar[d][r] <= maxAgeVar[d][r]);
					
					// Room not empty
					if(maxAge[d][r] > -1){
						assert(minAge[d][r] < INT_MAX);
						// Old age cons
						model.addConstr(minAgeVar[d][r] <= minAge[d][r]);
						model.addConstr(maxAge[d][r] <= maxAgeVar[d][r]);

						// Old cost
						Objective -= in.Weight(0)*(maxAge[d][r] - minAge[d][r]);
					}
				}
			}
		}
	}

}

void GurSolver::SavePatients(vector<int>& PatientsId, vector<int>& DaysId, vector<int>& RoomsId, std::vector<std::array<int,4>>& newAssignments, const bool NurseFixed){
	// Save the assignments
	// Only do the actual assignments after retrieving all sols (needed to check whether variable exists)
	// std::vector<std::array<int,4>> newAssignments;

	/*
	if (!NurseFixed){
		std::cout << "*** Patients in rooms *** " << std::endl;
		for (int d = 0; d < in.Days(); d++){
			std::cout << "Day " << d << std::endl;
			if (DaysId.at(0) <= d && d <= DaysId.back()){
				std::cout << "! " << std::endl;
			}
			for (int r = 0; r < in.Rooms(); r++){
				for (auto& p: gursol.getPatientsRoomDay(r, d)){
					std::cout << "Patient " << p << " is in room " << r << " in day " << d << std::endl;
				}
			}
		}

		for (int i = 0; i < in.Patients(); i++){
			if (std::find(PatientsId.begin(), PatientsId.end(), i) != PatientsId.end()){
				continue;
			}
			for (int n = 0; n < in.Nurses(); n++){
				if (b_in.at(i).at(n).get(GRB_DoubleAttr_X) > 0.9){
					std::cout << "Patient " << i << " sees nurse " << n << std::endl;
				}
			}
		}
		std::cout << "***********" << std::endl;
	}
	*/

	for(int i=0; i < PatientsId.size(); ++i){
		int i_ = PatientsId[i];
		int c = in.PatientSurgeon(i_);
		int assignedR=-1;
		int assignedT=-1;
		int assignedD=-1;


		int d, d_;
		for (d = 0; d < DaysId.size(); d++){
			d_ = DaysId.at(d);
			if ( d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}

			for (int r = 0; r < in.Rooms(); r++){
				if(q_exists[i][r][d] && q_ird[i][r][d].get(GRB_DoubleAttr_X) > 0.9){
					assignedR = r;
					assignedD = d_;
					break;
				}

			}
			if(assignedR != -1){
				break;
			}
		}

		if(assignedD != -1){
			// std::cout << "Patient " << i_ << " is admitted on day " << assignedD << " in room " << assignedR << std::endl;
			// Retrieve theater
                          for (int t = 0; t < in.OperatingTheaters(); t++){
				  if(w_exists[i][t][d] && w_itd[i][t][d].get(GRB_DoubleAttr_X) > 0.9){
					assignedT = t;
					break;
				  }
			  }
			  assert(assignedT != -1);
			  newAssignments.push_back({i_,assignedD,assignedR,assignedT});
		}
	}

	if (!NurseFixed){
		for (int n = 0; n < in.Nurses(); n++)
		{
			for (int r = 0; r < RoomsId.size(); r++)
			{
				for (int s = 0; s < DaysId.size()*in.ShiftsPerDay(); s++)
				{
					int s_ = DaysId.at(0)*in.ShiftsPerDay() + s;

					if (in.IsNurseWorkingInShift(n, s_) && u_nrs.at(n).at(r).at(s).get(GRB_DoubleAttr_X) > 0.9){

						int r_ = RoomsId.at(r);
//#ifndef NDEBUG
//						std::cout << "Assign nurse " << n << " to room " << r_ << " and shift " << s_ << std::endl;
//#endif
						gursol.AssignNurse(n, r_, s_);
					}
				}
			}
		}
		/*
		for (int s = 0; s < in.Shifts(); s++){
			for (int r = 0;r  < in.Rooms(); r++){
				int n = gursol.getRoomShiftNurse(r, s);
				std::cout << "Nurse " << n << " is present in room " << r << " in shift " << s << std::endl;
			}
		}
			*/
	}
}

void GurSolver::FreeNurseConstraints(GRBModel& model, GRBLinExpr& Objective, vector<int>& PatientsId, vector<int>& DaysId, vector<int>& RoomsId, vector<vector<GRBLinExpr>>& NurseWorkload){
	
	int StartDay = DaysId[0];
	int StartShift = in.ShiftsPerDay()*(int)StartDay;
	int NrShifts = in.ShiftsPerDay()*(int)DaysId.size();

	assert(u_nrs.size() > 0);
	assert(b_in.size() > 0); // i is defined for in.Patients()!! in Nurse_formulation
	assert(b_jn.size() > 0);
	assert(delta_ns.size() > 0);

	vector<vector<vector<vector<GRBVar>>>>y_nies(in.Nurses(), vector<vector<vector<GRBVar>>>(PatientsId.size()));

	for (int n = 0; n < in.Nurses(); n++)
	{
		for (int i = 0; i < PatientsId.size(); i++)
		{

			int i_ = PatientsId.at(i);
			int e_max = std::min(in.PatientLengthOfStay(i_), (int)DaysId.size())*in.ShiftsPerDay();

			y_nies.at(n).at(i) = vector<vector<GRBVar>>(e_max, vector<GRBVar>(NrShifts)); 

			for (int e = 0; e < e_max; e++){
				for (int s = 0; s < NrShifts; s++){
					std::string varName = "y_nies";
					y_nies.at(n).at(i).at(e).at(s) = model.addVar(0, 1, 0.0, GRB_BINARY, varName);
				}

			}
		}
	}

	for (int i = 0; i < PatientsId.size(); i++){
		int i_ = PatientsId.at(i);
		for (int d = 0; d < DaysId.size(); d++){
			int d_ = DaysId.at(d);
			if (d_ < in.PatientSurgeryReleaseDay(i_) || d_ > in.PatientLastPossibleDay(i_)){
				continue;
			}
			for (int s = d*in.ShiftsPerDay(); s < std::min(d+in.PatientLengthOfStay(i_), (int)DaysId.size())*in.ShiftsPerDay(); s++){ // Shift starting from StartDay
				int e = s - d*in.ShiftsPerDay(); // Shift of the patient

				assert(e < std::min(in.PatientLengthOfStay(i_), (int)DaysId.size())*in.ShiftsPerDay());
				assert(s < NrShifts);

				for (int n = 0; n < in.Nurses(); n++){
					if (!in.IsNurseWorkingInShift(n, StartShift+s))
					{
						continue;
					}

					//for (int r = 0; r < in.Rooms(); r++){
					for (int r = 0; r < RoomsId.size(); r++){
						int r_ = RoomsId.at(r);
						if (q_exists[i][r_][d]){ // this also depends on other factors like OT and surgeon capacity!!
							model.addConstr(y_nies.at(n).at(i).at(e).at(s) >= q_ird.at(i).at(r_).at(d) + u_nrs.at(n).at(r).at(s) - 1);
							model.addConstr(b_in.at(i_).at(n) >= q_ird.at(i).at(r_).at(d) + u_nrs.at(n).at(r).at(s) - 1);
						}
					}

					if (in.PatientSkillLevelRequired(i_, e) > in.NurseSkillLevel(n)){
						Objective += in.Weight(1)*(in.PatientSkillLevelRequired(i_, e) - in.NurseSkillLevel(n))*y_nies.at(n).at(i).at(e).at(s);
					}
				}
			}
			if (d + in.PatientLengthOfStay(i_) > DaysId.size()){
				// for (int r = 0; r < in.Rooms(); r++){
				for (int r = 0; r < RoomsId.size(); r++){
					int r_ = RoomsId.at(r);
					if (q_exists[i][r_][d]){
						int start_s = (DaysId.back()+1)*in.ShiftsPerDay();
						int min_s = std::min(d_+in.PatientLengthOfStay(i_), in.Days())*in.ShiftsPerDay();
						for (int s = start_s; s < min_s ; s++){
							int n = gursol.getRoomShiftNurse(r_, s);
							assert(n != -1);
							model.addConstr(b_in.at(i_).at(n) >= q_ird.at(i).at(r_).at(d));
						}
					}
				}
			}
		}
	}

	for (int n = 0; n < in.Nurses(); n++){
		for (int s = 0; s < NrShifts; s++){
			int s_ = s + StartDay*in.ShiftsPerDay();
			if (!in.IsNurseWorkingInShift(n, s_))
			{
				continue;
			}
			GRBLinExpr sum_ie = 0;
			for (int i = 0; i < PatientsId.size(); i++){
				int i_ = PatientsId.at(i);
				int e_max = std::min(in.PatientLengthOfStay(i_), (int)DaysId.size())*in.ShiftsPerDay();
				for (int e = 0; e < e_max; e++){
					sum_ie += in.PatientWorkloadProduced(i_, e)*y_nies.at(n).at(i).at(e).at(s);
				}
			}

			assert(s < NurseWorkload.at(n).size());
			model.addConstr(sum_ie + NurseWorkload.at(n).at(s) <= std::max(in.NurseMaxLoad(n, s_)-gursol.NurseShiftWorkload(n, s_), 0) + delta_ns.at(n).at(s));

			Objective += in.Weight(3)*delta_ns.at(n).at(s);
		}
	}
}

int GurSolver::MandatoryPatientsOnly(const int noThreads, const int timeLimit){

	// Model to assign all mandatory patients

	GRBModel model = GRBModel(env);
	model.set(GRB_IntParam_LogToConsole, 0);
	model.set(GRB_IntParam_Threads, noThreads);
        model.set(GRB_DoubleParam_TimeLimit, timeLimit);
	model.set(GRB_IntParam_Seed, rand());

	// All mandatory patients
	PatientsId.clear();
	int maxLos = 0;
	for (int p = 0; p < in.Patients(); p++){ 
		if(in.PatientMandatory(p)){
			PatientsId.push_back(p);
			if(in.PatientLengthOfStay(p) > maxLos){
				maxLos = in.PatientLengthOfStay(p);
			}
		}
	}

	// All days, all surgeons
	DaysId.clear();
	for(int d=0; d < in.Days(); ++d){
		DaysId.push_back(d);
	}
	std::vector<int> SurgeonsId;
	for(int s=0; s < in.Surgeons(); ++s){
		SurgeonsId.push_back(s);
	}

	GRBLinExpr obj = 0;
	const bool NurseFixed = true;
	int firstDay = 0;
	std::vector<std::array<int,4>> assignments;
	std::vector<bool>RoomFixed(in.Rooms(), true); // RoomFixed: for the nurses!!
	vector<vector<GRBLinExpr>>NurseWorkload; // Not needed here, only when considering nurses
	PatientConstraints(NurseFixed, model, Objective, PatientsId, DaysId, SurgeonsId, firstDay, maxLos, assignments, false, RoomFixed, NurseWorkload);

#ifndef NDEBUG
	model.write("Mandatory.lp");
#endif

	model.optimize();
	int ModelRunTime = model.get(GRB_DoubleAttr_Runtime);
	if (model.get(GRB_IntAttr_Status) == GRB_INFEASIBLE || model.get(GRB_IntAttr_Status) == GRB_INF_OR_UNBD) { 
		std::cout << "No feasible solution found within time limit..." << std::endl;
		return 0;
	} else {
		std::cout << "A feasible starting solution was found by IP within " << ModelRunTime << " seconds" << std::endl;

		//bool NurseFixed = true;
		vector<int>RoomsId; // not needed here, only needed when NurseFixed=false
		try {
			SavePatients(PatientsId, DaysId, RoomsId, assignments, NurseFixed);
		}catch(...) {
			std::cout << "Error occured during saving of patients" << std::endl;
			return 0;	
		}
		
		for(auto &a : assignments){
			gursol.AssignPatient(a[0],a[1],a[2],a[3]);
#ifndef NDEBUG
			std::cout << "Assign patient " << a[0] << " with los " << in.PatientLengthOfStay(a[0]) << " and age " << in.PatientAgeGroup(a[0]) << " on day " << a[1] << " in room " << a[2] << " and ot " << a[3] << std::endl;
#endif
		}


		return 1;
	}
}

int GurSolver::RebuildOptimizePatientsDays(const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime, const bool NurseFixed){
	// Reoptimize all patients admitted during a subset of the days and belonging to a subset of the surgeons
	// double FracFreeDays = 0.1;
	// double FracFreeSurgeons = 0.1;

	double FracFreeDays = destrSize.at(0);
	double FracFreeSurgeons = destrSize.at(1);
	double FracFreeRooms;
	if (!NurseFixed){
		FracFreeRooms = destrSize.at(2);
	}

	// Note no need to reconstruct environment, see https://groups.google.com/g/gurobi/c/O7Kht25-OIA
	// Environment would involve reading license file etc. and thus has significant overhead!
	GRBModel model = GRBModel(env);
	model.set(GRB_IntParam_LogToConsole, 0);
	// Absolute gap should be less than 1! (Objective is integer)
	// If working with relative gap, the best solution can be lost because numbers are large
	// E.g. 8001 vs 80000 results in very small gap...
        model.set(GRB_DoubleParam_MIPGapAbs, 0.5); 
        model.set(GRB_DoubleParam_MIPGap, 0); 
	model.set(GRB_IntParam_Threads, 1);
        model.set(GRB_DoubleParam_TimeLimit, maxRunTime);
	model.set(GRB_IntParam_Seed, rand());

	// TODO TODO Time limit

	// Determine the days and surgeons
	std::vector<int> DaysId = SubsetDays(FracFreeDays);
	std::vector<int> SurgeonsId = SubsetSurgeons(FracFreeSurgeons, DaysId);
	std::set<int> SurgeonSet(SurgeonsId.begin(), SurgeonsId.end());

	std::vector<int> RoomsId;
	std::vector<bool>RoomFixed(in.Rooms(), true); // RoomFixed: for the nurses!!
	if (!NurseFixed){
		RoomsId = SubsetRooms(FracFreeRooms);
		assert(RoomsId.size() > 0);
		for (auto& r: RoomsId){
			RoomFixed[r] = false;
		}
	}

	int objValNow = gursol.getObjValue();

#ifndef NDEBUG
	std::cout.setstate(std::ios::failbit);
	gursol.PrintCosts();
	std::cout.clear();
	std::cout << "ObjValue before: " << objValNow << std::endl;
	std::cout << "Real: " << gursol.getObjValue() << std::endl;
	assert(objValNow == gursol.getObjValue());
	assert(gursol.getInfValue() == 0);
#endif

	// Unassign all patients admitted within DaysId or belonging to one of the surgeons in SurgeonsId
	int firstDay = DaysId[0];
	int lastDay = DaysId.back();
	int currentAssignmentCost = 0;
	int currentUnassignedCost = 0;
	int maxLos = 0;
	int r,t;
	PatientsId.clear();
	std::vector<std::array<int,4>> assignments;
	for (int p = 0; p < in.Patients(); p++){ 
		int d = gursol.AdmissionDay(p);
		if (firstDay <= d && d <= lastDay && SurgeonSet.count(in.PatientSurgeon(p))){
			r = gursol.PatientRoom(p);
			t = gursol.PatientOperatingTheater(p);
			gursol.UnassignPatient(p);
			assert(gursol.isPatientFeasibleForRoom(p, d, r));
			currentAssignmentCost += gursol.CostEvaluationAddingPatient(false, p, r, d, t) + (in.PatientMandatory(p) ? 0 : in.Weight(7));
			assignments.push_back({static_cast<int>(PatientsId.size()), d-firstDay, r, t});
			PatientsId.push_back(p);
//#ifndef NDEBUG
//			std::cout << "Unassign " << p << " (" << in.PatientMandatory(p) << ") on day " << d << " in room " << r << " and theater " << t << std::endl;
//#endif
			if(in.PatientLengthOfStay(p) > maxLos) maxLos = in.PatientLengthOfStay(p);
		} else if(d == -1 && SurgeonSet.count(in.PatientSurgeon(p))){
			assert(!in.PatientMandatory(p));
			PatientsId.push_back(p);
			currentUnassignedCost += in.Weight(7);
			if(in.PatientLengthOfStay(p) > maxLos) maxLos = in.PatientLengthOfStay(p);
		}
	}

	// If no patients were removed or can be added, stop
	if(PatientsId.size() == 0) return gursol.getObjValue();

	/**************
	*  IP MODEL  *
	**************/

	vector<vector<GRBLinExpr>>NurseWorkload(in.Nurses(), vector<GRBLinExpr>(DaysId.size()*in.ShiftsPerDay(), 0));

	// Reset the objective
	Objective = 0;
	PatientConstraints(NurseFixed, model, Objective, PatientsId, DaysId, SurgeonsId, firstDay, maxLos, assignments, true, RoomFixed, NurseWorkload);
	std::vector<std::array<int,3>> assignments_nurses;
	int costNurses = 0;

	if (!NurseFixed){ 

		vector<int>ShiftsId(DaysId.size()*in.ShiftsPerDay());
		std::iota(ShiftsId.begin(), ShiftsId.end(), DaysId.at(0)*in.ShiftsPerDay());

		bool IsInSlotModel = true;
		vector<vector<bool>>beta_in(in.Patients(), vector<bool>(in.Nurses(), false));
		vector<vector<bool>>beta_jn(in.Occupants(), vector<bool>(in.Nurses(), false));
		gursol.ComputeBetas(RoomsId, ShiftsId, beta_in, beta_jn); 
		
		costNurses = UnassignNurses(RoomsId, ShiftsId, assignments_nurses);

#ifndef NDEBUG		
		for (auto& p: PatientsId){
			for (int n = 0; n < in.Nurses(); n++){
				assert(!beta_in.at(p).at(n));
			}
		}
#endif
		Nurse_formulation2(RoomsId, ShiftsId, beta_in, beta_jn, model, Objective, IsInSlotModel, NurseWorkload, assignments_nurses);
		FreeNurseConstraints(model, Objective, PatientsId, DaysId, RoomsId, NurseWorkload); // put delta_ns here and only here in the objective!
	}
	model.setObjective(Objective, GRB_MINIMIZE);
	model.optimize();
#ifndef NDEBUG
	model.write("PatientModel.lp");
#endif
	ModelRunTime = model.get(GRB_DoubleAttr_Runtime);

	std::vector<std::array<int,4>> newAssignments;
	SavePatients(PatientsId, DaysId, RoomsId, newAssignments, NurseFixed);


	for(auto &a : newAssignments){
		gursol.AssignPatient(a[0],a[1],a[2],a[3]);
//#ifndef NDEBUG
//		std::cout << "Assign patient " << a[0] << " with los " << in.PatientLengthOfStay(a[0]) << " and age " << in.PatientAgeGroup(a[0]) << " on day " << a[1] << " in room " << a[2] << " and ot " << a[3] << std::endl;
//#endif
	}

	int IPValue = std::round(model.get(GRB_DoubleAttr_ObjVal));
	int objValue = std::round(objValNow - currentUnassignedCost - currentAssignmentCost - costNurses + IPValue);
	gursol.setObjValue(objValue);

#ifndef NDEBUG
	std::cout.setstate(std::ios::failbit);
	gursol.PrintCosts();
	std::cout.clear();
	std::cout << "Now: " << objValNow << std::endl;
	std::cout << "Assignment cost: " << currentAssignmentCost << std::endl;
	std::cout << "Unassigned cost: " << currentUnassignedCost << std::endl;
	std::cout << "Nurse cost: " << costNurses << std::endl;
	std::cout << "Claim: " << objValue << std::endl;
	std::cout << "gursol.getObjValue() = " << gursol.getObjValue() << std::endl;
	std::cout << "IP Value: " << IPValue << std::endl;
	assert(objValue == gursol.getObjValue());
	std::cout << "Check: " << objValue << " vs " << objValNow << std::endl;
	std::cout << "Gurobi runtime: " << model.get(GRB_DoubleAttr_Runtime) << std::endl;
	std::cout << "IP Value: " << IPValue << std::endl;
	if(objValue > objValNow){
		// Cannot happen: local search...

		for(auto & a : assignments){
			int i=a[0], d=a[1], r=a[2], t=a[3];
			assert(w_exists[i][t][d]);
			assert(q_exists[i][r][d]);
			model.addConstr(w_itd[i][t][d] == 1);
			model.addConstr(q_ird[i][r][d] == 1);
		}

		if (!NurseFixed){
			for (auto& a: assignments_nurses){
				int n=a[0], r=a[1], s=a[2];
				model.addConstr(u_nrs[n][r][s] == 1);
			}
		}

		model.optimize();
		if (model.get(GRB_IntAttr_Status) == GRB_INFEASIBLE || model.get(GRB_IntAttr_Status) == GRB_INF_OR_UNBD) { 
			model.write("test.lp");
			std::cout << "Model is infeasible. Computing IIS...\n";

			// Compute IIS
			model.computeIIS();

			// Retrieve constraints
			GRBConstr* constraints = model.getConstrs();
			int numConstrs = model.get(GRB_IntAttr_NumConstrs);

			std::cout << "Minimal infeasible subset of constraints:\n";
			for (int i = 0; i < numConstrs; i++) {
				if (constraints[i].get(GRB_IntAttr_IISConstr) == 1) {
					std::cout << "  " << constraints[i].get(GRB_StringAttr_ConstrName) << "\n";
				}
			}

			// Clean up allocated memory
			delete[] constraints;
			std::abort();
		} else {
			std::cout << "Model with fixed cons: " << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
			int IPValue = std::round(model.get(GRB_DoubleAttr_ObjVal));
			int objValue = std::round(objValNow - currentUnassignedCost - currentAssignmentCost - costNurses + IPValue);
			std::cout << "IP Value fixed: " << IPValue << std::endl;
			std::cout << "Obj value when fixed: " << objValue << std::endl;
		}

	}


	assert(objValue <= objValNow); // Pure local search

	assert(gursol.getInfValue() == 0);
	std::cout << "All done" << std::endl;
#endif

	return objValue;
}

int GurSolver::UnassignNurses(vector<int>& RoomsId, vector<int>& ShiftsId, std::vector<std::array<int,3>>& assignments_nurses){

	int currentAssignmentCost = 0;
	int n, r, r_, s, s_;
	bool verbose=false;

	for (r_ = 0; r_ < RoomsId.size(); r_++){ 
		r = RoomsId[r_];
		for (s_ = 0; s_ < ShiftsId.size(); s_++){
			s = ShiftsId[s_];
			n = gursol.getRoomShiftNurse(r, s);
			gursol.UnassignNurse(n, r, s);
			assert(in.IsNurseWorkingInShift(n, s));
			currentAssignmentCost += gursol.CostEvaluationAddingNurse(verbose, n, r, s);
			assignments_nurses.push_back({n, r_, s_});
		}
	}

	return currentAssignmentCost;
}

void GurSolver::SubsetShifts(vector<int>& ShiftsId, int& StartDay, int& EndDay, double& FracFreeShifts){

	assert(FracFreeShifts <= 1);
	const int freeDays = std::ceil(FracFreeShifts*in.Days());
	StartDay = (freeDays == in.Days()) ? 0 : std::rand()%(in.Days()-freeDays);
	EndDay = StartDay + freeDays;
	assert(EndDay <= in.Days());

	int StartShift = in.ShiftsPerDay()*StartDay;
	int EndShift = in.ShiftsPerDay()*EndDay;

	ShiftsId.erase(ShiftsId.begin(), ShiftsId.begin()+StartShift);
	ShiftsId.resize(EndShift-StartShift);
}

int GurSolver::RebuildOptimizeNurses(const std::vector<double> destrSize, double& ModelRunTime, double maxRunTime){

	GRBModel model = GRBModel(env);
	model.set(GRB_IntParam_LogToConsole, 0);
	model.set(GRB_IntParam_Threads, 1);
        model.set(GRB_DoubleParam_TimeLimit, maxRunTime);
        model.set(GRB_DoubleParam_MIPGapAbs, 0.5); 
        model.set(GRB_DoubleParam_MIPGap, 0); 
	model.set(GRB_IntParam_Seed, rand());


	double FracFreeRooms = destrSize.at(0);
	double FracFreeShifts = destrSize.at(1);
	double objValNow = gursol.getObjValue();

	vector<int>ShiftsId(in.Shifts());
	std::iota(ShiftsId.begin(), ShiftsId.end(), 0);

	vector<int>RoomsId;

	int StartDay = 0;
	int EndDay = in.Days()-1;

	int StartShift = 0;
	int EndShift = in.ShiftsPerDay()*(in.Days()-1);
	
	double rand = static_cast<double>(std::rand()) / RAND_MAX;
	if (rand < 0.33){
		// Optimize nurses over all rooms, but a subset of the slots
		double FracAllRooms = 1.0;
		RoomsId = SubsetRooms(FracAllRooms);
		assert(RoomsId.size() > in.Rooms()-1);
		SubsetShifts(ShiftsId, StartDay, EndDay, FracFreeShifts);
	}
	else if (rand < 0.67){
		// Optimize nurses over all slots, but only a subset of the rooms
		RoomsId = SubsetRooms(FracFreeRooms);
	}
	else{
		RoomsId = SubsetRooms(FracFreeRooms);
		SubsetShifts(ShiftsId, StartDay, EndDay, FracFreeShifts);
	}

	// At least one room and slot
	assert(RoomsId.size() > 0);
	assert(ShiftsId.size() > 0);

#ifndef NDEBUG
	std::cout << "Optimizing Shifts: " << std::endl;
	for(auto &p: ShiftsId){
		std::cout << p << "\t";
	}
	std::cout << std::endl;

	std::cout << "Optimizing Rooms: " << std::endl;
	for(auto &r: RoomsId){
		std::cout << r << "\t";
	}
	std::cout << std::endl;
#endif

	// Deduct costs related to the free rooms and slots
	// Also include occupants!!

	// vector<vector<int>>CapacitiesNurses(in.Nurses(), vector<int>(ShiftsId.size(), -1));
	vector<vector<bool>>beta_in(in.Patients(), vector<bool>(in.Nurses(), false));
	vector<vector<bool>>beta_jn(in.Occupants(), vector<bool>(in.Nurses(), false));
	
	// int cost = gursol.CostEvaluationRemovingNurseRoomsShifts(RoomsId, ShiftsId, beta_in, beta_jn, CapacitiesNurses);
	gursol.ComputeBetas(RoomsId, ShiftsId, beta_in, beta_jn); // N
	std::vector<std::array<int,3>> assignments_nurses;
	int cost = UnassignNurses(RoomsId, ShiftsId, assignments_nurses); // N
	objValNow -= cost;  

	bool IsInSlotModel = false; // N

	Objective = 0; // N
	vector<vector<GRBLinExpr>>NurseWorkload; // empty vector, only needed when combining it with PatientModel
	Nurse_formulation2(RoomsId, ShiftsId, beta_in, beta_jn, model, Objective, IsInSlotModel, NurseWorkload, assignments_nurses); // N
	// Nurse_formulation(RoomsId, ShiftsId, beta_in, beta_jn, CapacitiesNurses, model);
	model.setObjective(Objective, GRB_MINIMIZE); // N
	model.optimize();
	ModelRunTime = model.get(GRB_DoubleAttr_Runtime);
	int objValue = std::round(objValNow + model.get(GRB_DoubleAttr_ObjVal));
	saveSolutionNurses(objValue, RoomsId, ShiftsId); // Save the new best solution

#ifndef NDEBUG
	int dummy = gursol.getObjValue();
	std::cout.setstate(std::ios::failbit);
	gursol.PrintCosts();
	std::cout.clear();
	if(objValue != gursol.getObjValue()){
		std::cout << "cost found by gurobi = " << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
		std::cout << "Now: " << objValNow << std::endl;
		std::cout << "Claim: " << objValue << std::endl;
		std::cout << "Real: " << gursol.getObjValue() << std::endl;
		assert(objValue == gursol.getObjValue());
	}
	// Local search
	if(objValue > dummy){
		std::cout << "ObjValue: " << objValue << std::endl;
		std::cout << "Dummy: " << dummy << std::endl;
		assert(objValue <= dummy);
	}
#endif

	return objValue;
}

//bool Worker::optimizeNurses(const Solution &sol, std::shared_mutex &solMutex, double destrSize){
//	// Fix all patients to days, and resolve the model...
//	std::list<GRBVar*> fixedVars;
//
//	// We are going to perform actions that involve reading the solution object
//	// --> Forbid any other thread to make adaptions to sol
//	{ 
//		std::shared_lock<std::shared_mutex> lock(solMutex);
//		if(model.get(GRB_IntAttr_SolCount) == 0 || sol.getObjValue() < model.get(GRB_DoubleAttr_ObjVal) - EPS){
//			// Warm start: we have not found any solution yet, or another thread found a new best solution
//			std::cout << "Load solution with value " << sol.getObjValue() << std::endl;
//			loadSolution(sol);
//		}
//		// x.splice(x.end(), y): moves the list y at the end of x in x
//		fixedVars.splice(fixedVars.end(), fixPatientToDay(sol)); // admission days of all admitted patients are fixed (x_id)
//		fixedVars.splice(fixedVars.end(), fixPatientToRoom(sol, 0)); // assignment of patients to rooms during certain period are fixed (y_ir)
//		fixedVars.splice(fixedVars.end(), fixPatientToOT(sol)); // assignment of patients to OT's is fixed (w_it)
//		fixedVars.splice(fixedVars.end(), fixNurseShiftToRoom(sol, destrSize)); // assignment of nurses their shifts to rooms (u_nrs)
//	}
//
//
//	// Solve and release variables
//	GurSolver::solve(-1);
//	//std::cout << "Nurse gap: " << model.get(GRB_DoubleAttr_MIPGap) << std::endl;
//	releaseVars(fixedVars);
//	return (model.get(GRB_DoubleAttr_MIPGap) <= 0.001);
//}

//bool Worker::optimizePatient(const Solution &sol, std::shared_mutex &solMutex, double destrSize){
//	std::list<GRBVar*> fixedVars;
//
//	// We are going to perform actions that involve reading the solution object
//	// --> Forbid any other thread to make adaptions to sol
//	{ 
//		std::shared_lock<std::shared_mutex> lock(solMutex);
//		if(model.get(GRB_IntAttr_SolCount) == 0 || sol.getObjValue() < model.get(GRB_DoubleAttr_ObjVal) - EPS){
//			// Warm start: we have not found any solution yet, or another thread found a new best solution
//			std::cout << "Load solution with value " << sol.getObjValue() << std::endl;
//			loadSolution(sol);
//		}
//		fixedVars.splice(fixedVars.end(), fixPatient(sol, destrSize)); // fix all variables of patients (x_id, y_ir, w_it) that are admitted outside a certain period
//		fixedVars.splice(fixedVars.end(), fixNurseShiftToRoom(sol, 0));
//	}
//
//	GurSolver::solve(-1);
//	releaseVars(fixedVars);
//	//std::cout << "Patient gap: " << model.get(GRB_DoubleAttr_MIPGap) << std::endl;
//	return (model.get(GRB_DoubleAttr_MIPGap) <= 0.001);
//}

int GurSolver::RebuildOptimizeOT(double& ModelRunTime, double maxRunTime){

	// Note no need to reconstruct environment, see https://groups.google.com/g/gurobi/c/O7Kht25-OIA
	// Environment would involve reading license file etc. and thus has significant overhead!
	GRBModel model = GRBModel(env);
	model.set(GRB_IntParam_LogToConsole, 0);
	model.set(GRB_IntParam_Threads, 1);
        model.set(GRB_DoubleParam_TimeLimit, maxRunTime);
        model.set(GRB_DoubleParam_MIPGapAbs, 0.5); 
        model.set(GRB_DoubleParam_MIPGap, 0); 
	model.set(GRB_IntParam_Seed, rand());

	double objValNow = gursol.getObjValue();
	const int d_ = std::rand()%in.Days();

	// Evaluate cost for freeing this day
	
	objValNow -= gursol.costEvaluationOTDay(d_);

#ifndef NDEBUG
	std::cout << "I substract " << gursol.costEvaluationOTDay(d_) << std::endl;
#endif
	// Build the OT mode

	OT_formulation(d_, gursol, model);

	model.optimize();
	ModelRunTime = model.get(GRB_DoubleAttr_Runtime);
	int objValue = std::round(objValNow + model.get(GRB_DoubleAttr_ObjVal));
	saveSolutionOTs(objValue);

#ifndef NDEBUG
	std::cout.setstate(std::ios::failbit);
	gursol.PrintCosts();
	std::cout.clear();
	int dummy = gursol.getObjValue();
	assert(objValue == gursol.getObjValue());
	// Local search
	assert(objValue <= dummy);
#endif

	return objValue;
}

//bool Worker::optimizeOT(const Solution &sol, std::shared_mutex &solMutex){
//	std::list<GRBVar*> fixedVars;
//
//	// We are going to perform actions that involve reading the solution object
//	// --> Forbid any other thread to make adaptions to sol
//	{ 
//		std::shared_lock<std::shared_mutex> lock(solMutex);
//		if(model.get(GRB_IntAttr_SolCount) == 0 || sol.getObjValue() < model.get(GRB_DoubleAttr_ObjVal) - EPS){
//			// Warm start: we have not found any solution yet, or another thread found a new best solution
//			std::cout << "Load solution with value " << sol.getObjValue() << std::endl;
//			loadSolution(sol);
//		}
//		fixedVars.splice(fixedVars.end(), fixPatientToDay(sol));
//		fixedVars.splice(fixedVars.end(), fixPatientToRoom(sol, 0));
//		fixedVars.splice(fixedVars.end(), fixNurseShiftToRoom(sol, 0));
//	}
//
//	GurSolver::solve(-1);
//	releaseVars(fixedVars);
//	//std::cout << "OT gap: " << model.get(GRB_DoubleAttr_MIPGap) << std::endl;
//	return (model.get(GRB_DoubleAttr_MIPGap) <= 0.001);
//}


//bool Worker::optimizeSlot(const Solution &sol, std::shared_mutex &solMutex, double destrSize){
//	std::list<GRBVar*> fixedVars;
//
//	// We are going to perform actions that involve reading the solution object
//	// --> Forbid any other thread to make adaptions to sol
//	{ 
//		std::shared_lock<std::shared_mutex> lock(solMutex);
//		if(model.get(GRB_IntAttr_SolCount) == 0 || sol.getObjValue() < model.get(GRB_DoubleAttr_ObjVal) - EPS){
//			// Warm start: we have not found any solution yet, or another thread found a new best solution
//			std::cout << "Load solution with value " << sol.getObjValue() << std::endl;
//			loadSolution(sol);
//		}
//		fixedVars.splice(fixedVars.end(), fixSlot(sol, destrSize));
//	}
//
//	GurSolver::solve(-1);
//	releaseVars(fixedVars);
//	//std::cout << "Patient gap: " << model.get(GRB_DoubleAttr_MIPGap) << std::endl;
//	return (model.get(GRB_DoubleAttr_MIPGap) <= 0.001);
//}

//void optimizeThread(int id, Worker* slave, Solution& bestSol, double& bestObjValue, std::shared_mutex& solMutex, std::chrono::steady_clock::time_point endTime, double &destrSizeNurse, double &destrSizePatient, double &destrSizeSlots, std::shared_mutex& destrMutex) {
//
//	// Construct the model
//	slave->Constraints();
//	do {
//
//		// Arbitrarily pick one of two operators
//		std::string choiceStr = "Nurses";
//		double rand = static_cast<double>(std::rand()) / RAND_MAX;
//		bool solvedOpt;
//
//		if(rand <= 0.32){
//			double dummy;
//			{
//				std::shared_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				dummy = destrSizeNurse;
//			}
//#ifndef NDEBUG
//			std::cout << "Optimize nurses: " << dummy << std::endl;
//#endif
//			solvedOpt = slave->optimizeNurses(bestSol, solMutex, dummy); // Each thread improves its local solution
//			{
//				std::unique_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				if(!solvedOpt){
//					destrSizeNurse = std::max(0.01, destrSizeNurse - 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size nurse to: " << destrSizeNurse << std::endl;
//#endif
//				} else {
//					destrSizeNurse = std::min(0.99, destrSizeNurse + 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size nurse to: " << destrSizeNurse << std::endl;
//#endif
//				}
//			}
//		} else if (rand <= 0.64) {
//			double dummy;
//			{
//				std::shared_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				dummy = destrSizeSlots;
//			}
//			choiceStr = "Slots";
//#ifndef NDEBUG
//			std::cout << "Optimize slots: " << dummy << std::endl;
//#endif
//			solvedOpt = slave->optimizeSlot(bestSol, solMutex, dummy); // Each thread improves its local solution
//			{
//				std::unique_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				if(!solvedOpt){
//					destrSizeSlots = std::max(0.01, destrSizeSlots - 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size slots to: " << destrSizeSlots << std::endl;
//#endif
//				} else {
//					destrSizeSlots = std::min(0.99, destrSizeSlots + 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size slots to: " << destrSizeSlots << std::endl;
//#endif
//				}
//			}
//		} else if (rand <= 0.96) {
//			double dummy;
//			{
//				std::shared_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				dummy = destrSizePatient;
//			}
//			choiceStr = "Patients";
//#ifndef NDEBUG
//			std::cout << "Optimize patients: " << dummy << std::endl;
//#endif
//			solvedOpt = slave->optimizePatient(bestSol, solMutex, dummy); // Each thread improves its local solution
//			{
//				std::unique_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//				if(!solvedOpt){
//					destrSizePatient = std::max(0.01, destrSizePatient - 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size patient to: " << destrSizePatient << std::endl;
//#endif
//				} else {
//					destrSizePatient = std::min(1.0, destrSizePatient + 0.01);
//#ifndef NDEBUG
//					std::cout << "Updated destr size patient to: " << destrSizePatient << std::endl;
//#endif
//				}
//			}
//		} else {
//			choiceStr = "OT";
//#ifndef NDEBUG
//			std::cout << "Optimize OT" << std::endl;
//#endif
//			solvedOpt = slave->optimizeOT(bestSol, solMutex);
//		}
//
//		double objValue = slave->model.get(GRB_DoubleAttr_ObjVal); // Get objective value
//		{
//			std::unique_lock<std::shared_mutex> lock(solMutex); // Lock for thread-safe access
//			if (objValue < bestObjValue - EPS) {
//				std::cout << "IP Thread " << id << " with operator " << choiceStr << " found a new best solution: " << objValue << " vs. " << bestObjValue << std::endl;
//				bestObjValue = objValue; // Update the global best objective value
//				slave->saveSolution(bestSol); // Save the new best solution
//#ifndef NDEBUG
//				bestSol.PrintCosts();
//#endif
//			}
//		}
//	} while (std::chrono::steady_clock::now() < endTime);
//}


//Controller::Controller(const int noThreads, const IHTP_Input &in, Solution &out) : noThreads(noThreads){
//
//	// A feasible starting solution is required
//	if(out.getInfValue() > 0){
//		std::cout << "David expected Lisa's constructive sol to be feasible!" << std::endl;
//		std::cout << "Please go complain with Lisa, and leave David alone..." << std::endl;
//		std::abort();
//	}
//
//	// Construct all the workers
//	workers.resize(noThreads, nullptr);
//
//	// Constructing the models is expensive --> parallelize
//	std::cout << "\n\n\nConstructing the workers" << std::endl;
//	for (int i = 0; i < noThreads; ++i) {
//		workers[i] = new Worker(i, in, out);	
//	}
//	std::cout << "Construction of workers done!\n\n\n" << std::endl;
//	
//	// Overall time limit for the optimization process
//	int timeLimitSeconds = 600;
//	auto startTime = std::chrono::steady_clock::now();
//	// Time by when we are out of time
//	auto endTime = startTime + std::chrono::seconds(timeLimitSeconds);
//
//			    
//	// Parallel execution of the optimization algorithm
//	// optimizeThread function is responsible for determining the 
//	// remainder of the algorithm
//	std::vector<std::thread> threads;
//	threads.reserve(noThreads);
//	double bestObjValue = out.getObjValue();
//	double destrSizeNurse = 0.05, destrSizePatient = 0.05, destrSizeSlots = 0.01;
//
//#ifndef NDEBUG 
//	int noThreadsIP = 1;
//#else
//	int noThreadsIP = 2;
//#endif
//
//	for (int i = 0; i < noThreadsIP; ++i) {
//		workers[i]->setNoThreads(1);
//#ifdef NDEBUG 
//		workers[i]->model.set(GRB_IntParam_LogToConsole, 0);
//#endif
//		workers[i]->setTimeLimit(30);
//		// threads.emplace_back(optimizeThread, i, workers[i], std::ref(out), std::ref(bestObjValue), std::ref(solMutex), endTime, std::ref(destrSizeNurse), std::ref(destrSizePatient), std::ref(destrSizeSlots), std::ref(destrMutex));
//		//threads.emplace_back(optimizeThreadRebuild, i, workers[i], std::ref(in), std::ref(out), std::ref(bestObjValue), std::ref(solMutex), endTime, std::ref(destrSizeNurse), std::ref(destrSizePatient), std::ref(destrSizeSlots), std::ref(destrMutex));
//	}
//	for (int i = noThreadsIP; i < noThreads; ++i) {
//		workers[i]->setNoThreads(1);
//		workers[i]->model.set(GRB_IntParam_LogToConsole, 0);
//		workers[i]->setTimeLimit(30);
//		//threads.emplace_back(optimizeThreadSwap, i, workers[i], std::ref(out), std::ref(bestObjValue), std::ref(solMutex), endTime, std::ref(destrMutex), 20);
//	}
//	// Wait for all threads to finish
//	for (auto& thread : threads) {
//		thread.join();
//	}
//
//	// Print final solution found
//	std::cout << "====================================" << std::endl;
//	std::cout << "  Best sol found: " << out.getInfValue() << " - " << out.getObjValue() << std::endl;
//	std::cout << "====================================" << std::endl;
//}

//Controller::~Controller(){
//	for (int i = 0; i < noThreads; ++i) {
//		delete workers[i];
//	}
//}
