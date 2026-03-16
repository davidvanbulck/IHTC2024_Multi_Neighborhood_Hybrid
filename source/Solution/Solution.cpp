#include "Solution.hpp"
#include "IHTP_Validator.h"

void Solution::loadSolution(const Solution &solution) {
  this->Reset();
  this->setInfValue(solution.getInfValue());
  this->setObjValue(solution.getObjValue());

  for (int p = 0; p < in.Patients(); p++) {
    int d = solution.AdmissionDay(p);
    if (d >= 0) {
      int r = solution.PatientRoom(p);
      int t = solution.PatientOperatingTheater(p);
      this->AssignPatient(p, d, r, t);
    }
  }
  for (int r = 0; r < in.Rooms(); r++) {
    for (int s = 0; s < in.Shifts(); s++) {
      int n = solution.getRoomShiftNurse(r, s);
      if (n != -1) {
        this->AssignNurse(n, r, s);
      }
    }
  }
}

int Solution::costEvaluationOTDay(int d) {
  int surgeon_cost = 0, count;
  int s, t;
  for (s = 0; s < in.Surgeons(); s++) {
    count = 0;
    for (t = 0; t < in.OperatingTheaters(); t++)
      if (surgeon_day_theater_count[s][d][t] > 0)
        count++;
    if (count > 1) {
      surgeon_cost += in.Weight(5) * (count - 1);
      if (VERBOSE)
        cout << "Surgeon " << in.SurgeonId(s) << " operates in " << count
             << " distinct operating theaters" << endl;
    }
  }
  int OT_cost = 0;
  for (int t = 0; t < in.OperatingTheaters(); t++) {
    if (operatingtheater_day_load[t][d] > 0) {
      OT_cost++;
    }
  }
  OT_cost *= in.Weight(4);

  return surgeon_cost + OT_cost;
}

void Solution::to_json(string filename) const {
  json patients, nurses;

  for (int p = 0; p < in.Patients(); p++) {
    if (admission_day.at(p) < 0) {
      //! this means patient is not admitted
      patients.push_back(
          json{{"id", in.PatientId(p)}, {"admission_day", "none"}});
    } else {
      patients.push_back(json{
          {"id", in.PatientId(p)},
          {"admission_day", admission_day.at(p)},
          {"room", in.RoomId(room.at(p))},
          {"operating_theater", in.OperatingTheaterId(operating_room.at(p))},
      });
    }
  }
  for (int n = 0; n < in.Nurses(); n++) {
    json shift_assignment;
    // in.room
    // in.NurseWorkingShifts()

    for (int sidx = 0; sidx < in.NurseWorkingShifts(n); sidx++) {
      //! go over all rooms assigned to nurse in this shift
      vector<string> shift_room;
      int s = in.NurseWorkingShift(n, sidx);

      for (int r : this->nurse_shift_room_list.at(n).at(s)) {
        shift_room.push_back(in.RoomId(r));
      }

      shift_assignment.push_back(json{
          {"day", s / in.ShiftsPerDay()},
          {"shift", in.ShiftName(s % in.ShiftsPerDay())},
          {"rooms", shift_room},
      });
    }

    nurses.push_back(
        json{{"id", in.NurseId(n)}, {"assignments", shift_assignment}});
  }

  // admission_day[p] = ad;
  // room[p] = r;
  // operating_room[p] = t;
  json j = {
      {"patients", patients},
      {"nurses", nurses},
  };

  std::ofstream o(filename.c_str());
  o << j;
  o.close();
}

void Solution::UnassignPatient(int p) {
  int d, ad, r, s, n, s1, t, u;

  ad = admission_day.at(p);

  if (ad < 0) {
    std::cout << "Error: you want to unassign a patient that is not assigned."
              << std::endl;
    assert(ad >= 0);
  } else {
    r = room.at(p);

    for (d = ad; d < min(in.Days(), ad + in.PatientLengthOfStay(p)); d++) {
      // Remove patient from day
      auto index = std::find(room_day_patient_list[r][d].begin(),
                             room_day_patient_list[r][d].end(), p);
      room_day_patient_list[r][d].erase(index);
      if (in.PatientGender(p) == Gender::A)
        room_day_a_patients[r][d]--;
      else
        room_day_b_patients[r][d]--;
      for (s = d * in.ShiftsPerDay(); s < (d + 1) * in.ShiftsPerDay(); s++) {
        // Remove patient from nurse's workload
        n = room_shift_nurse[r][s];
        if (n !=
            -1) { // reminder that patient data is relative to the admission,
                  // not absolute (s1 is s shifted to the admission of p)
          s1 = s - ad * in.ShiftsPerDay();
          patient_shift_nurse[p][s1] = -1;
          nurse_shift_load[n][s] -= in.PatientWorkloadProduced(p, s1);
        }
      }
    }
    // Remove patient from operating theather and surgeon workload
    t = operating_room.at(p);
    auto index = std::find(operatingtheater_day_patient_list[t][ad].begin(),
                           operatingtheater_day_patient_list[t][ad].end(), p);
    operatingtheater_day_patient_list[t][ad].erase(index);
    operatingtheater_day_load[t][ad] -= in.PatientSurgeryDuration(p);
    u = in.PatientSurgeon(p);
    surgeon_day_load[u][ad] -= in.PatientSurgeryDuration(p);
    surgeon_day_theater_count[u][ad][t]--;

    admission_day[p] = -1;
    room[p] = -1;
    operating_room[p] = -1;
  }
}

void Solution::UnassignPatientOperatingTheater(int p) {
  int t, ad, u;

  t = operating_room.at(p);
  ad = admission_day.at(p);
  auto index = std::find(operatingtheater_day_patient_list[t][ad].begin(),
                         operatingtheater_day_patient_list[t][ad].end(), p);
  operatingtheater_day_patient_list[t][ad].erase(index);
  operatingtheater_day_load[t][ad] -= in.PatientSurgeryDuration(p);
  u = in.PatientSurgeon(p);
  surgeon_day_load[u][ad] -= in.PatientSurgeryDuration(p);
  surgeon_day_theater_count[u][ad][t]--;
  operating_room[p] = -1;
}

void Solution::AssignPatientOperatingTheater(int p, int t) {
  int ad = admission_day.at(p);
  operating_room[p] = t;
  operatingtheater_day_patient_list[t][ad].push_back(p);
  operatingtheater_day_load[t][ad] += in.PatientSurgeryDuration(p);
  int u = in.PatientSurgeon(p);
  surgeon_day_load[u][ad] += in.PatientSurgeryDuration(p);
  surgeon_day_theater_count[u][ad][t]++;
}

void Solution::UnassignNurse(int n, int r, int s) {
  int d, s1;
  assert(room_shift_nurse[r][s] == n);
  d = s / in.ShiftsPerDay();

  for (int i = 0; i < room_day_patient_list[r][d].size();
       i++) { // reminder that patient_shift_nurse is relative to the admission,
              // not absolute (s1 is s shifted to the admission)
    int p = room_day_patient_list[r][d][i];
    if (p < in.Patients()) {
      s1 = s - admission_day[p] * in.ShiftsPerDay();
      nurse_shift_load[n][s] -= in.PatientWorkloadProduced(p, s1);
    } else {
      s1 = s;
      nurse_shift_load[n][s] -=
          in.OccupantWorkloadProduced(p - in.Patients(), s);
    }
    patient_shift_nurse[p][s1] = -1;
  }
  room_shift_nurse[r][s] = -1;
  auto index = std::find(nurse_shift_room_list[n][s].begin(),
                         nurse_shift_room_list[n][s].end(), r);
  nurse_shift_room_list[n][s].erase(index);
}

int Solution::RoomShiftSkillLevel(int r, int s) {
  int d, max, p, s1;

  d = s / in.ShiftsPerDay();
  max = 0;
  for (int i = 0; i < room_day_patient_list[r][d].size(); i++) {
    p = room_day_patient_list[r][d][i];
    if (p < in.Patients()) {
      s1 = s - admission_day[p] *
                   in.ShiftsPerDay(); // translation of the shift w.r.t. the
                                      // admission of the patient
      if (in.PatientSkillLevelRequired(p, s1) > max) {
        max = in.PatientSkillLevelRequired(p, s1);
      }
    } else if (in.OccupantSkillLevelRequired(p - in.Patients(), s) > max) {
      max = in.OccupantSkillLevelRequired(p - in.Patients(), s);
    }
  }
  return max;
}

void Solution::ComputeBetas(vector<int>& RoomsId, vector<int>&ShiftsId, vector<vector<bool>> &beta_in, vector<vector<bool>> &beta_jn) {

  // beta_in = 1 if patient i sees nurse n in shift not in ShifsId
  std::unordered_set<int> RoomsIdSet(RoomsId.begin(), RoomsId.end());
  for (int p = 0; p < in.Patients(); ++p) {
    if (AdmissionDay(p) == -1 || !RoomsIdSet.count(PatientRoom(p)))
      continue;
    int firstShift = AdmissionDay(p) * in.ShiftsPerDay();
    int lastShift =
        std::min(firstShift + in.PatientLengthOfStay(p) * in.ShiftsPerDay() - 1,
                in.Shifts() - 1);
    for (int s = firstShift; s <= lastShift; ++s) {
      if (s < ShiftsId.front() || s > ShiftsId.back()) {
        assert(PatientRoom(p) != -1);
        assert(getRoomShiftNurse(PatientRoom(p), s) != -1);
        beta_in[p][getRoomShiftNurse(PatientRoom(p), s)] = 1;
      }
    }
  }

  // beta_jn = 1 if occupant i sees nurse n in shift not in ShifsId
  for (int p = 0; p < in.Occupants(); ++p) {
    if (!RoomsIdSet.count(in.OccupantRoom(p)))
      continue;
    int firstShift = 0;
    int lastShift = std::min(
        firstShift + in.OccupantLengthOfStay(p) * in.ShiftsPerDay() - 1,
        in.Shifts() - 1);
    for (int s = firstShift; s <= lastShift; ++s) {
      if (s < ShiftsId.front() || s > ShiftsId.back()) {
        beta_jn[p][getRoomShiftNurse(in.OccupantRoom(p), s)] = 1;
      }
    }
  }
}

int Solution::CostEvaluationRemovingNurseRoomsShifts(
    vector<int> &RoomsId, vector<int> &ShiftsId, vector<vector<bool>> &beta_in,
    vector<vector<bool>> &beta_jn, vector<vector<int>> &CapacitiesNurses) {
  int costExcessiveWorkload = 0;
  int costRoomSkillLevel = 0;
  int costContinuityOfCare = 0;

  // beta_in = 1 if patient i sees nurse n in shift not in ShifsId
  std::unordered_set<int> RoomsIdSet(RoomsId.begin(), RoomsId.end());
  for (int p = 0; p < in.Patients(); ++p) {
    if (AdmissionDay(p) == -1 || !RoomsIdSet.count(PatientRoom(p)))
      continue;
    int firstShift = AdmissionDay(p) * in.ShiftsPerDay();
    int lastShift =
        std::min(firstShift + in.PatientLengthOfStay(p) * in.ShiftsPerDay() - 1,
                 in.Shifts() - 1);
    for (int s = firstShift; s <= lastShift; ++s) {
      if (s < ShiftsId.front() || s > ShiftsId.back()) {
        assert(PatientRoom(p) != -1);
        assert(getRoomShiftNurse(PatientRoom(p), s) != -1);
        beta_in[p][getRoomShiftNurse(PatientRoom(p), s)] = 1;
      }
    }
  }

  // beta_jn = 1 if occupant i sees nurse n in shift not in ShifsId
  for (int p = 0; p < in.Occupants(); ++p) {
    if (!RoomsIdSet.count(in.OccupantRoom(p)))
      continue;
    int firstShift = 0;
    int lastShift = std::min(
        firstShift + in.OccupantLengthOfStay(p) * in.ShiftsPerDay() - 1,
        in.Shifts() - 1);
    for (int s = firstShift; s <= lastShift; ++s) {
      if (s < ShiftsId.front() || s > ShiftsId.back()) {
        beta_jn[p][getRoomShiftNurse(in.OccupantRoom(p), s)] = 1;
      }
    }
  }

  std::vector<bool> patientFlag(in.Patients() + in.Occupants(), 0);
  for (int n = 0; n < in.Nurses(); n++) {
    patientFlag.assign(patientFlag.size(), false);
    for (int s_ = 0; s_ < ShiftsId.size(); s_++) {
      int s = ShiftsId.at(s_);
      int d = s / in.ShiftsPerDay();
      int workload_ns =
          0; // work done of nurse n in shift s in the subset of rooms
      for (auto &r : RoomsId) {
        if (n != room_shift_nurse[r][s]) {
          continue;
        }
        for (int j = 0; j < room_day_patient_list.at(r).at(d).size(); j++) {
          int p = room_day_patient_list.at(r).at(d)[j];
          if (p < in.Patients()) {
            int s1 = s - admission_day.at(p) * in.ShiftsPerDay();

            // ExcessiveNurseWorkload
            workload_ns += in.PatientWorkloadProduced(p, s1);

            // RoomSkillLevel
            if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
              costRoomSkillLevel -=
                  (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n));
            }

            // ContinuityOfCare
            // int s2 = 0;
            // while (s2 < min((int)patient_shift_nurse.at(p).size(), (in.Days()
            // - admission_day.at(p)) * in.ShiftsPerDay()) &&
            // !beta_in.at(p).at(n))
            //{
            //    int s2_global_shift = s2 + admission_day.at(p) *
            //    in.ShiftsPerDay(); // d if (s2_global_shift < ShiftsId.front()
            //    || s2_global_shift > ShiftsId.back()) // d
            //    {
            //	std::cout << "For patient " << p << " check " << s2_global_shift
            //<< std::endl;
            //        int n2 = patient_shift_nurse.at(p)[s2];
            //        if (n2 == n) {
            //	    beta_in[p][n] = true;
            //            //break;
            //        }
            //    } else {
            //	    std::cout << "For patient " << p << " skip " <<
            // s2_global_shift << std::endl;
            //    }
            //    s2++;
            //}
            if (!beta_in[p][n] && !patientFlag[p]) {
              costContinuityOfCare--;
              patientFlag[p] = true;
            }
          } else {
            int j_ = p - in.Patients();
            workload_ns += in.OccupantWorkloadProduced(j_, s);
            if (in.OccupantSkillLevelRequired(j_, s) > in.NurseSkillLevel(n)) {
              costRoomSkillLevel -= (in.OccupantSkillLevelRequired(j_, s) -
                                     in.NurseSkillLevel(n));
            }

            // ContinuityOfCare
            // int s2 = 0;
            // while (s2 < in.OccupantLengthOfStay(j_) * in.ShiftsPerDay() &&
            // !beta_jn.at(j_).at(n))
            //{
            //    if (s2 < ShiftsId.front() || s2 > ShiftsId.back()) // d
            //    {
            //        int n2 = patient_shift_nurse.at(p)[s2];
            //        if (n2 == n) {
            //            beta_jn.at(j_).at(n) = true;
            //            break;
            //        }
            //    }
            //    s2++;
            //}
            if (!beta_jn.at(j_).at(n) && !patientFlag[p]) {
              costContinuityOfCare--;
              patientFlag[p] = true;
            }
          }
        }
      }
      assert(nurse_shift_load[n][s] - workload_ns >= 0);
      if (nurse_shift_load[n][s] > in.NurseMaxLoad(n, s)) {
        if (nurse_shift_load[n][s] - workload_ns > in.NurseMaxLoad(n, s)) {
          CapacitiesNurses.at(n).at(s_) = 0;
          costExcessiveWorkload -= workload_ns;
        } else {
          CapacitiesNurses.at(n).at(s_) =
              in.NurseMaxLoad(n, s) - (nurse_shift_load[n][s] - workload_ns);
          costExcessiveWorkload -=
              (nurse_shift_load[n][s] - in.NurseMaxLoad(n, s));
        }
      } else {
        CapacitiesNurses.at(n).at(s_) =
            in.NurseMaxLoad(n, s) - (nurse_shift_load[n][s] - workload_ns);
      }
    }
  }

#ifndef NDEBUG
  if (costRoomSkillLevel * in.Weight(1) + costContinuityOfCare * in.Weight(2) +
          costExcessiveWorkload * in.Weight(3) <
      0) {
    std::cout << "Old cost Room Skill Level: "
              << costRoomSkillLevel * in.Weight(1) << std::endl;
    std::cout << "Old cost Continuity Of Care: "
              << costContinuityOfCare * in.Weight(2) << std::endl;
    std::cout << "Old cost Excessive Nurse Workload: "
              << costExcessiveWorkload * in.Weight(3) << std::endl;
  }
#endif

  return costRoomSkillLevel * in.Weight(1) +
         costContinuityOfCare * in.Weight(2) +
         costExcessiveWorkload * in.Weight(3);
}

int Solution::CostEvaluationAddingNurse(bool verbose, int n, int r, int s) {
  int costExcessiveWorkload = 0;
  int costRoomSkillLevel = 0;
  int costContinuityOfCare = 0;
#ifndef NDEBUG
  int checkCOCcost = 0;
#endif

  if (!in.IsNurseWorkingInShift(n, s)) {
    std::cout << "Error! You want to assign a nurse to an invalid shift."
              << std::endl;
    return 100;
  }

  int d = s / in.ShiftsPerDay();
  int load = 0;
  for (int j = 0; j < room_day_patient_list.at(r).at(d).size(); j++) {
    int p = room_day_patient_list.at(r).at(d)[j];
    if (p < in.Patients()) {
      int s1 = s - admission_day.at(p) * in.ShiftsPerDay();

      // ExcessiveNurseWorkload
      load += in.PatientWorkloadProduced(p, s1);

      // RoomSkillLevel
      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n);
      }

      // ContinuityOfCare
      int limit = patient_shift_nurse.at(p).size();
      if (p < in.Patients()) { limit = min((int)patient_shift_nurse.at(p).size(), (in.Days() - admission_day[p]) * in.ShiftsPerDay()); }
      if (std::find(patient_shift_nurse.at(p).begin(), patient_shift_nurse.at(p).begin() + limit, n) == patient_shift_nurse.at(p).begin() + limit)
      {
        costContinuityOfCare++;
      }
#ifndef NDEBUG
      
      int s2 = 0;
      bool new_nurse_already = false;

      while (s2 < min((int)patient_shift_nurse.at(p).size(),
          (in.Days() - admission_day.at(p)) * in.ShiftsPerDay()) &&
          !new_nurse_already) {
          if (s2 != s1) {
              int n2 = patient_shift_nurse.at(p)[s2];
              if (n2 == n) {
                  new_nurse_already = true;
              }
          }
          s2++;
      }
      if (!new_nurse_already) {
          checkCOCcost++;
      }
#endif


    } else {
      load += in.OccupantWorkloadProduced(p - in.Patients(), s);
      if (in.OccupantSkillLevelRequired(p - in.Patients(), s) >
          in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            in.OccupantSkillLevelRequired(p - in.Patients(), s) -
            in.NurseSkillLevel(n);
      }
      // ContinuityOfCare
      int limit = patient_shift_nurse.at(p).size();
      if (p < in.Patients()) { limit = min((int)patient_shift_nurse.at(p).size(), (in.Days() - admission_day[p]) * in.ShiftsPerDay()); }
      if (std::find(patient_shift_nurse.at(p).begin(), patient_shift_nurse.at(p).begin() + limit, n) == patient_shift_nurse.at(p).begin() + limit)
      {
          costContinuityOfCare++;
      }
#ifndef NDEBUG
      int s2 = 0;
      bool new_nurse_already = false;
      while (s2 < in.OccupantLengthOfStay(p - in.Patients()) *
          in.ShiftsPerDay() &&
          !new_nurse_already) {
          if (s2 != s) {
              int n2 = patient_shift_nurse.at(p)[s2];
              if (n2 == n) {
                  new_nurse_already = true;
              }
          }
          s2++;
      }
      if (!new_nurse_already) {
          checkCOCcost++;
      }
#endif
    }
  }
  if (nurse_shift_load[n][s] + load > in.NurseMaxLoad(n, s)) {
    if (nurse_shift_load[n][s] >= in.NurseMaxLoad(n, s)) {
      costExcessiveWorkload += load;
    } else {
      costExcessiveWorkload +=
          (load - (in.NurseMaxLoad(n, s) - nurse_shift_load[n][s]));
    }
  }
  if (verbose) {
    std::cout << "COST ADDING NURSE " << n << " TO ROOM " << r << " ON SHIFT "
              << s << std::endl;
    std::cout << "cost Room Skill Level: " << costRoomSkillLevel << " * "
              << in.Weight(1) << " = " << costRoomSkillLevel * in.Weight(1)
              << std::endl;
    std::cout << "cost Continuity Of Care: " << costContinuityOfCare << " * "
              << in.Weight(2) << " = " << costContinuityOfCare * in.Weight(2)
              << std::endl;
    std::cout << "cost Excessive Nurse Workload: " << costExcessiveWorkload
              << " * " << in.Weight(3) << " = "
              << costExcessiveWorkload * in.Weight(3) << std::endl;
    std::cout << "Total cost: "
              << costRoomSkillLevel * in.Weight(1) +
                     costContinuityOfCare * in.Weight(2) +
                     costExcessiveWorkload * in.Weight(3)
              << std::endl;
  }
#ifndef NDEBUG
  assert(checkCOCcost == costContinuityOfCare);
#endif

  return costRoomSkillLevel * in.Weight(1) +
         costContinuityOfCare * in.Weight(2) +
         costExcessiveWorkload * in.Weight(3);
}

int Solution::CostAddingNurseNoCOC(bool verbose, int n, int r, int s)
{
    int costExcessiveWorkload = 0;
    int costRoomSkillLevel = 0;

    if (!in.IsNurseWorkingInShift(n, s)) {
        std::cout << "Error! You want to assign a nurse to an invalid shift."
            << std::endl;
        return 1000 * in.Weight(7);
    }

    int d = s / in.ShiftsPerDay();
    int load = 0;
    for (int j = 0; j < room_day_patient_list.at(r).at(d).size(); j++) {
        int p = room_day_patient_list.at(r).at(d)[j];
        if (p < in.Patients()) {
            int s1 = s - admission_day.at(p) * in.ShiftsPerDay();

            // ExcessiveNurseWorkload
            load += in.PatientWorkloadProduced(p, s1);

            // RoomSkillLevel
            if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
                costRoomSkillLevel +=
                    in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n);
            }
        }
        else {
            load += in.OccupantWorkloadProduced(p - in.Patients(), s);
            if (in.OccupantSkillLevelRequired(p - in.Patients(), s) >
                in.NurseSkillLevel(n)) {
                costRoomSkillLevel +=
                    in.OccupantSkillLevelRequired(p - in.Patients(), s) -
                    in.NurseSkillLevel(n);
            }
        }
    }
    if (nurse_shift_load[n][s] + load > in.NurseMaxLoad(n, s)) {
        if (nurse_shift_load[n][s] >= in.NurseMaxLoad(n, s)) {
            costExcessiveWorkload += load;
        }
        else {
            costExcessiveWorkload +=
                (load - (in.NurseMaxLoad(n, s) - nurse_shift_load[n][s]));
        }
    }
    if (verbose) {
        std::cout << "COST ADDING NURSE " << n << " TO ROOM " << r << " ON SHIFT "
            << s << std::endl;
        std::cout << "cost Room Skill Level: " << costRoomSkillLevel << " * "
            << in.Weight(1) << " = " << costRoomSkillLevel * in.Weight(1)
            << std::endl;
        std::cout << "cost Excessive Nurse Workload: " << costExcessiveWorkload
            << " * " << in.Weight(3) << " = "
            << costExcessiveWorkload * in.Weight(3) << std::endl;
        std::cout << "Total cost: "
            << costRoomSkillLevel * in.Weight(1) +
            costExcessiveWorkload * in.Weight(3)
            << std::endl;
    }
    return costRoomSkillLevel * in.Weight(1) +
        costExcessiveWorkload * in.Weight(3);
}

int Solution::CostEvaluationRemovingNurse(bool verbose, int n, int r, int s) {
  int costExcessiveWorkload = 0;
  int costRoomSkillLevel = 0;
  int costContinuityOfCare = 0;

#ifndef NDEBUG
  int checkCostCOC = 0;
#endif

  if (n == -1) {
    return 0;
  }
  assert(room_shift_nurse[r][s] == n);

  int d = s / in.ShiftsPerDay();
  int load = 0;
  for (int j = 0; j < room_day_patient_list.at(r).at(d).size(); j++) {
    int p = room_day_patient_list.at(r).at(d)[j];
    if (p < in.Patients()) {
        int s1 = s - admission_day.at(p) * in.ShiftsPerDay();

        // ExcessiveNurseWorkload
        load += in.PatientWorkloadProduced(p, s1);

        // RoomSkillLevel
        if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
            costRoomSkillLevel -=
                (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n));
        }

        // ContinuityOfCare
        int limit = min((int)patient_shift_nurse.at(p).size(), (in.Days() - admission_day[p]) * in.ShiftsPerDay());;
        if (std::count(patient_shift_nurse.at(p).begin(), patient_shift_nurse.at(p).begin() + limit, n) == 1)
        {
            costContinuityOfCare--;          
        }
#ifndef NDEBUG
        int s2 = 0;
        bool current_nurse_already = false;

        while (s2 < min((int)patient_shift_nurse.at(p).size(),
            (in.Days() - admission_day.at(p)) * in.ShiftsPerDay()) &&
            !current_nurse_already) {
            if (s2 != s1) {
                int n2 = patient_shift_nurse.at(p)[s2];
                if (n2 == n) {
                    current_nurse_already = true;
                }
            }
            s2++;
        }
        if (!current_nurse_already) {
            checkCostCOC--;
        }
#endif
    
    }
    else {
      load += in.OccupantWorkloadProduced(p - in.Patients(), s);
      if (in.OccupantSkillLevelRequired(p - in.Patients(), s) >
          in.NurseSkillLevel(n)) {
          costRoomSkillLevel -=
              (in.OccupantSkillLevelRequired(p - in.Patients(), s) -
                  in.NurseSkillLevel(n));
      }

      // ContinuityOfCare
      int limit = patient_shift_nurse.at(p).size();
      if (std::count(patient_shift_nurse.at(p).begin(), patient_shift_nurse.at(p).begin() + limit, n) == 1)
      {
          costContinuityOfCare--;
      }
#ifndef NDEBUG
      int s2 = 0;
      bool current_nurse_already = false;
      while (s2 < in.OccupantLengthOfStay(p - in.Patients()) *
          in.ShiftsPerDay() &&
          !current_nurse_already) {
          if (s2 != s) {
              int n2 = patient_shift_nurse.at(p)[s2];
              if (n2 == n) {
                  current_nurse_already = true;
              }
          }
          s2++;
      }
      if (!current_nurse_already)
          checkCostCOC--;
#endif
    }
  }
  if (nurse_shift_load[n][s] > in.NurseMaxLoad(n, s)) {
    if (nurse_shift_load[n][s] - load > in.NurseMaxLoad(n, s)) {
      costExcessiveWorkload -= load;
    } else {
      costExcessiveWorkload -= (nurse_shift_load[n][s] - in.NurseMaxLoad(n, s));
    }
  }
  if (verbose) {
    std::cout << "COST REMOVING NURSE " << n << " FROM ROOM " << r
              << " ON SHIFT " << s << std::endl;
    std::cout << "cost Room Skill Level: " << costRoomSkillLevel << " * "
              << in.Weight(1) << " = " << costRoomSkillLevel * in.Weight(1)
              << std::endl;
    std::cout << "cost Continuity Of Care: " << costContinuityOfCare << " * "
              << in.Weight(2) << " = " << costContinuityOfCare * in.Weight(2)
              << std::endl;
    std::cout << "cost Excessive Nurse Workload: " << costExcessiveWorkload
              << " * " << in.Weight(3) << " = "
              << costExcessiveWorkload * in.Weight(3) << std::endl;
    std::cout << "Total cost: "
              << costRoomSkillLevel * in.Weight(1) +
                     costContinuityOfCare * in.Weight(2) +
                     costExcessiveWorkload * in.Weight(3)
              << std::endl;
  }

  assert(checkCostCOC == costContinuityOfCare);

  return costRoomSkillLevel * in.Weight(1) +
         costContinuityOfCare * in.Weight(2) +
         costExcessiveWorkload * in.Weight(3);
}

vector<int> Solution::determineBestOTAndCosts(int p, int d) {
  int totalBestCost = in.Weight(4) + in.Weight(5) + 1;
  int bestCostSurgeonTransfer = 2;
  int bestCostOpenOT = 2;
  int bestOT = -1;
  int s = in.PatientSurgeon(p);

  int t = 0;

  while (t < in.OperatingTheaters() && totalBestCost > 0) {
    if (operatingtheater_day_load[t][d] + in.PatientSurgeryDuration(p) <=
        in.OperatingTheaterAvailability(t,
                                        d)) // Otherwise this OT is not feasible
    {
      pair<int, int> costOT = costEvaluationAssignOT(p, d, t);
      int costOpenOT = costOT.first;
      int costSurgeonTransfer = costOT.second;

      if (costSurgeonTransfer * in.Weight(5) + costOpenOT * in.Weight(4) <
          totalBestCost) {
        totalBestCost =
            costSurgeonTransfer * in.Weight(5) + costOpenOT * in.Weight(4);
        bestCostSurgeonTransfer = costSurgeonTransfer;
        bestCostOpenOT = costOpenOT;
        bestOT = t;
      }
    }
    t++;
  }
  vector<int> total = {bestOT, bestCostOpenOT, bestCostSurgeonTransfer};
  return total;
}

int Solution::CostEvaluationAddingPatient(bool verbose, int p, int r, int d,
                                          int t) {
  /*
      If t = -1, this function finds the best possible OT
      Otherwise, it computes the cost for the given OT t
  */

  // Room Age Mix
  int costRoomAgeMix = 0;
  int ageP = in.PatientAgeGroup(p);
  for (int d2 = d; d2 < min(in.Days(), d + in.PatientLengthOfStay(p)); d2++) {
    int min = in.AgeGroups() + 1;
    int max = -1;
    int age;
    bool sameAgeFound = false;

    int i = 0;
    while (i < room_day_patient_list.at(r).at(d2).size() && !sameAgeFound) {
      int p2 = room_day_patient_list.at(r).at(d2).at(i);
      if (p2 < in.Patients()) {
        age = in.PatientAgeGroup(p2);
      } else {
        age = in.OccupantAgeGroup(p2 - in.Patients());
      }
      if (age == ageP) {
        sameAgeFound = true;
      }
      if (age < min) {
        min = age;
      }
      if (age > max) {
        max = age;
      }
      i++;
    }
    if (max > -1 &&
        !sameAgeFound) // Otherwise p is the first patient in this room, cost 0
    {
      if (ageP < min) {
        costRoomAgeMix += (min - ageP);
      } else if (ageP > max) {
        costRoomAgeMix += (ageP - max);
      }
    }
  }

  // Room Skill Level, Continuity of care, and Excessive Nurse Workload
  int costRoomSkillLevel = 0;
  int costCOC = 0;
  int costExcessiveNurseWorkload = 0;

  vector<bool> seenNurses(in.Nurses(), false);

  for (int s = d * in.ShiftsPerDay();
       s <
       min(in.Shifts(), (d + in.PatientLengthOfStay(p)) * in.ShiftsPerDay());
       s++) {
    int n = room_shift_nurse.at(r).at(s);
    if (n != -1) {
      int s1 = s - d * in.ShiftsPerDay();
      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel +=
            (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n));
      }
      if (!seenNurses[n]) {
        seenNurses[n] = true;
        costCOC++;
      }
      if (nurse_shift_load[n][s] + in.PatientWorkloadProduced(p, s1) >
          in.NurseMaxLoad(n, s)) {
        if (nurse_shift_load[n][s] >= in.NurseMaxLoad(n, s)) {
          costExcessiveNurseWorkload += in.PatientWorkloadProduced(p, s1);
        } else {
          costExcessiveNurseWorkload +=
              (nurse_shift_load[n][s] + in.PatientWorkloadProduced(p, s1) -
               in.NurseMaxLoad(n, s));
        }
      }
    }
  }

  // PatientDelay
  int costPatientDelay = d - in.PatientSurgeryReleaseDay(p);

  // Elective unscheduled patient
  int costElectiveUnscheduledPatient = (in.PatientMandatory(p) ? 0 : -1);

  // Surgeon transfer & Open OT
  int costOpenOT;
  int costSurgeonTransfer;

  if (t > -1) {
    pair<int, int> costOT = costEvaluationAssignOT(p, d, t);
    costOpenOT = costOT.first;
    costSurgeonTransfer = costOT.second;
  } else {
    vector<int> bestOT = determineBestOTAndCosts(p, d);
    costOpenOT = bestOT.at(1);
    costSurgeonTransfer = bestOT.at(2);
  }

  // Total cost
  vector<int> costs{costRoomAgeMix,   costRoomSkillLevel,
                    costCOC,          costExcessiveNurseWorkload,
                    costOpenOT,       costSurgeonTransfer,
                    costPatientDelay, costElectiveUnscheduledPatient};

  int objValue = 0;

  for (size_t i = 0; i < costs.size(); ++i) {
    objValue += costs[i] * in.Weight(i);

    if (verbose) {
      std::cout << std::setw(30) << std::left; // Left-align category names
      switch (i) {
      case 0:
        std::cout << "Age mix";
        break;
      case 1:
        std::cout << "Skill Level";
        break;
      case 2:
        std::cout << "Con care";
        break;
      case 3:
        std::cout << "Exc. nurse work";
        break;
      case 4:
        std::cout << "Open OT";
        break;
      case 5:
        std::cout << "Surgeon trans";
        break;
      case 6:
        std::cout << "Patient del";
        break;
      case 7:
        std::cout << "Umsched pat";
        break;
      default:
        std::cerr << "Error: Unexpected index " << i << std::endl;
        std::abort();
      }
      std::cout << " " << std::fixed << std::setprecision(2) << costs[i]
                << " * " << in.Weight(i) << " = " << costs[i] * in.Weight(i)
                << std::endl;
    }
  }
  if (verbose) {
    std::cout << "Total: " << objValue << std::endl;
  }

  return objValue;
}

int Solution::CostEvaluationRemovingPatient(bool verbose, int p) {
  int d = admission_day.at(p);
  int r = room.at(p);
  int t = operating_room.at(p);

  // Room Age Mix
  int costRoomAgeMix = 0;
  int ageP = in.PatientAgeGroup(p);
  for (int d2 = d; d2 < min(in.Days(), d + in.PatientLengthOfStay(p)); d2++) {
    int min = in.AgeGroups() + 1;
    int max = -1;
    int age;
    bool sameAgeFound = false;

    int i = 0;
    while (i < room_day_patient_list.at(r).at(d2).size() && !sameAgeFound) {
      int p2 = room_day_patient_list.at(r).at(d2).at(i);
      if (p2 != p) {
        if (p2 < in.Patients()) {
          age = in.PatientAgeGroup(p2);
        } else {
          age = in.OccupantAgeGroup(p2 - in.Patients());
        }
        if (age == ageP) {
          sameAgeFound = true;
        }
        if (age < min) {
          min = age;
        }
        if (age > max) {
          max = age;
        }
      }
      i++;
    }
    if (max > -1 &&
        !sameAgeFound) // Otherwise p was the only patient in this room, cost 0
    {
      if (ageP < min) {
        costRoomAgeMix -= (min - ageP);
      } else if (ageP > max) {
        costRoomAgeMix -= (ageP - max);
      }
    }
  }

  // Room Skill Level, Continuity of care, and Excessive Nurse Workload
  int costRoomSkillLevel = 0;
  int costCOC = 0;
  int costExcessiveNurseWorkload = 0;

  vector<bool> seenNurses(in.Nurses(), false);

  for (int s = d * in.ShiftsPerDay();
       s <
       min(in.Shifts(), (d + in.PatientLengthOfStay(p)) * in.ShiftsPerDay());
       s++) {
    int s1 = s - d * in.ShiftsPerDay();
    int n = room_shift_nurse.at(r).at(s);

    if (n != -1) {
      if (in.PatientSkillLevelRequired(p, s1) > in.NurseSkillLevel(n)) {
        costRoomSkillLevel -=
            (in.PatientSkillLevelRequired(p, s1) - in.NurseSkillLevel(n));
      }
      if (!seenNurses[n]) {
        seenNurses[n] = true;
        costCOC--;
      }
      if (nurse_shift_load[n][s] > in.NurseMaxLoad(n, s)) {
        if (nurse_shift_load[n][s] - in.PatientWorkloadProduced(p, s1) >
            in.NurseMaxLoad(n, s)) {
          costExcessiveNurseWorkload -= in.PatientWorkloadProduced(p, s1);
        } else {
          costExcessiveNurseWorkload -=
              (nurse_shift_load[n][s] - in.NurseMaxLoad(n, s));
        }
      }
    }
  }

  // PatientDelay
  int costPatientDelay = -1 * (d - in.PatientSurgeryReleaseDay(p));

  // Elective unscheduled patient
  int costElectiveUnscheduledPatient = (in.PatientMandatory(p) ? 0 : 1);

  // Surgeon transfer & open OT
  pair<int, int> costOT = costEvaluationRemoveOT(p);
  int costOpenOT = costOT.first;
  int costSurgeonTransfer = costOT.second;

  // Total cost
  vector<int> costs{costRoomAgeMix,   costRoomSkillLevel,
                    costCOC,          costExcessiveNurseWorkload,
                    costOpenOT,       costSurgeonTransfer,
                    costPatientDelay, costElectiveUnscheduledPatient};

  int objValue = 0;

  for (size_t i = 0; i < costs.size(); ++i) {
    objValue += costs[i] * in.Weight(i);

    if (verbose) {
      std::cout << std::setw(30) << std::left; // Left-align category names
      switch (i) {
      case 0:
        std::cout << "Age mix";
        break;
      case 1:
        std::cout << "Skill Level";
        break;
      case 2:
        std::cout << "Con care";
        break;
      case 3:
        std::cout << "Exc. nurse work";
        break;
      case 4:
        std::cout << "Open OT";
        break;
      case 5:
        std::cout << "Surgeon trans";
        break;
      case 6:
        std::cout << "Patient del";
        break;
      case 7:
        std::cout << "Umsched pat";
        break;
      default:
        std::cerr << "Error: Unexpected index " << i << std::endl;
        std::abort();
      }
      std::cout << " " << std::fixed << std::setprecision(2) << costs[i]
                << " * " << in.Weight(i) << " = " << costs[i] * in.Weight(i)
                << std::endl;
    }
  }
  if (verbose) {
    std::cout << "Total: " << objValue << std::endl;
  }

  return objValue;
}

pair<int, int> Solution::costEvaluationAssignOT(int p, int d, int t) {
  // --> this function assumes assigning p to t is feasible
  int costOpenOT = 0;
  int costSurgeonTransfer = 0;
  int s = in.PatientSurgeon(p);

  if (operatingtheater_day_load[t][d] == 0) {
    costOpenOT++;
  }

  if (surgeon_day_theater_count[s][d][t] == 0) // Otherwise no transfer
  {
    int t2 = 0;
    bool transfer = false;
    while (t2 < in.OperatingTheaters() && !transfer) {
      if (surgeon_day_theater_count[s][d][t2] > 0) {
        transfer = true;
      }
      t2++;
    }
    if (transfer) {
      costSurgeonTransfer++;
    }
  }

  return make_pair(costOpenOT, costSurgeonTransfer);
}

pair<int, int> Solution::costEvaluationRemoveOT(int p) {
  int costOpenOT = 0;
  int costSurgeonTransfer = 0;

  int s = in.PatientSurgeon(p);
  int ot = operating_room.at(p);
  int d = admission_day.at(p);

  // Open Operating Theater cost
  if (operatingtheater_day_load[ot][d] == in.PatientSurgeryDuration(p)) {
    costOpenOT--;
  }

  // Surgeon transfer cost
  if (surgeon_day_theater_count[s][d][ot] == 1) // Otherwise no transfer
  {
    int t2 = 0;
    bool transfer = false;
    while (t2 < in.OperatingTheaters() && !transfer) {
      if (t2 != ot && surgeon_day_theater_count[s][d][t2] > 0) {
        transfer = true;
      }
      t2++;
    }
    if (transfer) {
      costSurgeonTransfer--;
    }
  }

  return make_pair(costOpenOT, costSurgeonTransfer);
}
bool Solution::isPatientFeasibleForOT(int p, int t, int d) {
  return this->operatingtheater_day_load[t][d] + in.PatientSurgeryDuration(p) <=
         in.OperatingTheaterAvailability(t, d);
}

bool Solution::isPatientFeasibleForRoomDayPure(int p, int r, int d) {
  if (in.IncompatibleRoom(p, r)) {
    return false;
  }
  int los = in.PatientLengthOfStay(p);
  int room_cap = in.RoomCapacity(r);
  Gender gender = in.PatientGender(p);
  // loop over all days
  for (int d1 = d; d1 < d + los; d1++) {
    // check gender and cap
    if (RoomDayLoad(r, d1) == in.RoomCapacity(r)) {
      // no room left
      return false;
    }
    if (!this->isRoomGender(gender, r, d1)) {
      return false;
    }
  }
  return true;
}

int Solution::patientEndDay(int p) {
  if (!this->ScheduledPatient(p)) {
    return -1;
  }
  return this->admission_day.at(p) + in.PatientLengthOfStay(p);
}

bool Solution::isRoomGender(Gender gender, int r, int d) {
  switch (gender) {
  case Gender::A: {
    if (this->RoomDayBPatients(r, d) > 0) {
      return false;
    }
    break;
  }
  case Gender::B: {
    if (this->RoomDayAPatients(r, d) > 0) {
      return false;
    }
    break;
  }
  }
  return true;
}

bool Solution::isPatientSurgeryPossibleOnDay(int p, int d) {
  int dur = in.PatientSurgeryDuration(p);
  int s = in.PatientSurgeon(p);

  if (in.SurgeonMaxSurgeryTime(s, d) - this->SurgeonDayLoad(s, d) < dur) {
    return false;
  }
  //? ots have room av?
  for (int t = 0; t < in.OperatingTheaters(); t++) {
    if (operatingtheater_day_load.at(t).at(d) + dur <=
        in.OperatingTheaterAvailability(t, d)) {
      return true;
    }
  }
  return false;
}

bool Solution::isPatientFeasibleForRoomOnDayWithExcludedPatient(int p, int d,
                                                                int r, int p2) {
  int r2 = this->PatientRoom(p2);
  Gender gender2 = in.PatientGender(p2);
  int admission_day2 = this->AdmissionDay(p2);
  int los2 = in.PatientLengthOfStay(p2);
  int gender_remove = 0;
  if (r == -1) {
    r = this->PatientRoom(p);
  }

  // in debug mode throw an error if room incompatible (should not happen) as we
  // stay within rooms
  assert(r != -1);

  Gender gender = in.PatientGender(p);
  int room_cap = in.RoomCapacity(r);
  int load = this->RoomDayLoad(r, d);
  if (d >= admission_day2 || d < admission_day2 + los2) {
    load -= 1;

    if (gender2 != gender) {
      gender_remove += 1;
    }
  }
  if (load == 0) {
    return true;
  } else if (load == room_cap) {
    return false;
  }
  switch (gender) {
  case Gender::A: {
    if (this->RoomDayBPatients(r, d) - gender_remove > 0) {
      return false;
    }
    break;
  }
  case Gender::B: {
    if (this->RoomDayAPatients(r, d) - gender_remove > 0) {
      return false;
    }
    break;
  }
  }
  return true;
}
bool Solution::isPatientFeasibleForRoomOnDay(int p, int d, int r) {
  //if (r == -1) {
  //  r = this->PatientRoom(p);
  //}
  // in debug mode throw an error if room incompatible (should not happen) as we
  // stay within rooms
  assert(r != -1);
  assert(!in.IncompatibleRoom(p, r));
  assert(d >= in.PatientSurgeryReleaseDay(p) && d <= in.PatientLastPossibleDay(p));

  Gender gender = in.PatientGender(p);
  int room_cap = in.RoomCapacity(r);
  int load = this->RoomDayLoad(r, d);
  if (load == 0) {
    return true;
  } else if (load == room_cap) {
    return false;
  }

  switch (gender) {
  case Gender::A: {
    if (this->RoomDayBPatients(r, d) > 0) {
      return false;
    }
    break;
  }
  case Gender::B: {
    if (this->RoomDayAPatients(r, d) > 0) {
      return false;
    }
    break;
  }
  }
  return true;
}

bool Solution::isPatientFeasibleOnDayWithExcludedPatient(int p, int d, int r,
                                                         int p2) {
  int s2 = in.PatientSurgeon(p2);
  int admission_day2 = this->AdmissionDay(p2);
  int los2 = in.PatientLengthOfStay(p2);
  int dur2 = in.PatientSurgeryDuration(p2);
  int t2 = this->PatientOperatingTheater(p2);
  Gender gender2 = in.PatientGender(p2);
  // Room Compatability and Admission day
  if (d < in.PatientSurgeryReleaseDay(p) || d > in.PatientLastPossibleDay(p) ||
      in.IncompatibleRoom(p, r)) {
    // std::cout << "AdmissionDay or incompatible room" << std::endl;
    return false;
  }

  // Surgeon Overtime
  int s = in.PatientSurgeon(p);
  int dur_removed = 0;
  // if same surgeon and same day
  if (s == s2 && d == admission_day2) {
    dur_removed = in.PatientSurgeryDuration(p2);
  }
  if (surgeon_day_load[s][d] - dur_removed + in.PatientSurgeryDuration(p) >
      in.SurgeonMaxSurgeryTime(s, d)) {
    // std::cout << "Surgeon overtime: " << surgeon_day_load[s][d] << " + " <<
    // in.PatientSurgeryDuration(p) << " = " << surgeon_day_load[s][d] +
    // in.PatientSurgeryDuration(p) << " > " << in.SurgeonMaxSurgeryTime(s, d)
    // << std::endl;
    return false;
  }

  // Gender mix and room capacity
  // Not only on day of assignment but whole LOS!
  if (in.PatientGender(p) == Gender::A) {
    int d2 = d;
    while (d2 < std::min(d + in.PatientLengthOfStay(p), in.Days())) {
      int gender_removed = 0;
      int cap_removed = 0;
      if (d2 >= admission_day2 && d2 < admission_day2 + los2) {
        if (gender2 == Gender::B) {
          gender_removed = 1;
        }
        cap_removed = 1;
      }
      if (RoomDayBPatients(r, d2) - gender_removed > 0) {
        // std::cout << "Gender not ok" << std::endl;
        return false;
      }
      if (RoomDayLoad(r, d2) - cap_removed == in.RoomCapacity(r)) {
        // std::cout << "No cap free" << std::endl;
        return false;
      }
      d2++;
    }
  } else if (in.PatientGender(p) == Gender::B) {
    int d2 = d;
    while (d2 < std::min(d + in.PatientLengthOfStay(p), in.Days())) {
      int gender_removed = 0;
      int cap_removed = 0;
      if (d2 >= admission_day2 && d2 < admission_day2 + los2) {
        if (gender2 == Gender::A) {
          gender_removed = 1;
        }
        cap_removed = 1;
      }
      if (RoomDayAPatients(r, d2) - gender_removed > 0) {
        // std::cout << "Gender not ok" << std::endl;
        return false;
      }
      if (RoomDayLoad(r, d2) - cap_removed == in.RoomCapacity(r)) {
        // std::cout << "No cap free" << std::endl;
        return false;
      }
      d2++;
    }
  }

  // OT overtime
  int t = 0;
  bool otFound = false;
  while (t < in.OperatingTheaters() && !otFound) {
    int dur_removed = 0;
    if (t2 == t) {
      dur_removed = dur2;
    }
    if (operatingtheater_day_load[t][d] - dur_removed +
            in.PatientSurgeryDuration(p) <=
        in.OperatingTheaterAvailability(t, d)) {
      otFound = true;
    }
    t++;
  }
  return otFound;
}
bool Solution::isPatientFeasibleForRoom(int p, int d, int r) {

  // Room Compatability and Admission day
  if (d < in.PatientSurgeryReleaseDay(p) || d > in.PatientLastPossibleDay(p) ||
      in.IncompatibleRoom(p, r)) {
    // std::cout << "AdmissionDay or incompatible room" << std::endl;
    return false;
  }

  // Surgeon Overtime
  int s = in.PatientSurgeon(p);
  if (surgeon_day_load[s][d] + in.PatientSurgeryDuration(p) >
      in.SurgeonMaxSurgeryTime(s, d)) {
    // std::cout << "Surgeon overtime: " << surgeon_day_load[s][d] << " + " <<
    // in.PatientSurgeryDuration(p) << " = " << surgeon_day_load[s][d] +
    // in.PatientSurgeryDuration(p) << " > " << in.SurgeonMaxSurgeryTime(s, d)
    // << std::endl;
    return false;
  }

  // Gender mix and room capacity
  // Not only on day of assignment but whole LOS!
  if (in.PatientGender(p) == Gender::A) {
    int d2 = d;
    while (d2 < std::min(d + in.PatientLengthOfStay(p), in.Days())) {
      if (RoomDayBPatients(r, d2) > 0) {
        // std::cout << "Gender not ok" << std::endl;
        return false;
      }
      if (RoomDayLoad(r, d2) == in.RoomCapacity(r)) {
        // std::cout << "No cap free" << std::endl;
        return false;
      }
      d2++;
    }
  } else if (in.PatientGender(p) == Gender::B) {
    int d2 = d;
    while (d2 < std::min(d + in.PatientLengthOfStay(p), in.Days())) {
      if (RoomDayAPatients(r, d2) > 0) {
        // std::cout << "Gender not ok" << std::endl;
        return false;
      }
      if (RoomDayLoad(r, d2) == in.RoomCapacity(r)) {
        // std::cout << "No cap free" << std::endl;
        return false;
      }
      d2++;
    }
  }

  // OT overtime
  int t = 0;
  bool otFound = false;
  while (t < in.OperatingTheaters() && !otFound) {
    if (operatingtheater_day_load[t][d] + in.PatientSurgeryDuration(p) <=
        in.OperatingTheaterAvailability(t, d)) {
      otFound = true;
    }
    t++;
  }
  return otFound;
}

vector<int> Solution::sortedPatientsSurgeon(vector<int> patients) {
  vector<pair<int, int>> sorted;
  for (int p = 0; p < patients.size(); p++) {
    pair<int, int> pair;
    pair = make_pair(patients[p], (int)in.PatientSurgeon(patients[p]));
    sorted.push_back(pair);
  }
  std::sort(sorted.begin(), sorted.end(), &compareAscending);
  vector<int> sortedPatients;
  for (int p = 0; p < sorted.size(); p++) {
    sortedPatients.push_back(sorted[p].first);
  }
  return sortedPatients;
}

bool Solution::compareAscending(const pair<int, int> &p1,
                                const pair<int, int> &p2) {
  return (p1.second < p2.second);
}

bool Solution::compareDescending(const pair<int, int> &p1,
                                 const pair<int, int> &p2) {
  return (p1.second > p2.second);
}

bool Solution::compareDoubleAscending(const vector<int> p1,
                                      const vector<int> p2) {
  if (p1[1] < p2[1]) {
    return true;
  } else if (p1[1] == p2[1] && p1[2] < p2[2]) {
    return true;
  } else {
    return false;
  }
}

Solution::Solution(const IHTP_Input &in, const bool verbose)
    : IHTP_Output(in, verbose) {
  this->admission_day = vector<int>(in.Patients(), -1);
  this->room = vector<int>(in.Patients(), -1);
  this->operating_room = vector<int>(in.Patients(), -1);
  this->patient_shift_nurse =
      vector<vector<int>>(in.Patients() + in.Occupants());
  this->room_day_patient_list =
      vector<vector<vector<int>>>(in.Rooms(), vector<vector<int>>(in.Days()));
  this->room_day_a_patients =
      vector<vector<int>>(in.Rooms(), vector<int>(in.Days(), 0));
  this->room_day_b_patients =
      vector<vector<int>>(in.Rooms(), vector<int>(in.Days(), 0));
  this->room_shift_nurse =
      vector<vector<int>>(in.Rooms(), vector<int>(in.Shifts(), -1));
  this->nurse_shift_room_list = vector<vector<vector<int>>>(
      in.Nurses(), vector<vector<int>>(in.Shifts()));
  this->nurse_shift_load =
      vector<vector<int>>(in.Nurses(), vector<int>(in.Shifts(), 0));
  this->operatingtheater_day_patient_list = vector<vector<vector<int>>>(
      in.OperatingTheaters(), vector<vector<int>>(in.Days()));
  this->operatingtheater_day_load =
      vector<vector<int>>(in.OperatingTheaters(), vector<int>(in.Days(), 0));
  this->surgeon_day_load =
      vector<vector<int>>(in.Surgeons(), vector<int>(in.Days(), 0));
  this->surgeon_day_theater_count = vector<vector<vector<int>>>(
      in.Surgeons(),
      vector<vector<int>>(in.Days(), vector<int>(in.OperatingTheaters(), 0)));
  // this->in = in;
  for (int p = 0; p < in.Patients() + in.Occupants(); p++)
    if (p < in.Patients())
      patient_shift_nurse[p].resize(
          in.PatientLengthOfStay(p) * in.ShiftsPerDay(), -1);
    else
      patient_shift_nurse[p].resize(
          in.OccupantLengthOfStay(p - in.Patients()) * in.ShiftsPerDay(), -1);

  UpdatewithOccupantsInfo();
}

Solution &Solution::operator=(const Solution &other) {
  this->Reset();
  this->setInfValue(other.getInfValue());
  this->setObjValue(other.getObjValue());

  for (int p = 0; p < in.Patients(); p++) {
    int d = other.AdmissionDay(p);
    if (d >= 0) {
      int r = other.PatientRoom(p);
      int t = other.PatientOperatingTheater(p);
      this->AssignPatient(p, d, r, t);
    }
  }
  for (int r = 0; r < in.Rooms(); r++) {
    for (int s = 0; s < in.Shifts(); s++) {
      int n = other.getRoomShiftNurse(r, s);
      if (n != -1) {
        this->AssignNurse(n, r, s);
      }
    }
  }

  return *this;
}

Solution::~Solution() {}
