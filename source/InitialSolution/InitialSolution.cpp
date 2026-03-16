#include "InitialSolution.h"
#include <algorithm>

InSol::InSol(const IHTP_Input &in, Solution &out) : 
	BaseAlgo(in, out, INT_MAX) {
}
InSol::~InSol() {
}

Solution InSol::solve(int whichMethod) {

    out.Reset();

    if (whichMethod < 0 || whichMethod > 5)
    {
        std::cout << "No valid starting solution method chosen! Method: " << whichMethod << std::endl;
        std::cout << "Will continue with method 0..." << std::endl;
        whichMethod = 0;
    }
    if (whichMethod == 0)
    {
        addMandatoryPatients();
        addNonMandatoryPatients();
        addNurses();
    }
    else if (whichMethod == 1)
    {
        addMandatoryPatients();
        addNonMandatoryPatients();
        addNursesContinuityOfCare();
    }
    else if (whichMethod == 2)
    {
        addMandatoryGender();
        addNonMandatoryPatients();
        addNurses();
        
    }
    else if (whichMethod == 3)
    {
        addMandatoryGender();
        addNonMandatoryPatients();
        addNursesContinuityOfCare();
    }
    else if (whichMethod == 4)
    {
        addMandatoryPatients();
        addNurses();
        addNonMandatoryPatients();
    }
    else if (whichMethod == 5)
    {
        addMandatoryPatients();
        addNursesContinuityOfCare();
        addNonMandatoryPatients();
    }

    vector<int> violations{
  out.RoomGenderMix(),
  out.PatientRoomCompatibility(),
  out.SurgeonOvertime(),
  out.OperatingTheaterOvertime(),
  out.MandatoryUnscheduledPatients(),
  out.AdmissionDay(),
  out.RoomCapacity(),
  out.NursePresence(),
  out.UncoveredRoom() };

    vector<int> costs{
     out.RoomAgeMix(),
     out.RoomSkillLevel(),
     out.ContinuityOfCare(),
     out.ExcessiveNurseWorkload(),
     out.OpenOperatingTheater(),
     out.SurgeonTransfer(),
     out.PatientDelay(),
     out.ElectiveUnscheduledPatients() };

    int infValue = 0;
    for (int i = 0; i < violations.size(); i++)
    {
        infValue += violations[i];
    }

    int objValue = 0;
    for (int i = 0; i < costs.size(); i++)
    {
        objValue += costs[i] * in.Weight(i);
    }

    out.setInfValue(infValue);
    out.setObjValue(objValue);

    // Local search on this solution will be executed seperately

    return out;
}

void InSol::addMandatoryPatients()
{
    // Sort patients on earliest due date (only returns mandatory patients)
    vector<int> sorted = sortMandatoryPatientsDueDay();
    vector<bool> patientsAssigned(sorted.size());

    for (int d = 0; d < in.Days(); d++) {

        vector<int> operatingTheaters = sortOperatingTheatersAvailability(d);

        for (int i = 0; i < sorted.size(); i++) {
            
            int p = sorted.at(i);
            if (patientsAssigned.at(i)) continue;

            // Assign to room with largest capacity
            // In case of a tie: minimize age difference
            // If p is infeasible for this day, there will not be a best room (-1)
            vector<int> v = {};
            int currentBestRoom = findBestRoomFromList(v, d, p);

            if (currentBestRoom > -1) // Otherwise no feasible room found, continue
            {
                // Assign to OT (OT's are sorted largest availability first)
                int j = 0;
                while (j < operatingTheaters.size() && !patientsAssigned.at(i))
                {
                    int t = operatingTheaters.at(j);
                    if (in.OperatingTheaterAvailability(t, d) - out.OperatingTheaterDayLoad(t, d) >= in.PatientSurgeryDuration(p))
                    {
                        out.AssignPatient(p, d, currentBestRoom, t);
                        patientsAssigned[i] = true;
                    }
                    j++;
                }
            }
        }
    }
}

void InSol::addMandatoryGender()
{
    // Sort rooms by gender
    vector<int> roomsGenderA;
    vector<int> roomsGenderB;
    vector<int> remainingRooms;
    
    for (int r = 0; r < in.Rooms(); r++)
    {
        if (out.RoomDayAPatients(r, 0) > 0) { roomsGenderA.push_back(r); }
        else if (out.RoomDayBPatients(r, 0) > 0) { roomsGenderB.push_back(r); }
        else { remainingRooms.push_back(r); }
    }

    // Sort patients on earliest due date
    vector<int> sorted = sortMandatoryPatientsDueDay();
    vector<bool> patientsAssigned(sorted.size());

    for (int d = 0; d < in.Days(); d++) {

        vector<int> operatingTheaters = sortOperatingTheatersAvailability(d);

        for (int i = 0; i < sorted.size(); i++) {
            
            int p = sorted.at(i);
            if (patientsAssigned.at(i)) continue;
            if (d < in.PatientSurgeryReleaseDay(p) || d > in.PatientLastPossibleDay(p)) continue;
            if (in.SurgeonMaxSurgeryTime(in.PatientSurgeon(p), d) - out.SurgeonDayLoad(in.PatientSurgeon(p), d) < in.PatientSurgeryDuration(p)) continue;

            int currentBestRoom = -1;

            if (in.PatientGender(p) == Gender::A)
            {
                currentBestRoom = findBestRoomFromList(roomsGenderA, d, p);
                if (currentBestRoom == -1) 
                { 
                    currentBestRoom = findBestRoomFromList(remainingRooms, d, p);
                    if (currentBestRoom > -1)
                    {
                        roomsGenderA.push_back(currentBestRoom);
                        remainingRooms.erase(std::remove(remainingRooms.begin(), remainingRooms.end(), currentBestRoom), remainingRooms.end());
                    }
                }
                if (currentBestRoom == -1)
                {
                    currentBestRoom = findBestRoomFromList(roomsGenderB, d, p);
                    if (currentBestRoom > -1)
                    {
                        roomsGenderA.push_back(currentBestRoom);
                        roomsGenderB.erase(std::remove(roomsGenderB.begin(), roomsGenderB.end(), currentBestRoom), roomsGenderB.end());
                    }
                }
            }
            else
            {
                currentBestRoom = findBestRoomFromList(roomsGenderB, d, p);
                if (currentBestRoom == -1)
                {
                    currentBestRoom = findBestRoomFromList(remainingRooms, d, p);
                    if (currentBestRoom > -1)
                    {
                        roomsGenderB.push_back(currentBestRoom);
                        remainingRooms.erase(std::remove(remainingRooms.begin(), remainingRooms.end(), currentBestRoom), remainingRooms.end());
                    }
                }
                if (currentBestRoom == -1)
                {
                    currentBestRoom = findBestRoomFromList(roomsGenderA, d, p);
                    if (currentBestRoom > -1)
                    {
                        roomsGenderB.push_back(currentBestRoom);
                        roomsGenderA.erase(std::remove(roomsGenderA.begin(), roomsGenderA.end(), currentBestRoom), roomsGenderA.end());
                    }
                }
            }
            if (currentBestRoom > -1) // Otherwise no feasible room found, continue
            {
                // Assign to OT (OT's are sorted largest availability first)
                int j = 0;
                while (j < operatingTheaters.size() && !patientsAssigned.at(i))
                {
                    int t = operatingTheaters.at(j);
                    if (in.OperatingTheaterAvailability(t, d) - out.OperatingTheaterDayLoad(t, d) >= in.PatientSurgeryDuration(p))
                    {
                        out.AssignPatient(p, d, currentBestRoom, t);
                        patientsAssigned[i] = true;
                    }
                    j++;
                }
            }
        }
    }
}

int InSol::findBestRoomFromList(vector<int> &rooms, int d, int p)
{
    int currentBestRoom = -1;
    int currentBestCapacity = 0;
    int age = in.PatientAgeGroup(p);
    int currentBestAgeDifference = in.AgeGroups() + 1;

    int size = rooms.size();;
    if (rooms.size() == 0) { size = in.Rooms(); }

    // Assign to room with largest capacity
    // In case of a tie: minimize age difference

    for (int i = 0; i < size; i++)
    {
        int r = i;
        if (rooms.size() > 0) { r = rooms.at(i); }

        if (out.isPatientFeasibleForRoom(p, d, r))
        {
            int capacity = in.RoomCapacity(r) - out.RoomDayLoad(r, d);
            if (capacity > currentBestCapacity)
            {
                currentBestRoom = r;
                currentBestCapacity = capacity;
            }
            else if (capacity == currentBestCapacity)
            {
                int ageDifference = 0;
                if (out.RoomDayLoad(r, d) > 0)
                {
                    for (int j = 0; j < out.RoomDayLoad(r, d); j++)
                    {
                        int p2 = out.RoomDayPatient(r, d, j);
                        int difference;
                        if (p2 < in.Patients()) { difference = abs(in.PatientAgeGroup(p2) - age); }
                        else { difference = abs(in.OccupantAgeGroup(p2 - in.Patients()) - age); }
                        if (difference > ageDifference) { ageDifference = difference; }
                    }
                }
                if (ageDifference < currentBestAgeDifference)
                {
                    currentBestAgeDifference = ageDifference;
                    currentBestRoom = r;
                }
            }
        }
    }
    return currentBestRoom;
}

void InSol::addNonMandatoryPatients()
{
    vector<int> sorted = sortElectivePatientsFinishTime();
    vector<bool> patientsAssigned(in.Patients());
    for (int d = 0; d < in.Days(); d++)
    {
        vector<int> operatingTheaters = sortOperatingTheatersAvailability(d);
        for (int i = 0; i < sorted.size(); i++)
        {
            int p = sorted.at(i);
            if (patientsAssigned.at(p)) continue;
            if (d < in.PatientSurgeryReleaseDay(p) || d > in.PatientLastPossibleDay(p)) continue;
            if (in.SurgeonMaxSurgeryTime(in.PatientSurgeon(p), d) - out.SurgeonDayLoad(in.PatientSurgeon(p), d) < in.PatientSurgeryDuration(p)) continue;

            // Assign to room with smallest cost
            int currentBestRoom = -1;
            int currentBestOT = -1;
            int currentBestCost = 10 * in.Weight(7);

            for (int r = 0; r < in.Rooms(); r++)
            {
                if (out.isPatientFeasibleForRoom(p, d, r))
                {
                    int t = out.determineBestOTAndCosts(p, d).at(0);
                    if (t != -1)
                    {
                        int cost = out.CostEvaluationAddingPatient(false, p, r, d, t);
                        if (cost < currentBestCost)
                        {
                            currentBestRoom = r;
                            currentBestOT = t;
                            currentBestCost = cost;

                        }
                    }
                }
            }
            if (currentBestRoom > -1) // Otherwise no feasible room found, continue
            {
                assert(currentBestOT != -1);
                assert(out.OperatingTheaterDayLoad(currentBestOT, d) + in.PatientSurgeryDuration(p) <= in.OperatingTheaterAvailability(currentBestOT, d));
                out.AssignPatient(p, d, currentBestRoom, currentBestOT);
                patientsAssigned[p] = true;
            }
        }
    }
}

void InSol::addNurses()
{
    for (int s = 0; s < in.Shifts(); s++)
    {
        int d = s / in.ShiftsPerDay();
        // Sort rooms on descending max skill level 
        vector<int> rooms = sortedRoomsSkillLevel(s);
        // Sort nurses on descending skill level
        vector<int> nurses = sortedNursesSkillLevel(s);

        // Assign nurse with highest skill level until max workload is reached
        int j = 0;
        int n = nurses[j];
        for (int i = 0; i < rooms.size(); i++)
        {
            int r = rooms[i];
            bool assigned = false;
            while (!assigned && j < nurses.size())
            {
                if (out.NurseShiftLoad(n, s) + out.RoomDayLoad(r, d) <= in.NurseMaxLoad(n, s)) {
                    out.AssignNurse(n, r, s);
                    assigned = true;
                }
                else {
                    j++;
                    if (j < nurses.size()) { n = nurses[j]; }
                }
            }
            if (j == nurses.size())
            {
                out.AssignNurse(nurses[0], r, s);
                assigned = true;
            }
        }
    }
}

void InSol::addNursesContinuityOfCare()
{
    vector<int> nurses = sortedNursesWorkingShifts();
    vector<vector<int>> roomNurses(in.Rooms());

    for (int s = 0; s < in.Shifts(); s++)
    {
        for (int r = 0; r < in.Rooms(); r++)
        {
            // First try to assign nurse that already works in this room
            int i = 0;
            bool nurseFound = false;
            while (i < roomNurses[r].size() && !nurseFound)
            {
                int n = roomNurses[r][i];
                if (in.IsNurseWorkingInShift(n, s))
                {
                    out.AssignNurse(n, r, s);
                    nurseFound = true;
                }
                i++;
            }
            i = 0;
            while (i < nurses.size() && !nurseFound)
            {
                int n = nurses[i];
                if (in.IsNurseWorkingInShift(n, s))
                {
                    out.AssignNurse(n, r, s);
                    nurseFound = true;
                    roomNurses[r].push_back(n);
                }
                i++;
            }
        }
    }
}

vector<int> InSol::sortMandatoryPatientsDueDay() {

    // Only returns mandatory patients
    vector<pair<int, int>> sorted;
    for (int p = 0; p < in.Patients(); p++)
    {
        if (in.PatientMandatory(p))
        {
            pair<int, int> pair;
            pair = make_pair(p, in.PatientSurgeryReleaseDay(p) + in.PatientLengthOfStay(p));
            sorted.push_back(pair);
        }
    }
    std::sort(sorted.begin(), sorted.end(), &out.compareAscending);
    vector<int> patients;
    for (int p = 0; p < sorted.size(); p++)
    {
        patients.push_back(sorted[p].first);
    }
    return patients;
}

vector<int> InSol::sortElectivePatientsFinishTime() {

    vector<vector<int>> sorted;
    for (int p = 0; p < in.Patients(); p++)
    {
        if (!in.PatientMandatory(p))
        {
            vector<int> patient;
            patient.push_back(p);
            patient.push_back(in.PatientSurgeryReleaseDay(p) + in.PatientLengthOfStay(p));
            patient.push_back(in.PatientLengthOfStay(p));
            sorted.push_back(patient);
        }
    }

    std::sort(sorted.begin(), sorted.end(), &out.compareDoubleAscending);
    vector<int> patients;
    for (int p = 0; p < sorted.size(); p++)
    {
        patients.push_back(sorted[p][0]);
    }
    return patients;

}

vector<int> InSol::sortOperatingTheatersAvailability(int d)
{
    vector<pair<int, int>> sorted;
    for (int t = 0; t < in.OperatingTheaters(); t++)
    {
        pair<int, int> p;
        p = make_pair(t, in.OperatingTheaterAvailability(t, d));
        sorted.push_back(p);
    }
    std::sort(sorted.begin(), sorted.end(), &out.compareDescending);
    vector<int> operatingTheaters;
    for (int t = 0; t < sorted.size(); t++)
    {
        operatingTheaters.push_back(sorted[t].first);
    }
    return operatingTheaters;
}

vector<int> InSol::sortedRoomsSkillLevel(int s)
{
    vector<pair<int, int>> sorted;
    for (int r = 0; r < in.Rooms(); r++)
    {
        pair<int, int> pair = make_pair(r, out.RoomShiftSkillLevel(r, s));
        sorted.push_back(pair);
    }
    std::sort(sorted.begin(), sorted.end(), &out.compareDescending);
    vector<int> sortedRooms;
    for (int i = 0; i < sorted.size(); i++)
    {
        sortedRooms.push_back(sorted[i].first);
    }
    return sortedRooms;
}

vector<int> InSol::sortedNursesSkillLevel(int s)
{
    vector<pair<int, int>> sorted;
    for (int n = 0; n < in.Nurses(); n++)
    {
        if (in.IsNurseWorkingInShift(n, s))
        {
            pair<int, int> pair;
            pair = make_pair(n, in.NurseSkillLevel(n));
            sorted.push_back(pair);
        }
    }
    std::sort(sorted.begin(), sorted.end(), &out.compareDescending);
    vector<int> sortedNurses;
    for (int i = 0; i < sorted.size(); i++)
    {
        sortedNurses.push_back(sorted[i].first);
    }
    return sortedNurses;
}

vector<int> InSol::sortedNursesWorkingShifts()
{
    vector<pair<int, int>> sorted;
    for (int n = 0; n < in.Nurses(); n++)
    {
        pair<int, int> pair;
        pair = make_pair(n, in.NurseWorkingShifts(n));
        sorted.push_back(pair);
    }
    std::sort(sorted.begin(), sorted.end(), &out.compareDescending);
    vector<int> sortedNurses;
    for (int i = 0; i < sorted.size(); i++)
    {
        sortedNurses.push_back(sorted[i].first);
    }
    return sortedNurses;
}

