#ifndef BASEALGO_H
#define BASEALGO_H

#include "Solution.hpp"

// Abstract base class
// Inherit all you algorithms from this class
class BaseAlgo
{

public:
	BaseAlgo(const IHTP_Input &in, Solution &out, const int globalBest);
	virtual ~BaseAlgo();

	// Pure virtual function to ensure derived classes implement solve
	virtual Solution solve(int timeLimitSeconds) = 0;

	// Could define this private...
	const IHTP_Input& in;
	Solution& out;

	int globalBest;

};

#endif /* BASEALGO_H */
