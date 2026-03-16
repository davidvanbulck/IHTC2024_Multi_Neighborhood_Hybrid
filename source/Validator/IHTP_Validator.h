#ifndef CLASSCONSTRAINT_H
#define CLASSCONSTRAINT_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "json.hpp"
using namespace std; 

enum class Gender { A, B };

class Occupant
{
 public: 
  string id; 
  Gender gender;
  int age_group;
  int length_of_stay;
  vector<int> workload_produced;
  vector<int> skill_level_required;
  int assigned_room = -1;
};

class Patient: public Occupant
{
 public: 
  bool mandatory;
  int surgery_release_day;
  int surgery_due_day;
  int surgery_duration;
  int surgeon;
  vector<bool> incompatible_rooms;
};

class Surgeon
{
 public: 
  string id;
  vector<int> max_surgery_time;
};

class OperatingTheater
{
 public: 
  string id;
  vector<int> availability;
};

class Room
{
 public: 
  string id;
  int capacity;
};

class Nurse
{
 public: 
  string id;
  int skill_level;
  vector<int> working_shifts; // list of working shifts
  vector<bool> is_working_in_shift; // for all shift
  vector<int> max_loads; // for all shifts (0 when absent)
};

class IHTP_Input
{
 public:
  IHTP_Input(string file_name);
  int Days() const { return days; }
  int ShiftsPerDay() const { return shifts_per_day; }
  int Shifts() const { return shifts; }
  int Patients() const { return patients; }
  int Occupants() const {return occupants; }
  int SkillLevels() const { return skill_levels; }
  int AgeGroups() const { return age_groups; }
  int Surgeons() const { return surgeons; }
  int OperatingTheaters() const { return operating_theaters; }
  int Rooms() const { return rooms; }
  int Nurses() const { return nurses; }

// Nurses
  string NurseId(int n) const { return nurses_vect[n].id; }
  int NurseSkillLevel(int n) const { return nurses_vect[n].skill_level; }
  bool IsNurseWorkingInShift(int n, int s) const { return nurses_vect[n].is_working_in_shift[s]; }
  int NurseWorkingShifts(int n) const { return nurses_vect[n].working_shifts.size(); }
  int NurseWorkingShift(int n, int i) const { return nurses_vect[n].working_shifts[i]; }
  int AvailableNurses(int s) const { return available_nurses[s].size(); }
  int AvailableNurse(int s, int i) const { return available_nurses[s][i]; }
  int NurseMaxLoad(int n, int s) const { return nurses_vect[n].max_loads[s]; }

//Occupants
  string OccupantId(int p) const { return occupants_vect[p].id; }
  Gender OccupantGender(int p) const { return occupants_vect[p].gender; }
  int OccupantAgeGroup(int p) const { return occupants_vect[p].age_group; }
  int OccupantLengthOfStay(int p) const { return occupants_vect[p].length_of_stay; }
  int OccupantRoom(int p) const {return occupants_vect[p].assigned_room; }
  int OccupantSkillLevelRequired (int p, int s) const  { return occupants_vect[p].skill_level_required[s]; }
  int OccupantWorkloadProduced (int p, int s) const { return occupants_vect[p].workload_produced[s]; }
  int OccupantsPresent(int r,int d) const {return room_day_fixed_list[r][d].size();}
  int OccupantPresence(int r,int d,int i) const{return room_day_fixed_list[r][d][i];}

// Patients
  string PatientId(int p) const { return patients_vect[p].id; }
  Gender PatientGender(int p) const { return patients_vect[p].gender; }
  int PatientSurgeryReleaseDay(int p) const { return patients_vect[p].surgery_release_day; }
  int PatientAgeGroup(int p) const { return patients_vect[p].age_group; }
  int PatientLengthOfStay(int p) const { return patients_vect[p].length_of_stay; }
  int PatientSurgeryDueDay(int p) const { return patients_vect[p].surgery_due_day; }
  int PatientLastPossibleDay(int p) const { return PatientMandatory(p) ? patients_vect[p].surgery_due_day : days-1; }
  int PatientSurgeryDuration(int p) const { return patients_vect[p].surgery_duration; }
  int PatientSurgeon(int p) const { return patients_vect[p].surgeon; }
  bool PatientMandatory(int p) const { return patients_vect[p].mandatory; }
  bool IncompatibleRoom(int p, int r) const { return patients_vect[p].incompatible_rooms[r]; }
  int PatientSkillLevelRequired(int p, int s) const  { return patients_vect[p].skill_level_required[s]; }
  int PatientWorkloadProduced(int p, int s) const { return patients_vect[p].workload_produced[s]; }

// Rooms
  string RoomId(int r) const { return rooms_vect[r].id; }
  int RoomCapacity(int r) const { return rooms_vect[r].capacity; }

// Operating theaters and surgeons
  string OperatingTheaterId(int t) const { return operating_theaters_vect[t].id; }
  string SurgeonId(int u) const { return surgeons_vect[u].id; }
  int OperatingTheaterAvailability(int t, int d) const { return operating_theaters_vect[t].availability[d]; }
  int SurgeonMaxSurgeryTime(int s, int d) const { return surgeons_vect[s].max_surgery_time[d]; }
  string ShiftName(int s) const { return shift_names[s]; }
  string ShiftDescription(int s) const;  
  int Weight(int c) const { return weights[c]; }

  int FindAgeGroup(string age_group_name) const;
  int FindSurgeon(string surgeon_id) const;
  int FindShift(string shift_name) const;
  int FindRoom(string room_name) const;
  int FindOperatingTheater(string ot_name) const;
  int FindPatient(string patient_id) const;
  int FindNurse(string nurse_id) const;

 private:
  int days;
  int shifts_per_day;
  int shifts;
  int patients;
  int occupants;
  int skill_levels;
  int age_groups;
  int surgeons;
  int operating_theaters;
  int rooms;
  int nurses;
  const int SOFT_COST_COMPONENTS = 8;
  
  vector<Patient> patients_vect;
  vector<Occupant> occupants_vect;
  vector<Surgeon> surgeons_vect;
  vector<OperatingTheater> operating_theaters_vect;
  vector<Room> rooms_vect;
  vector<Nurse> nurses_vect; 
  vector<string> shift_names, age_group_names;
  vector<vector<vector<int>>> room_day_fixed_list;
  vector<vector<int>> available_nurses;  // list of available nurses per shift
  vector<int> weights;

  void ResizeDataStructures();
};

class IHTP_Output
{
 public:
  IHTP_Output(const IHTP_Input& my_in, bool verbose) : in(my_in), VERBOSE(verbose) {};
  IHTP_Output(const IHTP_Input& my_in, string file_name, bool verbose);
  void ReadJSON(string file_name);
  void AssignPatient(int p, int d, int r, int t);
  void AssignNurse(int n, int r, int s);
  void UpdatewithOccupantsInfo();
  void Reset();

  // Getters
  bool ScheduledPatient(int p) const { return admission_day[p] != -1; }
  int AdmissionDay(int p) const { return admission_day[p]; }
  int SurgeonDayLoad(int s, int d) const { return surgeon_day_load[s][d]; }
  int OperatingTheaterDayLoad(int s, int d) const { return operatingtheater_day_load[s][d]; }
  int RoomDayBPatients(int r, int d) const { return room_day_b_patients[r][d]; }
  int RoomDayAPatients(int r, int d) const { return room_day_a_patients[r][d]; }
  int RoomDayLoad(int r, int d) const { return room_day_patient_list[r][d].size(); }
  int RoomDayPatient(int r, int d, int i) const { return room_day_patient_list[r][d][i]; }
  int NurseShiftLoad(int n, int s) const { return nurse_shift_load[n][s]; }
  int SurgeonDayTheaterCount(int s, int d, int t) const { return surgeon_day_theater_count[s][d][t]; }

  double getObjValue() const { return total_cost; };
  double getInfValue() const { return total_violations; };
  void setInfValue(const double inf) { total_violations = inf; };
  void setObjValue(const double obj) { total_cost = obj; };

  // Costs and Constraints
  // SCP (surgery case planning)
  int OperatingTheaterOvertime() const;  
  int SurgeonOvertime() const;  
  int MandatoryUnscheduledPatients() const;  
  int ElectiveUnscheduledPatients() const; 
  int PatientDelay() const;
  int OpenOperatingTheater() const;
  int SurgeonTransfer() const;
  int AdmissionDay() const;
  // PAS (patient admission scheduling) 
  int RoomCapacity() const; 
  int RoomGenderMix() const; 
  int PatientRoomCompatibility() const; 
  int RoomAgeMix() const; 
  // NRA (nurse to room assignment)
  int RoomSkillLevel() const; 
  int NursePresence() const; 
  int UncoveredRoom() const; 
  int ExcessiveNurseWorkload() const;
  int ContinuityOfCare() const;

  int CountDistinctNurses(int p) const;
  int CountOccupantNurses(int o) const;

  void PrintCosts();

protected:
  const IHTP_Input& in;
  const bool VERBOSE;

  // patient data
  vector<int> admission_day;  // -1 for patients postponed
  vector<int> room;
  vector<int> operating_room;

  // patient data (redundant)
  vector<vector<int>> patient_shift_nurse; // nurse assigned to the patient in the shift
  // room data (redundant)
  vector<vector<vector<int>>> room_day_patient_list;
  vector<vector<int>> room_day_b_patients, room_day_a_patients;

  // nurse data
  vector<vector<int>> room_shift_nurse; // nurse assigned to the room in the sifht
  vector<vector<vector<int>>> nurse_shift_room_list; // list of rooms assigned to the nurse in the shift 
  vector<vector<int>> nurse_shift_load;
  
  // operating theaters and surgeons (redundant)
  vector<vector<vector<int>>> operatingtheater_day_patient_list;
  vector<vector<int>> operatingtheater_day_load, surgeon_day_load;
  vector<vector<vector<int>>> surgeon_day_theater_count; // number of operations per surgeon per day per theater
							 //
  int total_violations = - 1, total_cost = -1;
};

#endif
