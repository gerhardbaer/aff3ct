#include <cmath>
#include <sstream>
#include <streampu.hpp>
#include <vector>

#include "Module/Encoder/RSC/Encoder_RSC_generic_sys.hpp"

using namespace aff3ct;
using namespace aff3ct::module;

template<typename B>
Encoder_RSC_generic_sys<B>::Encoder_RSC_generic_sys(const int& K,
                                                    const int& N,
                                                    const bool buffered_encoding,
                                                    const std::vector<int> poly)
  : Encoder_RSC_sys<B>(K, N, (int)std::floor(std::log2(poly[0])), buffered_encoding)
  , out_parity()
  , next_state()
  , sys_tail()
{
    const std::string name = "Encoder_RSC_generic_sys";
    this->set_name(name);
    for (auto& t : this->tasks)
        t->set_replicability(true);

    if (poly.size() < 2)
    {
        std::stringstream message;
        message << "'poly.size()' has to be equal or greater than 2 ('poly.size()' = " << poly.size() << ").";
        throw spu::tools::length_error(__FILE__, __LINE__, __func__, message.str());
    }

    if (std::floor(std::log2(poly[0])) != std::floor(std::log2(poly[1])))
    {
        std::stringstream message;
        message << "floor(log2('poly[0]')) has to be equal to floor(log2('poly[1]')) ('poly[0]' = " << poly[0]
                << ", 'poly[1]' = " << poly[1] << ").";
        throw spu::tools::invalid_argument(__FILE__, __LINE__, __func__, message.str());
    }

    for (auto s = 0; s < this->n_states; s++)
    {
        auto temp_tail = 0, temp_parity = 0;
        for (auto i = 0; i < this->n_ff; i++)
        {
            temp_tail ^= ((s >> i) & 0x1) & ((poly[0] >> i) & 0x1);
            temp_parity ^= ((s >> i) & 0x1) & ((poly[1] >> i) & 0x1);
        }

        out_parity.push_back(temp_tail ^ temp_parity);
        out_parity.push_back((1 - temp_tail) ^ temp_parity);

        sys_tail.push_back(temp_tail);

        next_state.push_back((s >> 1) ^ (temp_tail << (this->n_ff - 1)));
        next_state.push_back((s >> 1) ^ ((1 - temp_tail) << (this->n_ff - 1)));
    }
}

template<typename B>
Encoder_RSC_generic_sys<B>*
Encoder_RSC_generic_sys<B>::clone() const
{
    auto m = new Encoder_RSC_generic_sys(*this);
    m->deep_copy(*this);
    return m;
}

template<typename B>
int
Encoder_RSC_generic_sys<B>::inner_encode(const int bit_sys, int& state)
{
    auto symbol = this->out_parity[2 * state + bit_sys];
    state = this->next_state[2 * state + bit_sys];
    return symbol;
}

template<typename B>
int
Encoder_RSC_generic_sys<B>::tail_bit_sys(const int& state)
{
    return this->sys_tail[state];
}

// ==================================================================================== explicit template instantiation
#include "Tools/types.h"
#ifdef AFF3CT_MULTI_PREC
template class aff3ct::module::Encoder_RSC_generic_sys<B_8>;
template class aff3ct::module::Encoder_RSC_generic_sys<B_16>;
template class aff3ct::module::Encoder_RSC_generic_sys<B_32>;
template class aff3ct::module::Encoder_RSC_generic_sys<B_64>;
#else
template class aff3ct::module::Encoder_RSC_generic_sys<B>;
#endif
// ==================================================================================== explicit template instantiation
