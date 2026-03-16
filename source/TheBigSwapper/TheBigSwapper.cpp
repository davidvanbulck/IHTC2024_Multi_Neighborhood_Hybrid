#include "TheBigSwapper.h"
#include "GurSolver.h"
#include "IHTP_Validator.h"
#include "Solution.hpp"
#include <bitset>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <vector>

Swap::Swap(const IHTP_Input &in, Solution &out)
    : sol(in, false), BaseAlgo(in, sol, INT_MAX), unassignPatients(), assignPatients(), unassignNurses(), assignNurses() {

  // Let sol be equal to out; sol is the solution we will work on
  sol = out; // Needed to call UpdatewithOccupantsInfo
}

bool Swap::executeNeighborhood(const int choiceInt,
                               const int perturbationValue) {
    assignPatients.clear();
    unassignPatients.clear();
    assignNurses.clear();
    unassignNurses.clear();

  switch (choiceInt) {
  case 0:
#ifdef CLIQUE
    clique(perturbationValue);
    return true;
#endif
    return false;
  case 1:
    shortestPathPatients(perturbationValue);
    return true;
  case 2:
    swapNursesAssignment(perturbationValue);
    return true;
  case 3:
      return swapPatients();
  case 4:
    return swapOperatingTheaters();
  case 5:
    return changeNurse();
  case 6:
    return kick();
  case 7:
    return movePatient();
  default:
    std::cout << "Operator choice " << choiceInt << " not implemented"
              << std::endl;
    std::abort();
  }
}

bool Swap::movePatient(){
	// Choose a random patient
	int p = std::rand() % in.Patients();

	// Choose a random admission day
	int oldD = sol.AdmissionDay(p);
	int d = oldD;
	if(d == -1 || std::rand() % 100 < 30) {
		std::vector<int> days;
		int s = in.PatientSurgeon(p);
		int release = in.PatientSurgeryReleaseDay(p);
		int last    = in.PatientLastPossibleDay(p);
		for(int day = release; day <= last; ++day){
			if(day != oldD && sol.SurgeonDayLoad(s, day) + in.PatientSurgeryDuration(p) > in.SurgeonMaxSurgeryTime(s, day)) continue;
			days.push_back(day);
		}
		if(days.size() == 0){
			return false;
		}
		d = days[std::rand() % days.size()];
	}
	bool newD = (d != sol.AdmissionDay(p));

	// Choose a random room
	
	const int noRooms = in.Rooms();
	std::vector<int> rooms;
  	Gender gender = in.PatientGender(p);
  	int los = in.PatientLengthOfStay(p);
	int oldR = sol.PatientRoom(p);
	int r = oldR;
	if(r == -1 || newD || std::rand() % 100 < 30) {
		for (int room = 0; room < noRooms; ++room) {
			if(in.IncompatibleRoom(p, room)){
				continue;
			}

			bool feas = true;
		      for (int day = d; day < min(d + los, in.Days()); day++) {
			// Gender
			if (gender ==  Gender::A && sol.RoomDayBPatients(room, day) > 0){
				feas = false;
				break;
			} else if (sol.RoomDayAPatients(room,day) > 0){
				feas = false;
				break;
			}

			// Capacity
			int cap = in.RoomCapacity(room) - sol.RoomDayLoad(room, day);
			if(oldD != -1 && oldR == room && day >= oldD && day < oldD + los){
				cap++;
			}
			if(cap < 1){
				feas = false;
				break;
			}
		      }

		      if(!feas) continue;
			rooms.push_back(room);
		}
		if(rooms.size() == 0) return false;
		r = rooms[std::rand() % rooms.size()];
	}
	bool newR = (r != sol.PatientRoom(p));

	// Choose a random OT
	int oldT = sol.PatientOperatingTheater(p);
	int t = oldT;
	if(oldT == -1 || newD || std::rand() % 100 < 30) {
		std::vector<int> theaters;
		for(int theater = 0; theater < in.OperatingTheaters(); ++theater){
			if((newD || theater!= oldT) && 
				(sol.OperatingTheaterDayLoad(theater, d) + in.PatientSurgeryDuration(p) >
				in.OperatingTheaterAvailability(theater, d))) {
					continue;
				}
			theaters.push_back(theater);
		}
		if(theaters.size() == 0) return false;

		t = theaters[std::rand() % theaters.size()];
	}

	bool newT = (t != sol.PatientOperatingTheater(p));
	bool outcome = moveOnePatient(p,r,d,t,newD,newR,newT);

	if(outcome){
		for (int day = d; day < std::min(in.Days(), d + los); ++day) {
			for(int k = 0; k < in.ShiftsPerDay(); ++k){
				swapNursesAssignment(0, (day)*in.ShiftsPerDay() + k);
			}
			break;
		}
	}

	return outcome;
}

bool Swap::swapPatients() {

  int p = std::rand() % in.Patients();
  vector<int> otherPatients;
  for (int p2 = 0; p2 < in.Patients(); p2++) {
    if (p2 != p) {
      otherPatients.push_back(p2);
    }
  }
  std::shuffle(otherPatients.begin(), otherPatients.end(), rng);

  int i = 0;
  while (i < otherPatients.size() && !swapTwoPatients(p, otherPatients[i])) {
    i++;
  }

  return (i < otherPatients.size());
}

bool Swap::changeNurse() {

  // Replace a nurse on a given shift in a given room by another nurse
  int s = std::rand() % in.Shifts();
  int r = std::rand() % in.Rooms();
  int i = std::rand() % in.AvailableNurses(s);
  int n = in.AvailableNurse(s, i);

  if (n != sol.getRoomShiftNurse(r, s)) {
    int cost = CostEvaluationNurseSwap(n, r, s);
    unassignNurses.push_back({ sol.getRoomShiftNurse(r, s), r, s });
    sol.UnassignNurse(sol.getRoomShiftNurse(r, s), r, s);
    sol.AssignNurse(n, r, s);
    assignNurses.push_back({ n, r, s });
    sol.setObjValue(sol.getObjValue() + cost);
    return true;
  }
  return false;
}

bool Swap::kick() {
  int p1 = std::rand() % in.Patients();
  while ((!sol.ScheduledPatient(p1))) {
    p1 = std::rand() % in.Patients();
  }
  vector<int> otherPatients;
  for (int p2 = 0; p2 < in.Patients(); p2++) {
      if (p2 != p1 && sol.ScheduledPatient(p2) && !in.IncompatibleRoom(p1, sol.PatientRoom(p2)) && in.PatientGender(p1) == in.PatientGender(p2) && 
          sol.AdmissionDay(p2) >= in.PatientSurgeryReleaseDay(p1) && sol.AdmissionDay(p2) <= in.PatientLastPossibleDay(p1)) {
          otherPatients.push_back(p2);
      }
  }
  std::shuffle(otherPatients.begin(), otherPatients.end(), rng);
  int i = 0;
  while (i < otherPatients.size() && !kickOtherPatient(p1, otherPatients.at(i))) {
      i++;
  }
  return (i < otherPatients.size());

  //int p2 = std::rand() % in.Patients();
  //while ((!sol.ScheduledPatient(p2)) || (p2 == p1)) {
  //  p2 = std::rand() % in.Patients();
  //}
  //return this->kickOtherPatient(p1, p2);
  // for (int r = 0; r < in.Rooms(); r++) {
  //   if (in.IncompatibleRoom(p, r)) {
  //     continue;
  //   } else {
  //     this->kickToRoom(p, r);
  //     break;
  //   }
  // }
  //return true;
}

bool Swap::kickOtherPatient(int p1, int p2, int room) {

  if (room == -1) { room = sol.PatientRoom(p2); }

  //! calculate if unassigning p2 would give enough space to p1.
  int first_possible_day = in.PatientSurgeryReleaseDay(p1);
  int last_possible_day = in.PatientLastPossibleDay(p1);
  int p2_addmission_day = sol.AdmissionDay(p2);
  int los1 = in.PatientLengthOfStay(p1);
  int los2 = in.PatientLengthOfStay(p2);

  // keep track of the admission days for rooms
  bitset<28> room_start_day_p1_bitset;
  // keep track of the availability of the ot
  bitset<28> ot_availability_p1_bitset;
  int diff_los = los1 - los2;
  // keep potential start day for p1
  int start_day_p1 = -1;
  //? check if there is room before the admission day of p2
  for (int d = p2_addmission_day - 1; d >= first_possible_day; d--) {
    if (sol.isPatientFeasibleForRoomOnDay(p1, d, room)) {
      diff_los--;
    } else {
      // no need to keep looking.
      break;
    }
    if (sol.isPatientSurgeryPossibleOnDay(p1, d)) {
      ot_availability_p1_bitset.flip(d);
    }
    if (diff_los <= 0) {
      room_start_day_p1_bitset.flip(d);
    }
  }
  //? means we need to check if there is room after p2
  for (int d = p2_addmission_day + los2; d <= last_possible_day; d++) {
    if (sol.isPatientFeasibleForRoomOnDay(p1, d, room)) {
      diff_los--;
    } else {
      break;
    }
    if (d - los1 + 1 < first_possible_day) {
      continue;
    }
    if (sol.isPatientSurgeryPossibleOnDay(p1, d - los1 + 1)) {
      ot_availability_p1_bitset.flip(d - los1 + 1);
    }
    if (diff_los <= 0) {
      room_start_day_p1_bitset.flip(d - los1 + 1);
    }
  }
  if (diff_los > 0) {
    //? kicking p2 would not help p1
    return false;
  } else {
    if (!(ot_availability_p1_bitset & room_start_day_p1_bitset).any()) {
      //? no days possible for p1
      return false;
    }
  }
  //? check if patient 2 can be moved somewhere
  //* SHUFFLE ROOMS AND DAYS
  vector<int> shuffled_days = vector<int>(in.Days(), 0);
  // fills list with index: 0..in.Days()-1
  std::iota(shuffled_days.begin(), shuffled_days.end(), 0);
  std::shuffle(shuffled_days.begin(), shuffled_days.end(), rng);
  vector<int> shuffled_rooms = vector<int>(in.Rooms(), 0);
  std::iota(shuffled_rooms.begin(), shuffled_rooms.end(), 0);
  std::shuffle(shuffled_rooms.begin(), shuffled_rooms.end(), rng);
  vector<int> shuffled_ots = vector<int>(in.OperatingTheaters(), 0);
  std::iota(shuffled_ots.begin(), shuffled_ots.end(), 0);
  std::shuffle(shuffled_ots.begin(), shuffled_ots.end(), rng);
  //* ITERATE OVER SHUFFLED ROOMS AND DAYS
  for (int r : shuffled_rooms) {
      for (int d : shuffled_days) {
          if (d < in.PatientSurgeryReleaseDay(p2) || d > in.PatientLastPossibleDay(p2)) { continue; }
          if (r == room) { continue; }
          if (r == sol.PatientRoom(p1) && !sol.isPatientFeasibleOnDayWithExcludedPatient(p2, d, r, p1)) { continue; }
          if (r != sol.PatientRoom(p1) && !sol.isPatientFeasibleForRoom(p2, d, r)) { continue; }

          // Unassign both patients, but remember their parameters in case we need to undo this.
          vector<int> originalP = {p1, sol.AdmissionDay(p1), sol.PatientRoom(p1), sol.PatientOperatingTheater(p1) };
          vector<int> originalP2 = {p2, sol.AdmissionDay(p2),  sol.PatientRoom(p2), sol.PatientOperatingTheater(p2) };
          int cost = 0;
          cost += sol.CostEvaluationRemovingPatient(false, p1);
          sol.UnassignPatient(p1);
          unassignPatients.push_back(originalP);
          cost += sol.CostEvaluationRemovingPatient(false, p2);
          sol.UnassignPatient(p2);
          unassignPatients.push_back(originalP2);

          // We have established that p2 can be admitted in room r on day d. We now find a day for p.
          bitset<28> possible_days = ot_availability_p1_bitset & room_start_day_p1_bitset;
          for (int day : shuffled_days) {
              if (possible_days.test(day)) {
                  // Find theater
                  int theater = -1;
                  for (int t : shuffled_ots) { if (sol.isPatientFeasibleForOT(p1, t, day)) { theater = t; break; } }
                  // If you want to admit p1 and p2 on the same day, check OT and surgeon overtime is still ok!
                  if (day == d)
                  {
                      // Surgeon Overtime
                      int s1 = in.PatientSurgeon(p1);
                      if (s1 == in.PatientSurgeon(p2) && in.SurgeonMaxSurgeryTime(s1, d) - sol.SurgeonDayLoad(s1, d) < in.PatientSurgeryDuration(p1) + in.PatientSurgeryDuration(p2))
                      {
                          // Infeasible                    
                          continue;
                      }
                      // OT Overtime (and find theater)
                      for (int t : shuffled_ots) {
                          if ((t != theater && sol.isPatientFeasibleForOT(p2, t, day)) ||
                              (t == theater && in.OperatingTheaterAvailability(t, d) - sol.OperatingTheaterDayLoad(t, d) >= in.PatientSurgeryDuration(p1) + in.PatientSurgeryDuration(p2)))
                          {
#ifndef NDEBUG
                              std::cout << "Patient kicked: " << p1 << " to room " << room
                                  << " on day " << d << " and theater " << theater
                                  << " gender "
                                  << (in.PatientGender(p1) == Gender::A ? "A" : "B")
                                  << std::endl;
                              std::cout << "Room: " << room << " Day: " << d
                                  << " gender A: " << sol.RoomDayAPatients(room, d)
                                  << " gender B: " << sol.RoomDayBPatients(room, d)
                                  << std::endl;
                              std::cout << "Patient kicked: " << p2 << " to room " << r
                                  << " on day" << d << " and theater " << t << " gender "
                                  << (in.PatientGender(p1) == Gender::A ? "A" : "B")
                                  << std::endl;
                              std::cout << "Room: " << r << " Day: " << d
                                  << " gender A: " << sol.RoomDayAPatients(r, d)
                                  << " gender B: " << sol.RoomDayBPatients(r, d)
                                  << std::endl;
#endif
                              cost += sol.CostEvaluationAddingPatient(false, p1, room, d, theater);
                              sol.AssignPatient(p1, d, room, theater);
                              assignPatients.push_back(p1);
                              cost += sol.CostEvaluationAddingPatient(false, p2, r, d, t);
                              sol.AssignPatient(p2, d, r, t);
                              assignPatients.push_back(p2);
                              sol.setObjValue(sol.getObjValue() + cost);
                              return true;
                          }
                      }
                  }
                  else
                  {
                      for (int t : shuffled_ots) {
                          if (sol.isPatientFeasibleForOT(p2, t, d))
                          {
#ifndef NDEBUG
                              std::cout << "Patient kicked: " << p1 << " to room " << room
                                  << " on day " << d << " and theater " << theater
                                  << " gender "
                                  << (in.PatientGender(p1) == Gender::A ? "A" : "B")
                                  << std::endl;
                              std::cout << "Room: " << room << " Day: " << d
                                  << " gender A: " << sol.RoomDayAPatients(room, d)
                                  << " gender B: " << sol.RoomDayBPatients(room, d)
                                  << std::endl;
                              std::cout << "Patient kicked: " << p2 << " to room " << r
                                  << " on day" << d << " and theater " << t << " gender "
                                  << (in.PatientGender(p1) == Gender::A ? "A" : "B")
                                  << std::endl;
                              std::cout << "Room: " << r << " Day: " << d
                                  << " gender A: " << sol.RoomDayAPatients(r, d)
                                  << " gender B: " << sol.RoomDayBPatients(r, d)
                                  << std::endl;
#endif
                              cost += sol.CostEvaluationAddingPatient(false, p1, room, day, theater);
                              sol.AssignPatient(p1, day, room, theater);
                              assignPatients.push_back(p1);
                              cost += sol.CostEvaluationAddingPatient(false, p2, r, d, t);
                              sol.AssignPatient(p2, d, r, t);
                              assignPatients.push_back(p2);
                              sol.setObjValue(sol.getObjValue() + cost);
                              return true;
                          }
                      }
                  }
              }
          }

          // If we get to this point, we did not find a feasible kick...
          assignPatients.clear();
          unassignPatients.clear();
          sol.AssignPatient(originalP[0], originalP[1], originalP[2], originalP[3]);
          sol.AssignPatient(originalP2[0], originalP2[1], originalP2[2], originalP2[3]);
          cost = 0;
      }
  }
  return false;
}

void Swap::swapNursesAssignment(int edgeCost, int s) {

  // Take a random shift, and unassign for each nurse (or 95% of the nurses) one
  // room If a nurse works in the same room on some consecutive days, we may
  // unassign her on these days as well
  bool initSProvided = (s!=-1);
  if(!initSProvided){
	  s = std::rand() % in.Shifts();
  }
  int destrSize = in.AvailableNurses(s);
  double rand = static_cast<double>(std::rand()) / RAND_MAX;
  if (rand < 0.1) {
    destrSize *= 0.95;
  } // Some nurses remain assigned
  int costChange = 0;

  // Only clear one room per nurse
  vector<int> roomsToClear;          // Rooms to clear
  vector<vector<int>> shiftsToClear; // For each room, the shifts to clear
  vector<bool> alreadyWorkingNurse(
      in.Nurses(), false); // To keep track which nurse is already cleared
  // Put rooms in random order
  vector<int> rooms;
  for (int r = 0; r < in.Rooms(); r++) {
    rooms.push_back(r);
  }
  std::shuffle(rooms.begin(), rooms.end(), rng);

  int i = 0;
  int roomSpecialEdge = -1;
  int originalNurseSpecialEdge = -1;
  // Clear the rooms
  while (i < rooms.size() && roomsToClear.size() < destrSize) {
    int r = rooms.at(i);
    int n = sol.getRoomShiftNurse(r, s);
    if (n != -1 && !alreadyWorkingNurse[n]) {
      alreadyWorkingNurse[n] = true;
      // Find all consecutive days that the nurse is also working
      assert(sol.getRoomShiftNurse(r, s) == n);
      vector<int> nurseShifts = {s};
      // Earlier days
      int d = (s / in.ShiftsPerDay());
      bool nurseStillWorking = true;
      while (d > 0 && nurseStillWorking) {
        d--;
	if(initSProvided) break;
        bool nurseWorkingDay = false;
        for (int k = 0; k < in.ShiftsPerDay(); k++) {
          if (sol.getRoomShiftNurse(r, d * in.ShiftsPerDay() + k) == n) {
            nurseWorkingDay = true;
            assert(in.IsNurseWorkingInShift(n, d * in.ShiftsPerDay() + k));
            assert(sol.getRoomShiftNurse(r, d * in.ShiftsPerDay() + k) == n);
            nurseShifts.push_back(d * in.ShiftsPerDay() + k);
            break;
          }
        }
        nurseStillWorking = nurseWorkingDay;
      }
      int firstDay = d + 1;
      std::reverse(nurseShifts.begin(), nurseShifts.end());
      int positionS = nurseShifts.size() - 1; // Position of the current shift
      // Later days
      d = (s / in.ShiftsPerDay());
      nurseStillWorking = true;
      while (d < in.Days() - 1 && nurseStillWorking) {
        d++;
	if(initSProvided) break;
        bool nurseWorkingDay = false;
        for (int k = 0; k < in.ShiftsPerDay(); k++) {
          if (sol.getRoomShiftNurse(r, d * in.ShiftsPerDay() + k) == n) {
            nurseWorkingDay = true;
            assert(in.IsNurseWorkingInShift(n, d * in.ShiftsPerDay() + k));
            assert(sol.getRoomShiftNurse(r, d * in.ShiftsPerDay() + k) == n);
            nurseShifts.push_back(d * in.ShiftsPerDay() + k);
            break;
          }
        }
        nurseStillWorking = nurseWorkingDay;
      }
      int lastDay = d - 1;
      int range = lastDay - firstDay + 1;
      std::vector<double> weights(range);
      for (int k = 0; k < range; ++k) {
        weights[k] = (1.0 / (k + 1.0));
      }
      std::discrete_distribution<int> dist(weights.begin(), weights.end());
      std::random_device rd;
      std::mt19937 gen(rd());
      int lengthPeriod = dist(gen) + 1; // Amount of shifts to clear
      int beginPoint = max(0, positionS - lengthPeriod + 1);
      int endPoint = min(positionS, (int)nurseShifts.size() - lengthPeriod + 1);
      int definitiveBeginPoint; // Index of first shift to clear
      if (endPoint != beginPoint) {
        definitiveBeginPoint =
            (std::rand() % (endPoint - beginPoint)) + beginPoint;
      } else {
        definitiveBeginPoint = positionS;
      }
      // Update shifts to clear
      vector<int> definitiveShiftsToClear;
      for (int k = definitiveBeginPoint;
           k < definitiveBeginPoint + lengthPeriod; k++) {
        assert(in.IsNurseWorkingInShift(n, nurseShifts[k]));
        assert(sol.getRoomShiftNurse(r, nurseShifts[k]) == n);
        costChange += sol.CostEvaluationRemovingNurse(false, n, r, nurseShifts[k]);
#ifndef NDEBUG
        std::cout << "Unassigned nurse " << n << " from room " << r << " on shift " << nurseShifts[k] << " with cost " << sol.CostEvaluationRemovingNurse(false, n, r, nurseShifts[k]) << std::endl;
#endif
        unassignNurses.push_back({ n, r, nurseShifts[k] });
        sol.UnassignNurse(n, r, nurseShifts[k]);
        definitiveShiftsToClear.push_back(nurseShifts[k]);

      }
      if (lengthPeriod == 1 && roomSpecialEdge == -1) {
        roomSpecialEdge = r;
        originalNurseSpecialEdge = n;
      }
      roomsToClear.push_back(r);
      shiftsToClear.push_back(definitiveShiftsToClear);
    }
    i++;
  }
  // Calculate cost matrix and solve assignment problem
  constructCostMatrix(s, roomsToClear, shiftsToClear, roomSpecialEdge,
                      originalNurseSpecialEdge, edgeCost);
  std::vector<long> assignment = dlib::max_cost_assignment(this->costMatrix);

  // Update assignment and costs
  for (int i = 0; i < in.AvailableNurses(s); i++) {
    int j = assignment[i];
    if (j < roomsToClear.size()) {
      int n = in.AvailableNurse(s, i);
      int r = roomsToClear[j];
      for (int k : shiftsToClear[j]) {
        assert(in.IsNurseWorkingInShift(n, k));
        sol.AssignNurse(n, r, k);
        assignNurses.push_back({ n, r, k });
      }
#ifndef NDEBUG
      std::cout << "Assigned nurse " << n << " to room " << r << " with cost " << -this->costMatrix(i, j) << std::endl;
#endif
      costChange -= this->costMatrix(i, j);
      if (r == roomSpecialEdge && n == originalNurseSpecialEdge) {
        costChange -= edgeCost;
      }
    }
  }

  // Debug check: is the solution not worse then perturbationvalue?
  assert(costChange <= edgeCost);

  sol.setObjValue(sol.getObjValue() + costChange);

#ifndef NDEBUG
  int check = sol.getObjValue();
  std::cout.setstate(std::ios::failbit);
  sol.PrintCosts();
  std::cout.clear(); 
  std::cout << "REAL COST: " << sol.getObjValue() << std::endl;
  assert(check == sol.getObjValue());
#endif
}

void Swap::constructCostMatrix(int s, vector<int> &roomsToClear,
                               vector<vector<int>> &shiftsToClear,
                               int roomSpecialEdge, int nurseSpecialEdge,
                               int costSpecialEdge) {
  int dimension = in.AvailableNurses(s);
  assert(roomsToClear.size() == shiftsToClear.size());
  dlib::matrix<int> matrix(dimension, dimension);
  this->costMatrix = matrix;

  // Set costs
  for (int i = 0; i < dimension; i++) {
    int n = in.AvailableNurse(s, i);
    for (int j = 0; j < roomsToClear.size(); j++) {
      int r = roomsToClear[j];
      int cost = 0;
      vector<bool> patientsSeen(in.Patients() + in.Occupants(), false); // For calculation COC
      int costContinuityOfCare = 0;
#ifndef NDEBUG
      int checkCostCOC = 0;
      vector<bool> patientsSeenCheck(in.Patients() + in.Occupants(), false); // For calculation COC
#endif
      for (int k : shiftsToClear[j]) {
        if (in.IsNurseWorkingInShift(n, k)) {
          cost -= sol.CostAddingNurseNoCOC(false, n, r, k);
          // REAL COC for this nurse
          int d = k / in.ShiftsPerDay();
          for (int p : sol.getPatientsRoomDay(r, d)) {
              bool addedMe = false;
              bool addedYou = false;

              if (!patientsSeen[p]) {
                  vector<int> nurses = sol.getPatientShiftNurses(p);
                  int limit = nurses.size();
                  if (p < in.Patients()) { limit = min((int)nurses.size(), (in.Days() - sol.AdmissionDay(p)) * in.ShiftsPerDay()); }
                  if (std::count(nurses.begin(), nurses.begin() + limit, n) == 0)
                  {
                      costContinuityOfCare++;
                      addedMe = true;
                  }
#ifndef NDEBUG
                  if (p < in.Patients()) {
                      int s1 = s - sol.AdmissionDay(p) * in.ShiftsPerDay();
                      int s2 = 0;
                      bool current_nurse_already = false;
                      while (s2 < min((int)sol.getPatientShiftNurses(p).size(),
                          (in.Days() - sol.AdmissionDay(p)) *
                          in.ShiftsPerDay()) &&
                          !current_nurse_already) {
                          if (s2 != s1) {
                              int n2 = sol.getPatientShiftNurses(p)[s2];
                              if (n2 == n) {
                                  current_nurse_already = true;
                              }
                          }
                          s2++;
                      }
                      if (!current_nurse_already) {
                          checkCostCOC++;
                          addedYou = true;
                      }
                  }
                  else {
                      int s2 = 0;
                      bool current_nurse_already = false;
                      while (s2 < in.OccupantLengthOfStay(p - in.Patients()) *
                          in.ShiftsPerDay() &&
                          !current_nurse_already) {
                          if (s2 != s) {
                              int n2 = sol.getPatientShiftNurses(p)[s2];
                              if (n2 == n) {
                                  current_nurse_already = true;
                              }
                          }
                          s2++;
                      }
                      if (!current_nurse_already) {
                          checkCostCOC++;
                          addedYou = true;
                      }
                  }
#endif
                  assert(addedMe == addedYou);
                  patientsSeen[p] = true;
              }
          }
        } 
        else {
          cost -= 1000 * in.Weight(7);
          break;
        }
      }
      assert(costContinuityOfCare == checkCostCOC);
      cost -= costContinuityOfCare * in.Weight(2); 
      if (r == roomSpecialEdge && n == nurseSpecialEdge) {
        cost -= costSpecialEdge;
      }
      this->costMatrix(i, j) = cost; // Negative because maximization problem
    }
    for (int j = roomsToClear.size(); j < dimension; j++) {
      this->costMatrix(i, j) = 0;
    }
  }
}


bool Swap::swapOperatingTheaters() {
    int d = std::rand() % in.Days();

    vector<int> openOTs;     // Open operating theaters on this day
    vector<int> patients;    // Patients scheduled on this day
    vector<int> originalOTs; // Operating theater that each patient was
                             // originally assigned to
    int originalCost = sol.getObjValue();
    for (int t = 0; t < in.OperatingTheaters(); t++) {
      if (sol.OperatingTheaterDayLoad(t, d) > 0) {
        openOTs.push_back(t);
      }
    }
    if (openOTs.size() == 0) { return false; }

    // Remove all patients from operating theatres and start from scratch
    for (int p = 0; p < in.Patients(); p++) {
      if (sol.AdmissionDay(p) == d) {
        originalOTs.push_back(sol.PatientOperatingTheater(p));
        pair<int, int> costOT = sol.costEvaluationRemoveOT(p);
        sol.setObjValue(sol.getObjValue() + costOT.first * in.Weight(4) +
                        costOT.second * in.Weight(5));
        unassignPatients.push_back({ p, sol.AdmissionDay(p), sol.PatientRoom(p), sol.PatientOperatingTheater(p) });
        sol.UnassignPatientOperatingTheater(p);
        patients.push_back(p);
      }
    }
    vector<int> sortedPatients = sol.sortedPatientsSurgeon(
        patients); // Sort patients based on whether they have the same surgeon
    vector<bool> patientAssigned(
        sortedPatients.size()); // Remembers which patients are already assigned
                                // to a new OT

    // Assign first patient to first open OT
    pair<int, int> costOT =
        sol.costEvaluationAssignOT(sortedPatients[0], d, openOTs[0]);
    sol.setObjValue(sol.getObjValue() + costOT.first * in.Weight(4) +
                    costOT.second * in.Weight(5));
    sol.AssignPatientOperatingTheater(sortedPatients[0], openOTs[0]);
    assignPatients.push_back(sortedPatients[0]);
    patientAssigned[0] = true;

    // First assign all surgeons to their own OT, as far as possible
    int j = 0; // OT index
    int i = 1; // Patient index
    while (j < openOTs.size() && i < sortedPatients.size()) {
      int p = sortedPatients[i];
      if (in.PatientSurgeon(p) != in.PatientSurgeon(sortedPatients[i - 1]) &&
          j < openOTs.size() - 1) {
        j++;
      }
      int t = openOTs[j];
      if (in.OperatingTheaterAvailability(t, d) -
              sol.OperatingTheaterDayLoad(t, d) >=
          in.PatientSurgeryDuration(p)) {
        pair<int, int> costOT = sol.costEvaluationAssignOT(p, d, t);
        sol.setObjValue(sol.getObjValue() + costOT.first * in.Weight(4) +
                        costOT.second * in.Weight(5));
        sol.AssignPatientOperatingTheater(p, t);
        assignPatients.push_back(p);
        patientAssigned[i] = true;
        i++;
      } else {
        j++;
      }
    }
    // Then fill gaps with remaining patients
    while (i < sortedPatients.size()) {
      j = 0;
      while (j < openOTs.size() && i < sortedPatients.size()) {
        int p = sortedPatients[i];
        int t = openOTs[j];
        if (in.OperatingTheaterAvailability(t, d) -
                sol.OperatingTheaterDayLoad(t, d) >=
            in.PatientSurgeryDuration(p)) {
          pair<int, int> costOT = sol.costEvaluationAssignOT(p, d, t);
          sol.setObjValue(sol.getObjValue() + costOT.first * in.Weight(4) +
                          costOT.second * in.Weight(5));
          assert(sol.OperatingTheaterDayLoad(t, d) +
                     in.PatientSurgeryDuration(p) <=
                 in.OperatingTheaterAvailability(t, d));
          sol.AssignPatientOperatingTheater(p, t);
          assignPatients.push_back(p);
          patientAssigned[i] = true;
          i++;
        } else {
          j++;
        }
      }
      if (j == openOTs.size()) {
        // Patient could not be assigned; go back to original schedule
        i = sortedPatients.size();
      }
    }

    // If not all patients could be assigned, return to original schedule
    if (std::count(patientAssigned.begin(), patientAssigned.end(), true) <
        patientAssigned.size()) {
      sol.setObjValue(originalCost);
      assignPatients.clear();
      unassignPatients.clear();
      for (i = 0; i < patients.size(); i++) {
        int p = patients[i];
        if (sol.PatientOperatingTheater(p) == -1) {
          sol.AssignPatientOperatingTheater(p, originalOTs[i]);
        } else {
          sol.UnassignPatientOperatingTheater(p);
          sol.AssignPatientOperatingTheater(p, originalOTs[i]);
        }
      }
      return false;
    }
    return true;
}


bool Swap::isPatientMoveFeasible(int p, int r, int d, int t, bool newD, bool newR, bool newT) {
  Gender gender = in.PatientGender(p);

  // Room Compatability
  if (newR && in.IncompatibleRoom(p, r)){
	  std::cout << "Room" << std::endl;
    return false;
  }

  // Admission Day
  if (newD && (d < in.PatientSurgeryReleaseDay(p) ||
                   d > in.PatientLastPossibleDay(p))) {
	  std::cout << "Adm" << std::endl;
    return false;
  }

  // Surgeon Overtime
  int s = in.PatientSurgeon(p);
  if (newD &&
      sol.SurgeonDayLoad(s, d) + in.PatientSurgeryDuration(p) >
          in.SurgeonMaxSurgeryTime(s, d)) {
	  std::cout << "Surgeon" << std::endl;
    return false;
  }

  // OT Overtime
  if ((newD || newT) &&
    (sol.OperatingTheaterDayLoad(t, d) + in.PatientSurgeryDuration(p) >
            in.OperatingTheaterAvailability(t, d))) {
	  std::cout << "OT" << std::endl;
	    return false;
  }

  // Gender Mix and Room capacity
  int los = in.PatientLengthOfStay(p);
  int oldD = sol.AdmissionDay(p);
  if(newD || newR){
      for (int day = d; day < min(d + los, in.Days()); day++) {
	// Gender
        if (gender ==  Gender::A && sol.RoomDayBPatients(r, day) > 0){
		std::cout << "Gender" << std::endl;
		return false;
	} else if (sol.RoomDayAPatients(r,day) > 0){
		std::cout << "Gender" << std::endl;
		return false;
	}

	// Capacity
	int cap = in.RoomCapacity(r) - sol.RoomDayLoad(r, day);
	if(oldD != -1 && !newR && day >= oldD && day < oldD + los){
		cap++;
	}
	if(cap < 1){
		std::cout << "Cap" << std::endl;
		return false;
	}
      }
  }

  return true;
}


bool Swap::moveOnePatient(int p, int r, int d, int t, bool newD, bool newR, bool newT){

#ifndef NDEBUG
  if (!isPatientMoveFeasible(p, r, d, t, newD, newR, newT)) { 
	  std::cout << "Patient move always expected to be feasible!" << std::endl;
	std::abort();
  }
#endif

  int oldD = sol.AdmissionDay(p);
  int costChange = 0;
  if (oldD != -1) {
    costChange += sol.CostEvaluationRemovingPatient(false, p);
    unassignPatients.push_back({ p, oldD, sol.PatientRoom(p), sol.PatientOperatingTheater(p) });
    sol.UnassignPatient(p);
  }

  costChange += sol.CostEvaluationAddingPatient(false, p, r, d, t);
  sol.CostEvaluationAddingPatient(false, p, r, d, t);
  sol.AssignPatient(p, d, r, t);
  assignPatients.push_back(p);


  sol.setObjValue(sol.getObjValue() + costChange);


#ifndef NDEBUG
  int check = sol.getObjValue();
  std::cout.setstate(std::ios::failbit);
  sol.PrintCosts();
  std::cout.clear(); 
  if(check != sol.getObjValue() || sol.getInfValue() != 0){
	  std::cout << "Move " << p << " - " << r << " - " << d << " - " << t << std::endl;
	  std::cout << "Cur add: " << oldD << std::endl;
	  std::cout << "REAL COST: " << sol.getObjValue() << std::endl;
	  std::cout << "CHECK: " << check << std::endl;
  	  sol.PrintCosts();
	  std::cout << "ERROR DUE TO MOVE" << std::endl;
  }
  assert(sol.getInfValue() == 0);
  assert(check == sol.getObjValue());
#endif

  return true;
}

bool Swap::swapTwoPatients(int p, int p2) {

  if (!isPatientSwapFeasible(p, p2)) { return false; }

  int d = sol.AdmissionDay(p);
  int d2 = sol.AdmissionDay(p2);
  int r = sol.PatientRoom(p);
  int r2 = sol.PatientRoom(p2);
  int ot = sol.PatientOperatingTheater(p);
  int ot2 = sol.PatientOperatingTheater(p2);

  int newOt = ot;
  int newOt2 = ot2;

  int costChange = 0;

  if (d != -1) {
    costChange += sol.CostEvaluationRemovingPatient(false, p);
    sol.UnassignPatient(p);
    unassignPatients.push_back({ p, d, r, ot });
  }
  if (d2 != -1) {
    costChange += sol.CostEvaluationRemovingPatient(false, p2);
    sol.UnassignPatient(p2);
    unassignPatients.push_back({ p2, d2, r2, ot2 });
  }

  // SPECIAL CASE: patients are in same room and scheduled consecutively
  int los = in.PatientLengthOfStay(p);
  int los2 = in.PatientLengthOfStay(p2);
  if (r == r2 && (d == d2 + los2 || d2 == d + los)) {
    if (d == d2 + los2) // Schedule p at d2 and p2 at d2 + los
    {
      newOt = sol.determineBestOTAndCosts(p, d2)[0];
      newOt2 = sol.determineBestOTAndCosts(p2, d2 + los)[0];
      costChange += sol.CostEvaluationAddingPatient(false, p, r2, d2, newOt);
      sol.AssignPatient(p, d2, r2, newOt);
      assignPatients.push_back(p);
      costChange +=
          sol.CostEvaluationAddingPatient(false, p2, r, d2 + los, newOt2);
      sol.AssignPatient(p2, d2 + los, r, newOt2);
      assignPatients.push_back(p2);
    } else // Schedule p2 at d and p at d + los2
    {
      newOt = sol.determineBestOTAndCosts(p, d + los2)[0];
      newOt2 = sol.determineBestOTAndCosts(p2, d)[0];
      costChange +=
          sol.CostEvaluationAddingPatient(false, p, r2, d + los2, newOt);
      sol.AssignPatient(p, d + los2, r2, newOt);
      assignPatients.push_back(p);
      costChange += sol.CostEvaluationAddingPatient(false, p2, r, d, newOt2);
      sol.AssignPatient(p2, d, r, newOt2);
      assignPatients.push_back(p2);
    }
  }
  // Other cases: regular swap
  else {
    if (d != -1 && d != d2) {
      newOt2 = sol.determineBestOTAndCosts(p2, d)[0];
    }
    if (d2 != -1 && d != d2) {
      newOt = sol.determineBestOTAndCosts(p, d2)[0];
    }

    if (d != -1) {
      costChange += sol.CostEvaluationAddingPatient(false, p2, r, d, newOt2);
      sol.AssignPatient(p2, d, r, newOt2);
      assignPatients.push_back(p2);
    }
    if (d2 != -1) {
      costChange += sol.CostEvaluationAddingPatient(false, p, r2, d2, newOt);
      sol.AssignPatient(p, d2, r2, newOt);
      assignPatients.push_back(p);
    }
  }
  sol.setObjValue(sol.getObjValue() + costChange);
  return true;
}

bool Swap::isPatientSwapFeasible(int p, int p2) {
  int d = sol.AdmissionDay(p);
  int d2 = sol.AdmissionDay(p2);
  int r = sol.PatientRoom(p);
  int r2 = sol.PatientRoom(p2);
  int los = in.PatientLengthOfStay(p);
  int los2 = in.PatientLengthOfStay(p2);
  int s = in.PatientSurgeon(p);
  int s2 = in.PatientSurgeon(p2);
  Gender gender = in.PatientGender(p);
  Gender gender2 = in.PatientGender(p2);

  // At least one patient assigned
  if (d == -1 && d2 == -1) {
    return false;
  }

  // SPECIAL CASE: patients are in same room and scheduled consecutively
  if (r == r2 && (d == d2 + los2 || d2 == d + los)) {
    if (d == d2 + los2) // Schedule p at d2 and p2 at d2 + los
    {
      if (d2 + los >= in.Days()) {
        return false;
      }
      // Admission Day
      if (d2 < in.PatientSurgeryReleaseDay(p) ||
          d2 + los > in.PatientLastPossibleDay(p2)) {
        return false;
      }
      // Surgeon Overtime
      if (sol.SurgeonDayLoad(s, d2) + in.PatientSurgeryDuration(p) >
          in.SurgeonMaxSurgeryTime(s, d2)) {
        return false;
      }
      if (sol.SurgeonDayLoad(s2, d2 + los) + in.PatientSurgeryDuration(p2) >
          in.SurgeonMaxSurgeryTime(s2, d2 + los)) {
        return false;
      }
      // OT Overtime
      int t = 0;
      bool ot = false;
      bool ot2 = false;
      while (t < in.OperatingTheaters() && (!ot || !ot2)) {
        if (sol.OperatingTheaterDayLoad(t, d2) + in.PatientSurgeryDuration(p) <=
            in.OperatingTheaterAvailability(t, d2)) {
          ot = true;
        }
        if (sol.OperatingTheaterDayLoad(t, d2 + los) +
                in.PatientSurgeryDuration(p2) <=
            in.OperatingTheaterAvailability(t, d2 + los)) {
          ot2 = true;
        }
        t++;
      }
      if (!ot || !ot2) {
        return false;
      }
      return (in.PatientGender(p) == in.PatientGender(p2));
    }
    if (d2 == d + los) // Schedule p2 at d and p at d + los2
    {
      if (d + los2 >= in.Days()) {
        return false;
      }
      // Admission Day
      if (d + los2 > in.PatientLastPossibleDay(p) ||
          d < in.PatientSurgeryReleaseDay(p2)) {
        return false;
      }
      // Surgeon Overtime
      if (sol.SurgeonDayLoad(s, d + los2) + in.PatientSurgeryDuration(p) >
          in.SurgeonMaxSurgeryTime(s, d + los2)) {
        return false;
      }
      if (sol.SurgeonDayLoad(s2, d) + in.PatientSurgeryDuration(p2) >
          in.SurgeonMaxSurgeryTime(s2, d)) {
        return false;
      }
      // OT Overtime
      int t = 0;
      bool ot = false;
      bool ot2 = false;
      while (t < in.OperatingTheaters() && (!ot || !ot2)) {
        if (sol.OperatingTheaterDayLoad(t, d + los2) +
                in.PatientSurgeryDuration(p) <=
            in.OperatingTheaterAvailability(t, d + los2)) {
          ot = true;
        }
        if (sol.OperatingTheaterDayLoad(t, d) + in.PatientSurgeryDuration(p2) <=
            in.OperatingTheaterAvailability(t, d)) {
          ot2 = true;
        }
        t++;
      }
      if (!ot || !ot2) {
        return false;
      }
      return (gender == gender2);
    }
  }

  // Room Compatability
  if (r2 != -1 && in.IncompatibleRoom(p, r2)) {
    return false;
  }
  if (r != -1 && in.IncompatibleRoom(p2, r)) {
    return false;
  }

  // Admission Day
  if (d2 != -1 && (d2 < in.PatientSurgeryReleaseDay(p) ||
                   d2 > in.PatientLastPossibleDay(p))) {
    return false;
  }
  if (d != -1 && (d < in.PatientSurgeryReleaseDay(p2) ||
                  d > in.PatientLastPossibleDay(p2))) {
    return false;
  }

  //  Mandatory Unscheduled Patients
  if (d2 == -1 && in.PatientMandatory(p)) {
    return false;
  }
  if (d == -1 && in.PatientMandatory(p2)) {
    return false;
  }

  // Surgeon Overtime
  if (d2 != -1 && d != d2 &&
      sol.SurgeonDayLoad(s, d2) + in.PatientSurgeryDuration(p) >
          in.SurgeonMaxSurgeryTime(s, d2)) {
    return false;
  }
  if (d != -1 && d != d2 &&
      sol.SurgeonDayLoad(s2, d) + in.PatientSurgeryDuration(p2) >
          in.SurgeonMaxSurgeryTime(s2, d)) {
    return false;
  }

  // OT Overtime
  int ot = sol.PatientOperatingTheater(p);
  int ot2 = sol.PatientOperatingTheater(p2);

  if (d2 != -1 && d != d2) {
    int t = 0;
    while (t < in.OperatingTheaters()) {
      if (t != ot2 &&
          sol.OperatingTheaterDayLoad(t, d2) + in.PatientSurgeryDuration(p) <=
              in.OperatingTheaterAvailability(t, d2)) {
        break;
      }
      if (t == ot2 && sol.OperatingTheaterDayLoad(t, d2) +
                              in.PatientSurgeryDuration(p) -
                              in.PatientSurgeryDuration(p2) <=
                          in.OperatingTheaterAvailability(t, d2)) {
        break;
      }
      t++;
    }
    if (t == in.OperatingTheaters()) {
      return false;
    }
  }
  if (d != -1 && d != d2) {
    int t = 0;
    while (t < in.OperatingTheaters()) {
      if (t != ot &&
          sol.OperatingTheaterDayLoad(t, d) + in.PatientSurgeryDuration(p2) <=
              in.OperatingTheaterAvailability(t, d)) {
        break;
      }
      if (t == ot && sol.OperatingTheaterDayLoad(t, d) +
                             in.PatientSurgeryDuration(p2) -
                             in.PatientSurgeryDuration(p) <=
                         in.OperatingTheaterAvailability(t, d)) {
        break;
      }
      t++;
    }
    if (t == in.OperatingTheaters()) {
      return false;
    }
  }

  // Gender Mix overlap part
  if (gender != gender2) {
    if (d != -1) {
      for (int day = d; day < min(d + los, in.Days()); day++) {
        if (sol.RoomDayLoad(r, day) > 1) {
          return false;
        }
      }
    }
    if (d2 != -1) {
      for (int day = d2; day < min(d2 + los2, in.Days()); day++) {
        if (sol.RoomDayLoad(r2, day) > 1) {
          return false;
        }
      }
    }
  }

  // Gender Mix non overlap part + Room Capacity
  if (los < los2 && d != -1 && d + los < in.Days()) {
    for (int day = d + los; day < min(d + los2, in.Days()); day++) {
      if (in.RoomCapacity(r) == sol.RoomDayLoad(r, day)) {
        return false;
      }
      if (gender2 == Gender::A && sol.RoomDayBPatients(r, day) > 0) {
        return false;
      } else if (gender2 == Gender::B && sol.RoomDayAPatients(r, day) > 0) {
        return false;
      }
    }
  } else if (los2 < los && d2 != -1 && d2 + los2 < in.Days()) {
    for (int day = d2 + los2; day < min(d2 + los, in.Days()); day++) {
      if (in.RoomCapacity(r2) == sol.RoomDayLoad(r2, day)) {
        return false;
      }
      if (gender == Gender::A && sol.RoomDayBPatients(r2, day) > 0) {
        return false;
      } else if (gender == Gender::B && sol.RoomDayAPatients(r2, day) > 0) {
        return false;
      }
    }
  }

  return true;
}

int Swap::CostEvaluationNurseSwap(int n, int r, int s) {
  int costExcessiveWorkload = 0;
  int costRoomSkillLevel = 0;
  int costContinuityOfCare = 0;
  int current_nurse = sol.getRoomShiftNurse(r, s);

  if (current_nurse == -1 || current_nurse == n) {
    return 0;
  }

  if (!in.IsNurseWorkingInShift(n, s)) {
    std::cout << "Error! You want to assign a nurse to an invalid shift."
              << std::endl;
    return 100;
  }

  int d = s / in.ShiftsPerDay();
  int load = 0;
  for (int j = 0; j < sol.getPatientsRoomDay(r, d).size(); j++) {
    int p = sol.getPatientsRoomDay(r, d)[j];
    if (p < in.Patients()) {
      int s1 = s - sol.AdmissionDay(p) * in.ShiftsPerDay();

      // ExcessiveNurseWorkload
      load += in.PatientWorkloadProduced(p, s1);

      // RoomSkillLevel
      if (in.PatientSkillLevelRequired(p, s1) >
          in.NurseSkillLevel(current_nurse)) {
        costRoomSkillLevel -= (in.PatientSkillLevelRequired(p, s1) -
                               in.NurseSkillLevel(current_nurse));
      }
      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n);
      }

      // ContinuityOfCare
      int s2 = 0;
      bool current_nurse_already = false;
      bool new_nurse_already = false;

      while (s2 < min((int)sol.getPatientShiftNurses(p).size(),
                      (in.Days() - sol.AdmissionDay(p)) * in.ShiftsPerDay()) &&
             !(current_nurse_already && new_nurse_already)) {
        if (s2 != s1) {
          int n2 = sol.getPatientShiftNurses(p)[s2];
          if (n2 == current_nurse) {
            current_nurse_already = true;
          }
          if (n2 == n) {
            new_nurse_already = true;
          }
        }
        s2++;
      }
      if (!current_nurse_already)
        costContinuityOfCare--;
      if (!new_nurse_already)
        costContinuityOfCare++;
    } else {
      load += in.OccupantWorkloadProduced(p - in.Patients(), s);
      if (in.OccupantSkillLevelRequired(p - in.Patients(), s) >
          in.NurseSkillLevel(current_nurse)) {
        costRoomSkillLevel -=
            (in.OccupantSkillLevelRequired(p - in.Patients(), s) -
             in.NurseSkillLevel(current_nurse));
      }
      if (in.OccupantSkillLevelRequired(p - in.Patients(), s) >
          in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            in.OccupantSkillLevelRequired(p - in.Patients(), s) -
            in.NurseSkillLevel(n);
      }

      // ContinuityOfCare
      int s2 = 0;
      bool current_nurse_already = false;
      bool new_nurse_already = false;
      while (s2 < in.OccupantLengthOfStay(p - in.Patients()) *
                      in.ShiftsPerDay() &&
             !(current_nurse_already && new_nurse_already)) {
        if (s2 != s) {
          int n2 = sol.getPatientShiftNurses(p)[s2];
          if (n2 == current_nurse) {
            current_nurse_already = true;
          }
          if (n2 == n) {
            new_nurse_already = true;
          }
        }
        s2++;
      }
      if (!current_nurse_already)
        costContinuityOfCare--;
      if (!new_nurse_already)
        costContinuityOfCare++;
    }
  }
  if (sol.NurseShiftLoad(current_nurse, s) >
      in.NurseMaxLoad(current_nurse, s)) {
    if (sol.NurseShiftLoad(current_nurse, s) - load >
        in.NurseMaxLoad(current_nurse, s)) {
      costExcessiveWorkload -= load;
    } else {
      costExcessiveWorkload -= (sol.NurseShiftLoad(current_nurse, s) -
                                in.NurseMaxLoad(current_nurse, s));
    }
  }
  if (sol.NurseShiftLoad(n, s) + load > in.NurseMaxLoad(n, s)) {
    if (sol.NurseShiftLoad(n, s) >= in.NurseMaxLoad(n, s)) {
      costExcessiveWorkload += load;
    } else
      costExcessiveWorkload +=
          (load - (in.NurseMaxLoad(n, s) - sol.NurseShiftLoad(n, s)));
  }

  // if (costRoomSkillLevel * in.Weight(1) + costContinuityOfCare * in.Weight(2)
  // + costExcessiveWorkload * in.Weight(3) < 0)
  //{
  //     std::cout << "cost Room Skill Level: " << costRoomSkillLevel *
  //     in.Weight(1) << std::endl; std::cout << "cost Continuity Of Care: " <<
  //     costContinuityOfCare * in.Weight(2) << std::endl; std::cout << "cost
  //     Excessive Nurse Workload: " << costExcessiveWorkload * in.Weight(3) <<
  //     std::endl;
  // }
  return costRoomSkillLevel * in.Weight(1) +
         costContinuityOfCare * in.Weight(2) +
         costExcessiveWorkload * in.Weight(3);
}

int Swap::CostEvaluationPatientSwap(int p, int p2) {

  // --> this function assumes both patients have the same LOS!
  // Does not yet take operating theatres into account
  // We do not include the cost for Elective Unscheduled Patients, since a
  // positive cost means we have replaced an elective patient with a mandatory
  // patient, and this should not be punished! Continuity of Care stays the same
  // since we assume patients have the same LOS
  int costRoomAgeMix = 0;
  int costPatientDelay = 0;
  int costRoomSkillLevel = 0;
  int costExcessiveNurseWorkload = 0;

  // Values
  int d = sol.AdmissionDay(p);
  int d2 = sol.AdmissionDay(p2);
  int r = sol.PatientRoom(p);
  int r2 = sol.PatientRoom(p2);
  int los = in.PatientLengthOfStay(p);

  if (d == -1 && d2 == -1) {
    return 0;
  }

  // Room Age Mix
  if (in.PatientAgeGroup(p) != in.PatientAgeGroup(p2)) {
    if (d != d2 || r != r2) {
      // --> This assumes both patients have the same LOS!
      // Moving p2 to r
      if (d != -1) {
        for (int d3 = d; d3 < min(in.Days(), d + los); d3++) {
          int min = 100;
          int max = -1;
          int age;

          for (int i = 0; i < sol.getPatientsRoomDay(r, d3).size(); i++) {
            int p3 = sol.getPatientsRoomDay(r, d3)[i];
            if (p == p3)
              continue;
            if (p3 < in.Patients()) {
              age = in.PatientAgeGroup(p3);
            } else {
              age = in.OccupantAgeGroup(p3 - in.Patients());
            }
            if (age < min) {
              min = age;
            }
            if (age > max) {
              max = age;
            }
          }
          if (max > -1) // Otherwise p was the only patient, cost 0
          {
            if (in.PatientAgeGroup(p) < min) {
              costRoomAgeMix -= (min - in.PatientAgeGroup(p));
            } else if (in.PatientAgeGroup(p) > max) {
              costRoomAgeMix -= (in.PatientAgeGroup(p) - max);
            }
            if (in.PatientAgeGroup(p2) < min) {
              costRoomAgeMix += (min - in.PatientAgeGroup(p2));
            } else if (in.PatientAgeGroup(p2) > max) {
              costRoomAgeMix += (in.PatientAgeGroup(p2) - max);
            }
          }
        }
      }
      // Moving p to r2
      if (d2 != -1) {
        for (int d3 = d2; d3 < min(in.Days(), d2 + los); d3++) {
          int min = in.AgeGroups() + 1;
          int max = -1;
          int age;

          for (int i = 0; i < sol.getPatientsRoomDay(r2, d3).size(); i++) {
            int p3 = sol.getPatientsRoomDay(r2, d3)[i];
            if (p2 == p3)
              continue;
            if (p3 < in.Patients()) {
              age = in.PatientAgeGroup(p3);
            } else {
              age = in.OccupantAgeGroup(p3 - in.Patients());
            }
            if (age < min) {
              min = age;
            }
            if (age > max) {
              max = age;
            }
          }
          if (max > -1) // Otherwise p2 was the only patient, cost 0
          {
            if (in.PatientAgeGroup(p2) < min) {
              costRoomAgeMix -= (min - in.PatientAgeGroup(p2));
            } else if (in.PatientAgeGroup(p2) > max) {
              costRoomAgeMix -= (in.PatientAgeGroup(p2) - max);
            }
            if (in.PatientAgeGroup(p) < min) {
              costRoomAgeMix += (min - in.PatientAgeGroup(p));
            } else if (in.PatientAgeGroup(p) > max) {
              costRoomAgeMix += (in.PatientAgeGroup(p) - max);
            }
          }
        }
      }
    }
  }

  // Patient Delay
  if (d > in.PatientSurgeryReleaseDay(p)) {
    costPatientDelay -= (d - in.PatientSurgeryReleaseDay(p));
  }
  if (d2 > in.PatientSurgeryReleaseDay(p)) {
    costPatientDelay += (d2 - in.PatientSurgeryReleaseDay(p));
  }
  if (d2 > in.PatientSurgeryReleaseDay(p2)) {
    costPatientDelay -= (d2 - in.PatientSurgeryReleaseDay(p2));
  }
  if (d > in.PatientSurgeryReleaseDay(p2)) {
    costPatientDelay += (d - in.PatientSurgeryReleaseDay(p2));
  }

  // Room Skill Level and Excessive Nurse Workload
  // --> this assumes both patients have the same LOS!
  // Moving p2 to r
  if (d != -1) {
    for (int s = d * in.ShiftsPerDay();
         s < min(in.Shifts(), (d + los) * in.ShiftsPerDay()); s++) {
      int s1 = s - d * in.ShiftsPerDay();
      int n = sol.getRoomShiftNurse(r, s);

      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel -=
            (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n));
      }
      if (in.PatientSkillLevelRequired(p2, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            (in.PatientSkillLevelRequired(p2, s1) - in.NurseSkillLevel(n));
      }

      if (d != d2 || n != sol.getRoomShiftNurse(r2, s)) {
        if (sol.NurseShiftLoad(n, s) > in.NurseMaxLoad(n, s)) {
          if (sol.NurseShiftLoad(n, s) - in.PatientWorkloadProduced(p, s1) >
              in.NurseMaxLoad(n, s)) {
            costExcessiveNurseWorkload -= in.PatientWorkloadProduced(p, s1);
          } else {
            costExcessiveNurseWorkload -=
                (sol.NurseShiftLoad(n, s) - in.NurseMaxLoad(n, s));
          }
        }
        if (sol.NurseShiftLoad(n, s) - in.PatientWorkloadProduced(p, s1) +
                in.PatientWorkloadProduced(p2, s1) >
            in.NurseMaxLoad(n, s)) {
          if (sol.NurseShiftLoad(n, s) - in.PatientWorkloadProduced(p, s1) >
              in.NurseMaxLoad(n, s)) {
            costExcessiveNurseWorkload += in.PatientWorkloadProduced(p2, s1);
          } else {
            costExcessiveNurseWorkload +=
                (sol.NurseShiftLoad(n, s) - in.PatientWorkloadProduced(p, s1) +
                 in.PatientWorkloadProduced(p2, s1) - in.NurseMaxLoad(n, s));
          }
        }
      }
    }
  }
  // Moving p to r2
  if (d2 != -1) {
    for (int s = d2 * in.ShiftsPerDay();
         s < min(in.Shifts(), (d2 + los) * in.ShiftsPerDay()); s++) {
      int s1 = s - d2 * in.ShiftsPerDay();
      int n2 = sol.getRoomShiftNurse(r2, s);

      if (in.PatientSkillLevelRequired(p2, s1) > in.NurseSkillLevel(n2)) {
        costRoomSkillLevel -=
            (in.PatientSkillLevelRequired(p2, s1) - in.NurseSkillLevel(n2));
      }
      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n2)) {
        costRoomSkillLevel +=
            (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n2));
      }

      if (d != d2 || n2 != sol.getRoomShiftNurse(r, s)) {
        if (sol.NurseShiftLoad(n2, s) > in.NurseMaxLoad(n2, s)) {
          if (sol.NurseShiftLoad(n2, s) - in.PatientWorkloadProduced(p2, s1) >
              in.NurseMaxLoad(n2, s)) {
            costExcessiveNurseWorkload -= in.PatientWorkloadProduced(p2, s1);
          } else {
            costExcessiveNurseWorkload -=
                (sol.NurseShiftLoad(n2, s) - in.NurseMaxLoad(n2, s));
          }
        }
        if (sol.NurseShiftLoad(n2, s) - in.PatientWorkloadProduced(p2, s1) +
                in.PatientWorkloadProduced(p, s1) >
            in.NurseMaxLoad(n2, s)) {
          if (sol.NurseShiftLoad(n2, s) - in.PatientWorkloadProduced(p2, s1) >
              in.NurseMaxLoad(n2, s)) {
            costExcessiveNurseWorkload += in.PatientWorkloadProduced(p, s1);
          } else {
            costExcessiveNurseWorkload +=
                (sol.NurseShiftLoad(n2, s) -
                 in.PatientWorkloadProduced(p2, s1) +
                 in.PatientWorkloadProduced(p, s1) - in.NurseMaxLoad(n2, s));
          }
        }
      }
    }
  }

  vector<int> costs{costRoomAgeMix,
                    costRoomSkillLevel,
                    0,
                    costExcessiveNurseWorkload,
                    0,
                    0,
                    costPatientDelay,
                    0};

  int objValue = 0;
  for (int i = 0; i < costs.size(); i++) {
    objValue += costs[i] * in.Weight(i);
  }

  // if (objValue < 0)
  //{
  //     std::cout << "cost Room Skill Level: " << costRoomSkillLevel *
  //     in.Weight(1) << std::endl; std::cout << "cost Room Age Mix: " <<
  //     costRoomAgeMix * in.Weight(0) << std::endl; std::cout << "cost
  //     Excessive Nurse Workload: " << costExcessiveNurseWorkload *
  //     in.Weight(3) << std::endl; std::cout << "cost Patient Delay " <<
  //     costPatientDelay * in.Weight(6) << std::endl;
  // }

  return objValue;
}

Swap::~Swap() {}

#ifdef CLIQUE
boolean my_time_limit_function(int level, int n, int max, int user_time,
                               double system_time, double total_time,
                               clique_options *opts) {

  // Stop if more than 10 sec used
  if (total_time > 60) {
    return false; // Stop the algorithm
  }

  return true; // Continue execution
}
#endif

#ifdef CLIQUE
void write_dot_file(graph_t *g, const char *filename) {
  FILE *file = fopen(filename, "w");
  if (!file) {
    perror("Failed to open file");
    return;
  }

  fprintf(file, "graph G {\n");

  // Write nodes with weights if available
  for (int i = 0; i < g->n; ++i) {
    if (g->weights) {
      fprintf(file, "    %d [label=\"%d\"];\n", i + 1, i); // 1-based index
    } else {
      fprintf(file, "    %d;\n", i + 1);
    }
  }

  // Write edges
  for (int i = 0; i < g->n; ++i) {
    for (int j = i + 1; j < g->n; ++j) {
      if (GRAPH_IS_EDGE(g, i, j)) {
        fprintf(file, "    %d -- %d;\n", i + 1, j + 1); // DOT format
      }
    }
  }

  fprintf(file, "}\n");
  fclose(file);
  printf("Graph written to %s\n", filename);
}
#endif

#ifdef CLIQUE
void write_dimacs_file(graph_t *g, const char *filename) {
  FILE *file = fopen(filename, "w");
  if (!file) {
    perror("Failed to open file");
    return;
  }

  // Count edges
  int numEdges = 0;
  for (int i = 0; i < g->n; ++i) {
    for (int j = i + 1; j < g->n; ++j) {
      if (GRAPH_IS_EDGE(g, i, j)) {
        numEdges++;
      }
    }
  }

  // Write problem line
  fprintf(file, "p edge %d %d\n", g->n, numEdges);

  // Write weights (optional, non-standard in DIMACS)
  if (g->weights) {
    for (int i = 0; i < g->n; ++i) {
      fprintf(file, "n %d %d\n", i + 1, g->weights[i]); // 1-based index
    }
  }
  // Write edges
  for (int i = 0; i < g->n; ++i) {
    for (int j = i + 1; j < g->n; ++j) {
      if (GRAPH_IS_EDGE(g, i, j)) {
        fprintf(file, "e %d %d\n", i + 1,
                j + 1); // DIMACS uses 1-based indexing
      }
    }
  }

  fclose(file);
  printf("Graph written to %s\n", filename);
}
#endif

// Find a maximum weight clique with cliquer software
// See Cliquer User's Guide Niskanen
std::vector<int>
Swap::solveClique(std::vector<int> &vertexWeights,
                  std::vector<std::list<int>> &adjacency_list) {

  // Determine number of vertices
  int numVertices = vertexWeights.size();
  std::vector<int> cliqueVec;

#ifdef CLIQUE

  // Construct the graph
  graph_t *g = graph_new(numVertices);

  // Timer: max 10 sec.
  // parallel issues
  // clique_default_options->time_function = my_time_limit_function;

  // Add weights
  for (int i = 0; i < numVertices; ++i) {
    g->weights[i] = vertexWeights[i];
  }

  // Add all edges
  for (int i = 0; i < numVertices; ++i) {
    for (auto &j : adjacency_list[i]) {
      GRAPH_ADD_EDGE(g, i, j);
    }
  }

#ifndef NDEBUG
  write_dimacs_file(g, "graph.dimacs");
  write_dot_file(g, "graph.dot");
#endif

#ifndef NDEBUG
  clock_t start = clock();
#endif
  set_t clique = clique_find_single(g, 0, 0, FALSE, clique_default_options);
#ifndef NDEBUG
  clock_t end = clock();
  double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
  std::cout << "Elapsed time:" << elapsed_time << std::endl;
#endif

  for (int i = 0; i < numVertices; i++) {
    if (SET_CONTAINS(clique, i)) {
      cliqueVec.push_back(i);
    }
  }

  //// Cleanup
  graph_free(g);
  set_free(clique);

#endif

  return cliqueVec;
}

void Swap::clique(const int perturbationValue) {
  // Randomly pick two rooms
  const int r1 = rand() % in.Rooms();
  int randno = rand() % (in.Rooms() - 1);
  const int r2 = (randno < r1) ? randno : randno + 1;

  // Free the bed from start to finish
  int endTime = 0, currentCostBed = 0;
  std::vector<std::pair<int, int>>
      patients; // Patients removed from the bed: <patient id, current admission
                // day>.
  int temp = rand() % in.Days();
  freeBed(r1, 0, temp, endTime, currentCostBed, patients);
  freeBed(r2, endTime, in.Days(), temp, currentCostBed, patients);

  // We want to have different solutions...
  // For one of the original assignments, increase the cost!
  // Pick a patient with more than one possible starting time...
  std::vector<int> dummyVec(patients.size());
  std::iota(dummyVec.begin(), dummyVec.end(), 0);
  std::shuffle(dummyVec.begin(), dummyVec.end(), rng);
  // We will continue after determining the adjacencylist...

  // Check how many of the unscheduled patients are optional
  // Info needed for objective update
  int unscheduledOptional = 0;
  std::vector<int> tempVec;
  for (auto &p : patients) {
    if (!in.PatientMandatory(p.first)) {
      unscheduledOptional++;
      tempVec.push_back(p.first);
    }
  }

  // Randomly choose one optional patient for whom we increase the cost for
  // being assigned on its current day
  int perturbationIndex = (tempVec.size() == 0 ? -1 : rand() % tempVec.size());
  int perturbationPatient =
      (perturbationIndex == -1 ? -1 : patients[perturbationIndex].first);
  int perturbationPatientDay =
      (perturbationIndex == -1 ? -1 : patients[perturbationIndex].second);

  // Add all other currently unassigned patients
  // First store all unassigned ones in a set
  std::set<int> selectedPatients;
  for (auto &p : patients) {
    selectedPatients.insert(p.first);
  }
  for (int p = 0; p < in.Patients(); ++p) {
    if (!sol.ScheduledPatient(p) && !selectedPatients.count(p)) {
      patients.push_back({p, -1});
    }
  }

  // For all patients, determine all possible start times
  std::vector<int> weights;
  std::vector<std::array<int, 3>> patientInfo;
  int patient;
  int weight;
  for (auto &p : patients) {
    patient = p.first;
    for (int d = in.PatientSurgeryReleaseDay(patient);
         d <= in.PatientLastPossibleDay(patient); ++d) {
      weight = -1;
      if (d + in.PatientLengthOfStay(patient) <= endTime ||
          endTime >= in.Days()) {
        if (sol.isPatientFeasibleForRoom(patient, d, r1)) {
          weight = -1 * sol.CostEvaluationAddingPatient(false, patient, r1, d) +
                   (in.PatientMandatory(patient) ? 10 * in.Weight(7) : 0);
        }
      } else if (d >= endTime && sol.isPatientFeasibleForRoom(patient, d, r2)) {
        weight = -1 * sol.CostEvaluationAddingPatient(false, patient, r2, d) +
                 (in.PatientMandatory(patient) ? 10 * in.Weight(7) : 0);
      }
      if (patient == perturbationPatient && d == perturbationPatientDay) {
        weight -= perturbationValue;
      }
      if (weight >
          0) { // In rare cases, not assigning a patient might be cheaper...
        weights.push_back(weight);
        patientInfo.push_back(
            {patient, d, d + in.PatientLengthOfStay(patient)});
      }
    }
  }

#ifndef NDEBUG
  std::cout << "Patients freed: " << patients.size() << std::endl;
  std::cout << "Graph size: " << weights.size() << std::endl;
  for (int d = 0; d < in.Days(); ++d) {
    std::cout << (in.RoomCapacity((d < endTime) ? r1 : r2) -
                  sol.RoomDayLoad((d < endTime) ? r1 : r2, d))
              << " ";
  }
#endif

  // Construct the adjacency list
  int intervals = patientInfo.size();
  std::vector<std::list<int>> adjacency_list(intervals, std::list<int>{});
  for (int p = 0; p < intervals; ++p) {
    for (int q = p + 1; q < intervals; ++q) {
      // Patient p and q different and they do not overlap (p starts after q, or
      // q starts after p)
      if (patientInfo[p][0] != patientInfo[q][0] &&
          ((patientInfo[p][1] >= patientInfo[q][2]) ||
           (patientInfo[q][1] >= patientInfo[p][2]))) {
        adjacency_list[p].push_back(q);
      }
    }
  }

  if (weights.size()) {
    std::vector<int> clique = solveClique(weights, adjacency_list);
    int assignmentCost = 0;
    int day, bestT;
    for (auto &c : clique) {
      patient = patientInfo[c][0];
      day = patientInfo[c][1];
      bestT = sol.determineBestOTAndCosts(patient, day).at(0);

      if (patient == perturbationPatient && day == perturbationPatientDay) {
        weights[c] += perturbationValue;
      }

      assignmentCost +=
          (weights[c] - (in.PatientMandatory(patient) ? 10 * in.Weight(7) : 0));
#ifndef NDEBUG
      if (day < endTime) {
        std::cout << "*** Assign " << patient << " on " << day << " in room "
                  << r1 << " and theater " << bestT << std::endl;
        std::cout << "Cost: "
                  << sol.CostEvaluationAddingPatient(false, patient, r1, day,
                                                     bestT)
                  << std::endl;
      } else {
        std::cout << "*** Assign " << patient << " on " << day << " in room "
                  << r2 << " and theater " << bestT << std::endl;
        std::cout << "Cost: "
                  << sol.CostEvaluationAddingPatient(false, patient, r2, day,
                                                     bestT)
                  << std::endl;
      }
      std::cout << "Mandat: " << in.PatientMandatory(patient) << std::endl;
      std::cout << "Node: " << c << " with weight " << weights[c] << " ("
                << (-weights[c] +
                    (in.PatientMandatory(patient) ? 10 * in.Weight(7) : 0))
                << ")" << std::endl;
      std::cout << "Weight: " << in.Weight(7) << std::endl;
#endif

      if (day < endTime) {
        sol.AssignPatient(patient, day, r1, bestT);
        assignPatients.push_back(patient);
      } else {
        sol.AssignPatient(patient, day, r2, bestT);
        assignPatients.push_back(patient);
      }
    }

#ifndef NDEBUG
    int oldObj = sol.getObjValue();
#endif

    // Update the objective function using the delta info
    sol.setObjValue(sol.getObjValue() + currentCostBed - assignmentCost +
                    unscheduledOptional * in.Weight(7));

#ifndef NDEBUG
    int check = sol.getObjValue();
    std::cout.setstate(std::ios::failbit);
    sol.PrintCosts();
    std::cout.clear();
    assert(sol.getObjValue() == check);
    // Pure local search
    // Pure local search
    std::cout << "New: " << sol.getObjValue() << std::endl;
    std::cout << "Old: " << oldObj << std::endl;
    std::cout << "Perturb: " << perturbationValue << std::endl;
    assert(sol.getObjValue() <= oldObj + perturbationValue);
#endif
  }

  return;
}

// Free a bed over the entire time horizon
// Idea: scan from left to right.
// - If there is at least one free bed in the room during the considered day,
// select it with high prob.
// - If no bed is available, or if the prob was rejected in prev line, we need
// to select a patient.
// - Check if there is a patient admitted on the considered day (but never
// select occupants!).
// - If there is such patient, randomly select one and free the bed for the
// patients' length of stay. Move
//   the same number of periods to the right.
// - If there is no such patient, it must be there is at least one free spot...
// Select the spot. (This can
//   only happen if we rejected the prob. of taking the free spot.)
// Free bed in room r between [startTime, requestedEndTime[
void Swap::freeBed(const int &r, const int &startTime,
                   const int &requestedEndTime, int &endTime,
                   int &currentCostBed,
                   std::vector<std::pair<int, int>> &patients) {
  int day = startTime;
  while (day < requestedEndTime) {
    // If room empty on day, skip (bed is already free)
    // Or, if least one bed free, skip with prob 2/3 (sometimes select long
    // patients)
    const int usage = sol.RoomDayLoad(r, day);
    if (usage == 0 || (usage < in.RoomCapacity(r) && rand() % 3 != 0)) {
      day++;
      continue;
    }

    // Choose a random patient admitted to the room during this day
    std::vector<int> admittedPatients;
    int noOccup = 0;
    for (int p = 0; p < sol.RoomDayLoad(r, day); ++p) {
      int patient = sol.RoomDayPatient(r, day, p);
      if (patient >= in.Patients())
        noOccup++;
      if (patient < in.Patients() &&
          sol.AdmissionDay(patient) ==
              day) { // Not an occupant and admitted today
        admittedPatients.push_back(patient);
      }
    }

    // If at least one patient is admitted during this day, randomly pick one,
    // otherwise skip the day
    if (admittedPatients.empty()) {
      // If there is no patient admitted, then there must be at least one free
      // bed... Unless it is full of occupants...
      assert(startTime > 0 || sol.RoomDayLoad(r, day) < in.RoomCapacity(r) ||
             noOccup ==
                 in.RoomCapacity(r)); // No longer valid in case of two rooms...
      day++;
    } else {
      // Store the patient as being free
      int patient = admittedPatients[rand() % admittedPatients.size()];

      // The priority of patients currently assigned is equal to their current
      // admission day. This makes the current solution remains feasible.
      patients.push_back({patient, day});

#ifndef NDEBUG
      int theater = sol.PatientOperatingTheater(patient);
      std::cout << "**** Unassign " << patient << " on day " << day
                << " in room " << r << " and OT " << theater << std::endl;
#endif

      // Update costs
      int value = sol.CostEvaluationRemovingPatient(false, patient);
      currentCostBed += value;

      // The function CostEvaluationRemoving patient assumes the patient to be
      // left unassigned, increasing the optional patient not assigned costs.
      // Here, we only want to consider the costs when the patient was assigned
      // to the bed.
      if (!in.PatientMandatory(patient)) {
        currentCostBed -= in.Weight(7);
      }

      // Unassign from the solution
      unassignPatients.push_back({ patient, sol.AdmissionDay(patient), sol.PatientRoom(patient), sol.PatientOperatingTheater(patient) });
      sol.UnassignPatient(patient);

      // Safety check: the old assignment must stay feasible...
      assert(sol.isPatientFeasibleForRoom(patient, day, r));

      // Safety check 2: and it should have the same cost...
      assert(-1 * value ==
             sol.CostEvaluationAddingPatient(false, patient, r, day, theater));

      // Update day
      day += in.PatientLengthOfStay(patient);
    }
  }
  endTime = day;

  return;
}

void Swap::shortestPathPatients(const int perturbationValue) {
  // Choose a random room in which we would like to optimize a bed
  const int r = rand() % in.Rooms();

  // Patient considered in the network <patientId, priority>
  // Priority is the current admission day for assigned patients or
  // a random day between release day and due date for unassigned patients
  std::vector<std::pair<int, int>> patients;

  // Current assignment (currentCostBed; i.e., continuity of care,...)
  // + unassignment costs for all patients in the vector
  int currentCostBed = 0;
  int currentCostUnassigned = 0;

  // Free bed from start to finish
  int endTime;
  freeBed(r, 0, in.Days(), endTime, currentCostBed, patients);
  std::unordered_set<int> patientsBed;
  for (auto &p : patients) {
    patientsBed.insert(p.first);
  }

  // Randomly choose one perturbation patient...
  const int perturbationIndex =
      (patients.size() == 0) ? -1 : rand() % patients.size();
  const int perturbationPatient =
      (patients.size() == 0) ? -1 : patients[perturbationIndex].first;
  const int perturbationPatientDay =
      (patients.size() == 0) ? -1 : patients[perturbationIndex].second;

  // Try to insert unassigned patients with at least one possible start day
  // Store for each patient all possible start days
  std::unordered_map<int, std::vector<int>> possibleStartDays;
  for (int p = 0; p < in.Patients(); ++p) {
    if (patientsBed.count(p)) {
      int lastDay = in.PatientLastPossibleDay(p);
      for (int d = in.PatientSurgeryReleaseDay(p); d <= lastDay; ++d) {
        if (sol.isPatientFeasibleForRoom(p, d, r)) {
          possibleStartDays[p].push_back(d);
        }
      }
      // Patients previously assigned can start during at least one day
      assert(possibleStartDays.count(p));
    } else if (!sol.ScheduledPatient(p)) {
      assert(!in.PatientMandatory(p));
      int lastDay = in.PatientLastPossibleDay(p);
      for (int d = in.PatientSurgeryReleaseDay(p); d <= lastDay; ++d) {
        if (sol.isPatientFeasibleForRoom(p, d, r)) {
          possibleStartDays[p].push_back(d);
        }
      }

      if (possibleStartDays.count(p)) {
        // Randomly pick one possible start time
        patients.push_back(
            {p, possibleStartDays[p][rand() % possibleStartDays[p].size()]});
        currentCostUnassigned -= in.Weight(7);
      }
    }
  }

  // Sort the patients on priority
  // Default sorting: Sorts first by the first element, then by the second
  std::sort(patients.begin(), patients.end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });

  // ****************************************************************************************************************
  // Construct the graph..
  // This graph is a lattice, where each row corresponds to a time slot, and
  // each column corresponds to a patient We only go to the right (= not
  // scheduling a patient), to the bottom (= leaving the bed empty for one
  // slot), or diagonal to the bottom right (=scheduling the patient in the bed)
  // One dummy row is added near the bottom, which is the final slot right after
  // the time horizon. One dummy column is added which represents an end node.
  // Hence, this graph is a DAG. We look for a shortest path from top left to
  // bottom right using a labelling algo implemented in Boost.
  // ****************************************************************************************************************

  // Define the graph using Boost's adjacency list
  const int numVertices =
      (in.Days() + 1) *
      (patients.size() + 1); // dummy start, dummy end, dummy end layer
  Graph g(numVertices);      // Type defined in .h file

  // Add horizontal edges for not scheduling the patient
  // And vertical edges for skipping a period
  // Add diagonal edges for scheduling the patient
  for (int p = 0; p < patients.size(); ++p) {
    const int patient = patients[p].first;

    // Horizontal edges
    if (!in.PatientMandatory(patient)) {
      for (int d = 0; d <= in.Days(); ++d) {
        // Meaning: patient is not scheduled
        // Only optional patients can be skipped!
        boost::add_edge(p * (in.Days() + 1) + d, (p + 1) * (in.Days() + 1) + d,
                        in.Weight(7), g);
      }
    }

    // Vertical edges
    for (int d = 0; d < in.Days(); ++d) {
      boost::add_edge(p * (in.Days() + 1) + d, p * (in.Days() + 1) + d + 1, 0,
                      g);
    }

    // Diagonal edges
    assert(possibleStartDays.count(patient));
    for (auto &d : possibleStartDays[patient]) {
      // Patients near the end are released shorter than final due day
      int cost = sol.CostEvaluationAddingPatient(false, patient, r, d) +
                 (in.PatientMandatory(patient) ? 0 : in.Weight(7));
      if (patient == perturbationPatient && d == perturbationPatientDay)
        cost += perturbationValue;
      boost::add_edge(
          p * (in.Days() + 1) + d,
          (p + 1) * (in.Days() + 1) +
              std::min(in.Days(), d + in.PatientLengthOfStay(patient)),
          cost, g);
    }
  }

  // Vertical edges between the last dummy layer
  for (int d = 0; d < in.Days(); ++d) {
    boost::add_edge(patients.size() * (in.Days() + 1) + d,
                    patients.size() * (in.Days() + 1) + d + 1, 0, g);
  }

#ifndef NDEBUG
  // Generate a dot file in debugging mode only
  // To visualize the graph, run the following commands in a linux terminal
  // $ dot -Tpdf graph.dot -o graph.pdf
  // Open graph.pdf
  std::ofstream dotFile("graph.dot");
  boost::write_graphviz(dotFile, g);
  dotFile.close();
#endif

  // Compute shortest paths using Boost DAG shortest paths algorithm
  // Additional properties for retrieiving the path
  vector<int> distances(numVertices, (numeric_limits<int>::max)());
  vector<Vertex> predecessor(numVertices, -1); // Stores the shortest path tree
  Vertex source = 0;
  dag_shortest_paths(
      g, source,
      boost::distance_map(&distances[0]).predecessor_map(&predecessor[0]));

  // Retrieve the solution from target to source
  int perturbationCorrection = 0;
  for (Vertex v = numVertices - 1; v != source; v = predecessor[v]) {
    int pred = predecessor[v];
    if (pred == v) { // No path exists
#ifndef NDEBUG
      int day_pred = pred % (in.Days() + 1);
      int patient_pred = patients[pred / (in.Days() + 1)].first;
      int day_v = v % (in.Days() + 1);
      int patient_v = patients[v / (in.Days() + 1)].first;

      std::cout << "******* WARNING FUNCTION shortestPathPatients ********"
                << std::endl;
      std::cout << "No path from source to sink..." << std::endl;
      std::cout << "Pred " << pred << "; patient = " << patient_pred << " day "
                << day_pred << std::endl;
      std::cout << "V " << v << "; patient = " << patient_v << " day " << day_v
                << std::endl;
      std::abort();
#else
      // Do not accept the move by setting its cost to a very high number
      distances[numVertices - 1] = 10000000;
      break;
#endif
    }
    // There is an edge from predecessor to v
    // Is this a diagonal edge?
    if (pred != v - 1 &&               // Vertical edge
        pred != v - (in.Days() + 1)) { // Horizontal edge
      // It must be a horizontal adge!
      int day = pred % (in.Days() + 1);
      int patient = patients[pred / (in.Days() + 1)].first;
      int bestT = sol.determineBestOTAndCosts(patient, day).at(0);
      if (patient == perturbationPatient && day == perturbationPatientDay)
        perturbationCorrection -= perturbationValue;
#ifndef NDEBUG
      std::cout << "*** Assign " << patient << " on " << day << " in room " << r
                << " and theater " << bestT << std::endl;
#endif
      sol.AssignPatient(patient, day, r, bestT);
      assignPatients.push_back(patient);
    }
  }

  // Update the objective function using the delta info
#ifndef NDEBUG
  int oldObj = sol.getObjValue();
#endif
  sol.setObjValue(sol.getObjValue() + currentCostBed + currentCostUnassigned +
                  distances[numVertices - 1] + perturbationCorrection);

#ifndef NDEBUG
  assert(sol.getObjValue() <= oldObj + perturbationValue);
#endif
}

void Swap::reverse()
{
    for (int p : assignPatients)
    {
        sol.UnassignPatient(p);
    }
    for (vector<int> p : unassignPatients)
    {
        sol.AssignPatient(p[0], p[1], p[2], p[3]);
    }
    for (vector<int> n : assignNurses)
    {
        sol.UnassignNurse(n[0], n[1], n[2]);
    }
    for (vector<int> n : unassignNurses)
    {
        sol.AssignNurse(n[0], n[1], n[2]);
    }
}


Solution Swap::solve(int noIterations) {

    //  swapOperatingTheaters();
    //
    //  // Not with a time limit, simply iteration based
    //  for (int i = 0; i < noIterations; ++i) {
    //    swapPatients();
    //    swapNursesAssignment(1);
    //
    //#ifndef NDEBUG
    //    int objValue = sol.getObjValue();
    //    sol.PrintCosts();
    //    std::cout << "ObjValue according to swaps: " << objValue << std::endl;
    //    std::cout << "Real ObjValue: " << sol.getObjValue() << std::endl;
    //    assert(objValue == sol.getObjValue());
    //#endif
    //
    //    // No room is yet assigned --> Cannot be run yet
    //    // shortestPathPatients();
    //  }

    return sol;
}
// Deprecated. Use solution = out instead
// void Swap::loadSolutionSwap(const Solution &solution)
//
//    // Copy sol to be equal to solution
//{
//    sol.Reset();
//    sol.setInfValue(solution.getInfValue());
//    sol.setObjValue(solution.getObjValue());
//
//    for (int p = 0; p < in.Patients(); p++)
//    {
//        int d = solution.AdmissionDay(p);
//        if (d >= 0)
//        {
//            int r = solution.PatientRoom(p);
//            int t = solution.PatientOperatingTheater(p);
//            sol.AssignPatient(p, d, r, t);
//        }
//    }
//    for (int r = 0; r < in.Rooms(); r++)
//    {
//        for (int s = 0; s < in.Shifts(); s++)
//        {
//            int n = solution.getRoomShiftNurse(r, s);
//            if (n != -1) { sol.AssignNurse(n, r, s); }
//        }
//    }
//}

// Deprecated. Use solution = out instead.
// void Swap::saveSolutionSwap(Solution &solution) {
//
//    // Copy out to be equal to solution
//    out.Reset();
//    out.setInfValue(sol.getInfValue());
//    out.setObjValue(sol.getObjValue());
//
//    for (int p = 0; p < in.Patients(); p++)
//    {
//        int d = sol.AdmissionDay(p);
//        if (d >= 0)
//        {
//            int r = sol.PatientRoom(p);
//            int t = sol.PatientOperatingTheater(p);
//            out.AssignPatient(p, d, r, t);
//        }
//    }
//    for (int r = 0; r < in.Rooms(); r++)
//    {
//        for (int s = 0; s < in.Shifts(); s++)
//        {
//            int n = sol.getRoomShiftNurse(r, s);
//            if (n != -1) { out.AssignNurse(n, r, s); }
//        }
//    }
//}

//bool Swap::kickToRoom(int p_i, int room) {
//  //? assume that the patient is elective
//  if (in.IncompatibleRoom(p_i, room)) {
//    throw std::runtime_error("incompatible room!\n");
//  }
//  if (!sol.ScheduledPatient(p_i)) {
//    // or should we simply return false?
//    throw std::runtime_error(
//        "Patient is not scheduled! So we cannot kick the patient!!!\n");
//  };
//  int room_cap = in.RoomCapacity(room);
//  // length of stay
//  int los = in.PatientLengthOfStay(p_i);
//  Gender p_i_gender = in.PatientGender(p_i);
//  int delta = 0;
//  delta += sol.CostEvaluationRemovingPatient(false, p_i);
//  // sol.UnassignPatient(p);
//  // initialized with all zeroes
//  bitset<28> room_cap_bitset;
//  bitset<28> ot_cap_bitset;
//  for (int d = in.PatientSurgeryReleaseDay(p_i); d < in.Days(); d++) {
//    //? check ot
//    if (sol.isPatientSurgeryPossibleOnDay(p_i, d)) {
//      // set the ot_capacity_bit to 1 for this day
//      printf("%d\n", d);
//      ot_cap_bitset.flip(d);
//    };
//    //? check ward
//    int room_load = sol.RoomDayLoad(room, d);
//    if (!sol.isRoomGender(p_i_gender, room, d)) {
//      continue;
//    }
//    if (room_cap - room_load >= 1) {
//      // set the room_capacity_bit to 1 for this day
//      room_cap_bitset.flip(d);
//    }
//  }
//  printf("room_cap_bitset: %s\n", room_cap_bitset.to_string().c_str());
//  printf("ot_cap_bitset: %s\n", ot_cap_bitset.to_string().c_str());
//  fflush(stdout);
//  for (int d = in.PatientSurgeryReleaseDay(p_i); d < in.Days(); d++) {
//    if (!sol.isRoomGender(p_i_gender, room, d)) {
//      // the room is not the right gender right now, so we should skip this day
//      continue;
//    }
//
//    vector<int> patients_in_room = sol.getPatientsRoomDay(room, d);
//    // if removing patient_j would aid the patient that we are making room for,
//    // then consider patient_j.
//    for (int p_j : patients_in_room) {
//      int end_day = sol.patientEndDay(p_j);
//      if (end_day - d < los) {
//        // check if the room is not full in the next days
//        continue;
//      }
//    }
//  }
//  return true;
//}

//bool Swap::kickForward(int p, int num_pos) {
//  int current_day = sol.AdmissionDay(p);
//  int los = in.PatientLengthOfStay(p);
//  int room = sol.PatientRoom(p);
//  int total_cap = in.RoomCapacity(room);
//  int d = -1;
//  int due_date = in.PatientSurgeryDueDay(p);
//  Gender gender = in.PatientGender(p);
//  //? This means that the patient is mandatory:
//  //? instead of checking whether the patient is mandatory,
//  //? we can save some computations
//  if (due_date != -1) {
//    num_pos = due_date - current_day;
//  }
//  while (true) { // current patients in this room:
//
//    if (current_day + d + los + 1 >= in.Days()) {
//      d = in.Days() - 1 - current_day;
//      break;
//    }
//    //? if there is room available, we can simply do the kick gg ez katka
//    if (sol.isPatientFeasibleForRoomOnDay(p, current_day + d + los + 1)) {
//      // probably in a loop just increment the day
//      if (d == num_pos) {
//        break;
//      }
//      d++;
//    } else {
//      // no space for the kick :(
//      break;
//    }
//  }
//  if (d >= num_pos || d == -1) {
//    //! this means that there was no space to kick the patient
//    return false;
//  } else {
//    //! we can kick the patient forwards
//    //! if there is room available in ots
//    while (d > 0) {
//      if (sol.isPatientSurgeryPossibleOnDay(p, current_day + d + 1)) {
//        int costChange = sol.CostEvaluationRemovingPatient(false, p);
//        sol.UnassignPatient(p);
//        int ot = sol.determineBestOTAndCosts(p, current_day + d + 1).at(0);
//        costChange += sol.CostEvaluationAddingPatient(false, p, room,
//                                                      current_day + d + 1, ot);
//        assert(sol.isPatientFeasibleForRoom(p, current_day + d + 1, room));
//        sol.AssignPatient(p, current_day + d + 1, room, ot);
//        sol.setObjValue(sol.getObjValue() + costChange);
//        return true;
//      }
//      d--;
//    }
//    return false;
//  }
//}

//int Swap::costInfeasiblePatientSwap(int p, int p2) {
//  // --> This function assumes the current solution is feasible, so only
//  // calculates positive infValue RoomGenderMix: assumed ok because compatible
//  // patients RoomCapacity: assumed ok because patients have same LOS
//  // OperatingTheatreOvertime: assumed that the swap function will take this
//  // into consideration
//
//  int infValue = 0;
//
//  // Room Compatability
//  int r = sol.PatientRoom(p);
//  int r2 = sol.PatientRoom(p2);
//
//  if (r2 != -1 && in.IncompatibleRoom(p, r2)) {
//    infValue++;
//  }
//  if (r != -1 && in.IncompatibleRoom(p2, r)) {
//    infValue++;
//  }
//
//  // Surgeon Overtime
//  int d = sol.AdmissionDay(p);
//  int s = in.PatientSurgeon(p);
//  int d2 = sol.AdmissionDay(p2);
//  int s2 = in.PatientSurgeon(p2);
//
//  if (d2 != -1 && d != d2 &&
//      sol.SurgeonDayLoad(s, d2) + in.PatientSurgeryDuration(p) >
//          in.SurgeonMaxSurgeryTime(s, d2)) {
//    infValue += sol.SurgeonDayLoad(s, d2) + in.PatientSurgeryDuration(p) -
//                in.SurgeonMaxSurgeryTime(s, d2);
//  }
//  if (d != -1 && d != d2 &&
//      sol.SurgeonDayLoad(s2, d) + in.PatientSurgeryDuration(p2) >
//          in.SurgeonMaxSurgeryTime(s2, d)) {
//    infValue += sol.SurgeonDayLoad(s2, d) + in.PatientSurgeryDuration(p2) -
//                in.SurgeonMaxSurgeryTime(s2, d);
//  }
//
//  // Admission Day
//  if (d2 != -1 && (d2 < in.PatientSurgeryReleaseDay(p) ||
//                   d2 > in.PatientLastPossibleDay(p))) {
//    infValue++;
//  }
//  if (d != -1 && (d < in.PatientSurgeryReleaseDay(p2) ||
//                  d > in.PatientLastPossibleDay(p2))) {
//    infValue++;
//  }
//
//  //  Mandatory Unscheduled Patients
//  if (d != -1 && d2 == -1 && in.PatientMandatory(p)) {
//    infValue++;
//  }
//  if (d2 != -1 && d == -1 && in.PatientMandatory(p2)) {
//    infValue++;
//  }
//
//  return infValue;
//}

//set<int> Swap::getCompatiblePatients(int p) {
//
//  set<int> compatiblePatients;
//  // Compatible patients are patients with the same gender and overlapping
//  // windows of admission
//  for (int p2 = 0; p2 < in.Patients(); p2++) {
//    if (p != p2 && in.PatientGender(p) == in.PatientGender(p2) &&
//        in.PatientSurgeryReleaseDay(p) <= in.PatientLastPossibleDay(p2) &&
//        in.PatientSurgeryReleaseDay(p2) <= in.PatientLastPossibleDay(p)) {
//      compatiblePatients.insert(p2);
//    }
//  }
//
//  return compatiblePatients;
//}

//vector<int> Swap::FindNewOperatingTheater(int p, int p2) {
//  // Returns vector of size 3: new ot for p, new ot for p2, total cost change
//  // --> We only return feasible new OTs (if possible)
//  int newOT = -1;
//  int newOT2 = -1;
//  int cost = 0;
//
//  int d = sol.AdmissionDay(p);
//  int d2 = sol.AdmissionDay(p2); // It is assumed d != d2
//  assert(d != d2);
//
//  // Day d: adding p2 and removing p
//  if (d != -1) {
//    // Surgeon transfer cost and Open operating theater cost for removing p
//    pair<int, int> costOT = sol.costEvaluationRemoveOT(p);
//    cost += costOT.first * in.Weight(4) + costOT.second * in.Weight(5);
//
//    // Find new OT for p2
//    int newOT2 = -1;
//    vector<int> bestOT = sol.determineBestOTAndCosts(p2, d);
//    // Also need to check cost of the current ot of p
//    int ot = sol.PatientOperatingTheater(p);
//    int costOldOT = in.Weight(4) + in.Weight(5) + 1;
//    if (sol.OperatingTheaterDayLoad(ot, d) + in.PatientSurgeryDuration(p2) -
//            in.PatientSurgeryDuration(p) <=
//        in.OperatingTheaterAvailability(ot, d)) {
//      costOldOT = 0;
//      int s2 = in.PatientSurgeon(p2);
//      if (in.PatientSurgeon(p) != s2) {
//        if (sol.SurgeonDayTheaterCount(s2, d, ot) == 0) // Otherwise no transfer
//        {
//          int t2 = 0;
//          bool transfer = false;
//          while (t2 != ot && t2 < in.OperatingTheaters() && !transfer) {
//            if (sol.SurgeonDayTheaterCount(s2, d, t2) > 0) {
//              transfer = true;
//            }
//            t2++;
//          }
//          if (transfer) {
//            costOldOT += in.Weight(5);
//          }
//        }
//      }
//      if (sol.OperatingTheaterDayLoad(ot, d) == in.PatientSurgeryDuration(p)) {
//        costOldOT += in.Weight(4);
//      }
//      newOT2 = ot;
//    }
//    cost += costOldOT;
//    if (bestOT[0] != -1 && bestOT[0] != sol.PatientOperatingTheater(p)) {
//      int costNewOT = bestOT[1] * in.Weight(4) + bestOT[2] * in.Weight(5);
//      if (costNewOT < costOldOT) {
//        newOT2 = bestOT[0];
//        cost += costNewOT - costOldOT;
//      }
//    }
//  }
//  // Day d2: adding p and removing p2
//  if (d2 != -1) {
//    // Surgeon transfer cost and Open operating theater cost for removing p2
//    pair<int, int> costOT = sol.costEvaluationRemoveOT(p2);
//    cost += costOT.first * in.Weight(4) + costOT.second * in.Weight(5);
//
//    // Find new OT for p
//    int newOT = -1;
//    vector<int> bestOT = sol.determineBestOTAndCosts(p, d2);
//    // Also need to check cost of the current ot of p2
//    int ot2 = sol.PatientOperatingTheater(p2);
//    int costOldOT = 3;
//    if (sol.OperatingTheaterDayLoad(ot2, d2) + in.PatientSurgeryDuration(p) -
//            in.PatientSurgeryDuration(p2) <=
//        in.OperatingTheaterAvailability(ot2, d2)) {
//      costOldOT = 0;
//      int s = in.PatientSurgeon(p);
//      if (in.PatientSurgeon(p2) != s) {
//        if (sol.SurgeonDayTheaterCount(s, d2, ot2) ==
//            0) // Otherwise no transfer
//        {
//          int t2 = 0;
//          bool transfer = false;
//          while (t2 != ot2 && t2 < in.OperatingTheaters() && !transfer) {
//            if (sol.SurgeonDayTheaterCount(s, d2, t2) > 0) {
//              transfer = true;
//            }
//            t2++;
//          }
//          if (transfer) {
//            costOldOT += in.Weight(5);
//          }
//        }
//      }
//      if (sol.OperatingTheaterDayLoad(ot2, d2) ==
//          in.PatientSurgeryDuration(p2)) {
//        costOldOT += in.Weight(4);
//      }
//      newOT = ot2;
//    }
//    cost += costOldOT;
//    if (bestOT[0] != -1 && bestOT[0] != sol.PatientOperatingTheater(p2)) {
//      int costNewOT = bestOT[1] * in.Weight(4) + bestOT[2] * in.Weight(5);
//      if (costNewOT < costOldOT) {
//        newOT = bestOT[0];
//        cost += costNewOT - costOldOT;
//      }
//    }
//  }
//
//  return {newOT, newOT2, cost};
//}
