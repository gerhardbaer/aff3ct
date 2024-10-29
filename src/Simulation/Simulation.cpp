#include <streampu.hpp>

#include "Simulation/Simulation.hpp"

using namespace aff3ct;
using namespace aff3ct::simulation;

Simulation ::Simulation()
  : simu_error(false)
{
    spu::tools::Signal_handler::init();
}

bool
Simulation ::is_error() const
{
    return this->simu_error;
}
