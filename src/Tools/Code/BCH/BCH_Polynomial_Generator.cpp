#include <sstream>

#include "BCH_Polynomial_Generator.hpp"

#include "Tools/Exception/exception.hpp"

using namespace aff3ct::tools;

BCH_Polynomial_Generator
::BCH_Polynomial_Generator(const int& K, const int& N, const int& t)
 : Galois(K, N), t(t), d(2 * t + 1)
{
	if (t < 1)
	{
		std::stringstream message;
		message << "The correction power 't' has to be strictly positive ('t' = " << t << ").";
		throw invalid_argument(__FILE__, __LINE__, __func__, message.str());
	}

	compute_polynomial();
}

BCH_Polynomial_Generator
::~BCH_Polynomial_Generator()
{
}

int BCH_Polynomial_Generator
::get_d() const
{
	return d;
}

int BCH_Polynomial_Generator
::get_t() const
{
	return t;
}


int BCH_Polynomial_Generator
::get_n_rdncy() const
{
	return g.size() -1;
}

const std::vector<int>& BCH_Polynomial_Generator
::get_g() const
{
	return g;
}

void BCH_Polynomial_Generator
::compute_polynomial()
{
	int rdncy = 0;

	bool test;

	std::vector<int> zeros, min;
	std::vector<std::vector<int>> cycle_sets(2, std::vector<int>(1));

	/* Generate cycle sets modulo N, N = 2**m - 1 */
	cycle_sets[0][0] = 0;
	cycle_sets[1][0] = 1;

	int cycle_set_representative = 0;
	do
	{
		/* Generate the last cycle set */
		auto& cycle_set = cycle_sets.back();
		cycle_set.reserve(32);
		do
		{
			cycle_set.push_back((cycle_set.back() * 2) % N);
		}
		while (((cycle_set.back() * 2) % this->N) != cycle_set.front());

		/* find next cycle set representative */
		do
		{
			cycle_set_representative++;
			test = false;

			/* Examine each cycle sets */
			for (unsigned i = 1; (i < cycle_sets.size()) && !test; i++)
				for (unsigned j = 0; (j < cycle_sets[i].size()) && !test; j++)
					test = (cycle_set_representative == cycle_sets[i][j]);
		}
		while (test && (cycle_set_representative < (this->N - 1)));

		if (!test)
		{
			/* add a new cycle set starting with the new representative */
			cycle_sets.push_back(std::vector<int>(1, cycle_set_representative));
		}
	}
	while (cycle_set_representative < (this->N - 1));


	/* Search for roots 1, 2, ..., d-1 in cycle sets */
	for (unsigned i = 1; i < cycle_sets.size(); i++)
	{
		test = false;

		for (unsigned j = 0; (j < cycle_sets[i].size()) && !test; j++)
			for (int root = 1; (root < d) && !test; root++)
				if (root == cycle_sets[i][j])
				{
					test = true;
					min.push_back((int)i);
					rdncy += cycle_sets[i].size();
				}
	}


	if (this->K > this->N - rdncy)
	{
		std::stringstream message;
		message << "'K' seems to be too big for this correction power 't' ('K' = " << this->K << ", 't' = " << t
		        << ", 'N' = " << this->N << ", 'rdncy' = " << rdncy << ").";
		throw runtime_error(__FILE__, __LINE__, __func__, message.str());
	}


	zeros.resize(rdncy+1);
	int kaux = 1;
	test = true;

	for (unsigned i = 0; i < min.size() && test; i++)
		for (unsigned j = 0; j < cycle_sets[min[i]].size() && test; j++)
		{
			zeros[kaux] = cycle_sets[min[i]][j];
			kaux++;
			test = (kaux <= rdncy);
		}


    g.resize(rdncy+1);

	/* Compute the generator polynomial */
	g[0] = alpha_to[zeros[1]];
	g[1] = 1; /* g(x) = (X + zeros[1]) initially */
	for (int i = 2; i <= rdncy; i++)
	{
		g[i] = 1;
		for (int j = i - 1; j > 0; j--)
			if (g[j] != 0)
				g[j] = g[j - 1] ^ alpha_to[(index_of[g[j]] + zeros[i]) % N];
			else
				g[j] = g[j - 1];

		g[0] = alpha_to[(index_of[g[0]] + zeros[i]) % N];
	}
}
