#ifndef SOLUTION_H
#define SOLUTION_H
#include "IHTP_Validator.h"
#include "json.hpp"
#include <unordered_set>
using json = nlohmann::ordered_json;
#pragma once

class Solution : public IHTP_Output {
public:
  /**
   * @brief Load in another solution
   *
   * @param solution - object to load in.
   */
  void loadSolution(const Solution &solution);
  void to_json(string filename) const;

  // Getters
  vector<int> getPatientShiftNurses(const int patient) const {
    return this->patient_shift_nurse.at(patient);
  }
  int getRoomShiftNurse(const int r, const int s) const {
    return room_shift_nurse.at(r).at(s);
  }
  vector<int> getPatientsRoomDay(const int r, const int d) const {
    return this->room_day_patient_list.at(r).at(d);
  }
  vector<int> getPatientsOTDay(const int t, const int d) const {
    return this->operatingtheater_day_patient_list.at(t).at(d);
  }
  int PatientOperatingTheater(const int p) const {
    return operating_room.at(p);
  }
  // returns the room the patient is assigned to (or -1 if no room)
  int PatientRoom(const int p) const { return room.at(p); }
  int RoomShiftSkillLevel(int r, int s);
  int NurseShiftWorkload(const int n, const int s) const {return nurse_shift_load.at(n).at(s);}
  /**
   * @brief Checks if a patient can be placed on a day when `p2` is excluded
   * 
   * Does all checks
   */
  bool isPatientFeasibleOnDayWithExcludedPatient(int p, int d, int r, int p2);
  bool isPatientFeasibleForRoomOnDayWithExcludedPatient(int p, int d, int r, int p2);
  bool isPatientFeasibleForRoom(int p, int d, int r);
  /**
   * @brief Single-repsonsibility! Checks room capacity AND gender! Does not
   * loop over all rooms!
   *
   * @param p - patient
   * @param d - day
   * @param r - room
   * @return true - patient can be presented in room
   * @return false - patient cannot be presented in room
   */
  bool isPatientFeasibleForRoomOnDay(int p, int d, int r = -1);

  /**
   * @brief Check if patient can be assigned to an operating theater and its
   * surgeon
   *
   * @param p
   * @param d
   * @return true - to quote lil john: yeayuh yeayuh
   * @return false - nope
   */
  bool isPatientSurgeryPossibleOnDay(int p, int d);

  /**
   * @brief SINGLE-RESPONSIBILITY! Checks whether there is still room in the
   * current sol for this patient on this ot-day
   *
   * @param p - patient
   * @param t - ot
   * @param d - day
   * @return true - ot has room for this surgery
   * @return false - ot has no room
   */
  bool isPatientFeasibleForOT(int p, int t, int d);
  /**
   * @brief SINGLE-RESPONSIBILITY! This method only checks whether a patient can
   * fit in a room. There are no side-effects.
   *
   *
   * @param p - patient
   * @param r - room
   * @param d - day
   * @return true - room has space left in the coming days
   * @return false - room has no space left in the coming days
   */
  bool isPatientFeasibleForRoomDayPure(int p, int r, int d);
  // Assign actions
  void UnassignPatient(int p);
  void UnassignPatientOperatingTheater(int p);
  void AssignPatientOperatingTheater(int p, int t);
  void UnassignNurse(int n, int r, int s);

  // Cost Evaluation functions
  int CostEvaluationAddingNurse(bool verbose, int n, int r, int s);
  int CostEvaluationRemovingNurse(bool verbose, int n, int r, int s);
  int CostEvaluationAddingPatient(bool verbose, int p, int r, int d,
                                  int t = -1);
  int CostEvaluationRemovingPatient(bool verbose, int p);
  int CostAddingNurseNoCOC(bool verbose, int n, int r, int s);
  /**
  * @brief Returns vector with three elements: best operating theater t, cost of open OT
    t, cost of surgeon transfer t Returns {-1, 2, 2} if no feasible ot exists
*/
  vector<int> determineBestOTAndCosts(int p, int d);
  /**
   * @brief Returns true if `gender` is allowed in room `r` on day `d`
   * 
   * @param gender - Gender
   * @param r - room
   * @param d - day
   * @return true - If the room has no gender, or if the gender is the same.
   * @return false - gender not possible
   */
  bool isRoomGender(Gender gender, int r, int d);
  /**
   * @brief Get last day in the hospital of this patient
   * 
   * @param p 
   * @return int - last day in the hospital (-1 if patient not assigned)
   */
  int patientEndDay(int p);

  // To know whether a patient sees a nurse outside the shifts considered in the IP
  void ComputeBetas(vector<int>& RoomsId, vector<int>&ShiftsId, vector<vector<bool>>& beta_in, vector<vector<bool>>& beta_jn);

  int CostEvaluationRemovingNurseRoomsShifts(
      vector<int> &RoomsId, vector<int> &ShiftsId,
      vector<vector<bool>> &beta_in, vector<vector<bool>> &beta_jn,
      vector<vector<int>> &CapacitiesNurses);

  int costEvaluationOTDay(int d);

  pair<int, int> costEvaluationAssignOT(int p, int d, int t);
  pair<int, int> costEvaluationRemoveOT(int p);

  // Sort functions
  vector<int> sortedPatientsSurgeon(vector<int> patients);
  static bool compareAscending(const pair<int, int> &p1,
                               const pair<int, int> &p2);
  static bool compareDescending(const pair<int, int> &p1,
                                const pair<int, int> &p2);
  static bool compareDoubleAscending(const vector<int> p1,
                                     const vector<int> p2);

  Solution &operator=(const Solution &other);
  Solution(const IHTP_Input &in, const bool verbose);
  Solution(const IHTP_Input &my_in, string file_name, bool verbose)
      : IHTP_Output(my_in, file_name, verbose) {}
  ~Solution();

private:
  double objValue = -1; // Objective value
  double infValue = -1; // Infeasibility value
};

#endif
