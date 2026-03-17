#ifndef INITIALSOLUTION_H
#define INITIALSOLUTION_H

#include "BaseAlgo.h"
#include "IHTP_Validator.h"
#include "TheBigSwapper.h"

class InSol : public BaseAlgo
{

public:

	InSol(const IHTP_Input &in, Solution &out);
	virtual ~InSol();
	Solution solve(int timeLimitSeconds);

	void addMandatoryPatients();
	void addMandatoryGender();
	void addNonMandatoryPatients();
	void addNurses();
	void addNursesContinuityOfCare();
	void addNursesRandom();
    void addMandatoryPatientsRandom();

	int findBestRoomFromList(vector<int>& rooms, int d, int p);

	// Sort functions:
	vector<int> sortMandatoryPatientsDueDay();
	vector<int> sortElectivePatientsFinishTime();
	vector <int> sortOperatingTheatersAvailability(int d);
	vector <int> sortedRoomsSkillLevel(int s);
	vector<int> sortedNursesSkillLevel(int s);
	vector<int> sortedNursesWorkingShifts();

};


#endif /* INITIALSOLUTION_H */
