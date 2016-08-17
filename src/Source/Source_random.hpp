#ifndef SOURCE_RANDOM_HPP_
#define SOURCE_RANDOM_HPP_

#include <random>
#include <vector>
#include "../Tools/MIPP/mipp.h"

#include "Source.hpp"

template <typename B>
class Source_random : public Source<B>
{
private:
	std::mt19937 rd_engine; // Mersenne Twister 19937
	// std::minstd_rand rd_engine; // LCG
	std::uniform_int_distribution<B> uniform_dist;

public:
	Source_random(const int seed = 0, const std::string name = "Source_random");

	virtual ~Source_random();

	void generate(mipp::vector<B>& U_K);
};

#endif /* SOURCE_RANDOM_HPP_ */
