#include <cmath>
#include <mipp.h>
#include <type_traits>

#include "Launcher/Code/RSC/RSC.hpp"
#include "Launcher/Simulation/BFER_std.hpp"

using namespace aff3ct;
using namespace aff3ct::launcher;

template<class L, typename B, typename R, typename Q>
RSC<L, B, R, Q>::RSC(const int argc, const char** argv, std::ostream& stream)
  : L(argc, argv, stream)
  , params_cdc(new factory::Codec_RSC("cdc"))
{
    this->params.set_cdc(params_cdc);
}

template<class L, typename B, typename R, typename Q>
void
RSC<L, B, R, Q>::get_description_args()
{
    params_cdc->get_description(this->args);

    auto penc = params_cdc->enc->get_prefix();

    this->args.erase({ penc + "-fra", "F" });
    this->args.erase({ penc + "-seed", "S" });

    L::get_description_args();
}

template<class L, typename B, typename R, typename Q>
void
RSC<L, B, R, Q>::store_args()
{
    auto dec_rsc = dynamic_cast<factory::Decoder_RSC*>(params_cdc->dec.get());

    params_cdc->store(this->arg_vals);

    if (dec_rsc->simd_strategy == "INTER") this->params.n_frames = mipp::N<Q>();
    if (dec_rsc->simd_strategy == "INTRA") this->params.n_frames = (int)std::ceil(mipp::N<Q>() / 8.f);

    if (std::is_same<Q, int8_t>())
    {
        this->params.qnt->n_bits = 6;
        this->params.qnt->n_decimals = 1;
    }
    else if (std::is_same<Q, int16_t>())
    {
        this->params.qnt->n_bits = 6;
        this->params.qnt->n_decimals = 3;
    }

    L::store_args();
}

// ==================================================================================== explicit template instantiation
#include "Launcher/Simulation/BFER_ite.hpp"
#include "Launcher/Simulation/BFER_std.hpp"
#include "Tools/types.h"
#ifdef AFF3CT_MULTI_PREC
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_std<B_8, R_8, Q_8>, B_8, R_8, Q_8>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_std<B_16, R_16, Q_16>, B_16, R_16, Q_16>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_std<B_32, R_32, Q_32>, B_32, R_32, Q_32>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_std<B_64, R_64, Q_64>, B_64, R_64, Q_64>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_ite<B_8, R_8, Q_8>, B_8, R_8, Q_8>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_ite<B_16, R_16, Q_16>, B_16, R_16, Q_16>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_ite<B_32, R_32, Q_32>, B_32, R_32, Q_32>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_ite<B_64, R_64, Q_64>, B_64, R_64, Q_64>;
#else
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_std<B, R, Q>, B, R, Q>;
template class aff3ct::launcher::RSC<aff3ct::launcher::BFER_ite<B, R, Q>, B, R, Q>;
#endif
// ==================================================================================== explicit template instantiation
