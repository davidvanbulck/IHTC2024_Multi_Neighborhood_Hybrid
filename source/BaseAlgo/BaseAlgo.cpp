#include "BaseAlgo.h"

BaseAlgo::BaseAlgo(const IHTP_Input &in, Solution &out, const int globalBest) : 
	in(in),	out(out), globalBest(globalBest) // Copy
{
}

BaseAlgo::~BaseAlgo(){
}

