#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "Module/Decoder/LDPC/BP/Horizontal_layered/Decoder_LDPC_BP_horizontal_layered_inter.hpp"
#include "Module/Decoder/LDPC/BP/Vertical_layered/Decoder_LDPC_BP_vertical_layered_inter.hpp"
#include "Tools/Perf/Reorderer/Reorderer.hpp"
#include "Tools/general_utils.h"

namespace aff3ct
{
namespace module
{

template<typename B, typename R, class Update_rule>
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::Decoder_LDPC_BP_vertical_layered_inter(
  const int K,
  const int N,
  const int n_ite,
  const tools::Sparse_matrix& _H,
  const std::vector<unsigned>& info_bits_pos,
  const Update_rule& up_rule,
  const bool enable_syndrome,
  const int syndrome_depth)
  : Decoder_SISO<B, R>(K, N)
  , Decoder_LDPC_BP(K, N, n_ite, _H, enable_syndrome, syndrome_depth)
  , info_bits_pos(info_bits_pos)
  , up_rule(up_rule)
  , sat_val((R)((1 << ((sizeof(R) * 8 - 2) - (int)std::log2(this->H.get_rows_max_degree()))) - 1))
  , var_nodes(this->get_n_waves(), mipp::vector<mipp::Reg<R>>(N))
  , messages(this->get_n_waves(), mipp::vector<mipp::Reg<R>>(this->H.get_n_connections()))
  , contributions(this->H.get_cols_max_degree())
  , messages_offsets(this->H.get_n_cols())
  , Y_N_reorderered(N)
  , V_reorderered(N)
{
    const std::string name = "Decoder_LDPC_BP_vertical_layered_inter<" + this->up_rule.get_name() + ">";
    this->set_name(name);
    this->set_n_frames_per_wave(mipp::N<R>());
    for (auto& t : this->tasks)
        t->set_replicability(true);

    tools::check_LUT(info_bits_pos, "info_bits_pos", (size_t)K);

    if (this->sat_val <= 0)
    {
        std::stringstream message;
        message << "'sat_val' has to be greater than 0 ('sat_val' = " << this->sat_val << ").";
        throw spu::tools::runtime_error(__FILE__, __LINE__, __func__, message.str());
    }

    size_t cur_off_msg = 0;
    const auto n_chk_nodes = (int)this->H.get_n_cols();
    for (auto c = 0; c < n_chk_nodes; c++)
    {
        messages_offsets[c] = (uint32_t)cur_off_msg;
        cur_off_msg += this->H[c].size();
    }

    this->reset();
}

template<typename B, typename R, class Update_rule>
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>*
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::clone() const
{
    auto m = new Decoder_LDPC_BP_vertical_layered_inter(*this);
    m->deep_copy(*this);
    return m;
}

template<typename B, typename R, class Update_rule>
void
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_reset(const size_t frame_id)
{
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();
    const auto zero = mipp::Reg<R>((R)0);
    std::fill(this->messages[cur_wave].begin(), this->messages[cur_wave].end(), zero);
    std::fill(this->var_nodes[cur_wave].begin(), this->var_nodes[cur_wave].end(), zero);
}

template<typename B, typename R, class Update_rule>
void
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_load(const R* Y_N, const size_t frame_id)
{
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();

    std::vector<const R*> frames(mipp::N<R>());
    for (auto f = 0; f < mipp::N<R>(); f++)
        frames[f] = Y_N + f * this->N;
    tools::Reorderer_static<R, mipp::N<R>()>::apply(frames, (R*)this->Y_N_reorderered.data(), this->N);

    for (auto i = 0; i < (int)var_nodes[cur_wave].size(); i++)
        this->var_nodes[cur_wave][i] += this->Y_N_reorderered[i]; // var_nodes contain previous extrinsic information
}

template<typename B, typename R, class Update_rule>
int
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_decode_siso(const R* Y_N1,
                                                                        int8_t* CWD,
                                                                        R* Y_N2,
                                                                        const size_t frame_id)
{
    // memory zones initialization
    this->_load(Y_N1, frame_id);

    auto status = this->_decode(frame_id);

    // prepare for next round by processing extrinsic information
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();
    for (auto v = 0; v < this->N; v++)
        this->var_nodes[cur_wave][v] -= Y_N_reorderered[v];

    std::vector<R*> frames(mipp::N<R>());
    for (auto f = 0; f < mipp::N<R>(); f++)
        frames[f] = Y_N2 + f * this->N;
    tools::Reorderer_static<R, mipp::N<R>()>::apply_rev((R*)this->var_nodes[cur_wave].data(), frames, this->N);

    for (auto f = 0; f < mipp::N<R>(); f++)
        CWD[f] = !((status >> (mipp::N<R>() - 1 - f)) & 1);
    return status;
}

template<typename B, typename R, class Update_rule>
int
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_decode_siho(const R* Y_N,
                                                                        int8_t* CWD,
                                                                        B* V_K,
                                                                        const size_t frame_id)
{
    //	auto t_load = std::chrono::steady_clock::now(); // -----------------------------------------------------------
    // LOAD
    this->_load(Y_N, frame_id);
    //	auto d_load = std::chrono::steady_clock::now() - t_load;

    //	auto t_decod = std::chrono::steady_clock::now(); // --------------------------------------------------------
    // DECODE
    auto status = this->_decode(frame_id);
    //	auto d_decod = std::chrono::steady_clock::now() - t_decod;

    //	auto t_store = std::chrono::steady_clock::now(); // ---------------------------------------------------------
    // STORE
    // take the hard decision
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();
    for (auto v = 0; v < this->K; v++)
    {
        const auto k = this->info_bits_pos[v];
        V_reorderered[v] = mipp::cast<R, B>(this->var_nodes[cur_wave][k]) >> (sizeof(B) * 8 - 1);
    }

    std::vector<B*> frames(mipp::N<R>());
    for (auto f = 0; f < mipp::N<R>(); f++)
        frames[f] = V_K + f * this->K;
    tools::Reorderer_static<B, mipp::N<R>()>::apply_rev((B*)V_reorderered.data(), frames, this->K);
    //	auto d_store = std::chrono::steady_clock::now() - t_store;

    //	(*this)[dec::tsk::decode_siho].update_timer(dec::tm::decode_siho::load,   d_load);
    //	(*this)[dec::tsk::decode_siho].update_timer(dec::tm::decode_siho::decode, d_decod);
    //	(*this)[dec::tsk::decode_siho].update_timer(dec::tm::decode_siho::store,  d_store);

    for (auto f = 0; f < mipp::N<R>(); f++)
        CWD[f] = !((status >> (mipp::N<R>() - 1 - f)) & 1);
    return status;
}

template<typename B, typename R, class Update_rule>
int
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_decode_siho_cw(const R* Y_N,
                                                                           int8_t* CWD,
                                                                           B* V_N,
                                                                           const size_t frame_id)
{
    //	auto t_load = std::chrono::steady_clock::now(); // -----------------------------------------------------------
    // LOAD
    this->_load(Y_N, frame_id);
    //	auto d_load = std::chrono::steady_clock::now() - t_load;

    //	auto t_decod = std::chrono::steady_clock::now(); // --------------------------------------------------------
    // DECODE
    auto status = this->_decode(frame_id);
    //	auto d_decod = std::chrono::steady_clock::now() - t_decod;

    //	auto t_store = std::chrono::steady_clock::now(); // ---------------------------------------------------------
    // STORE
    // take the hard decision
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();
    for (auto v = 0; v < this->N; v++)
        V_reorderered[v] = mipp::cast<R, B>(this->var_nodes[cur_wave][v]) >> (sizeof(B) * 8 - 1);

    std::vector<B*> frames(mipp::N<R>());
    for (auto f = 0; f < mipp::N<R>(); f++)
        frames[f] = V_N + f * this->N;
    tools::Reorderer_static<B, mipp::N<R>()>::apply_rev((B*)V_reorderered.data(), frames, this->N);
    //	auto d_store = std::chrono::steady_clock::now() - t_store;

    //	(*this)[dec::tsk::decode_siho_cw].update_timer(dec::tm::decode_siho_cw::load,   d_load);
    //	(*this)[dec::tsk::decode_siho_cw].update_timer(dec::tm::decode_siho_cw::decode, d_decod);
    //	(*this)[dec::tsk::decode_siho_cw].update_timer(dec::tm::decode_siho_cw::store,  d_store);

    for (auto f = 0; f < mipp::N<R>(); f++)
        CWD[f] = !((status >> (mipp::N<R>() - 1 - f)) & 1);
    return status;
}

template<typename B, typename R, class Update_rule>
int
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_decode(const size_t frame_id)
{
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();

    this->up_rule.begin_decoding(this->n_ite);

    int packed_synd = 0;
    for (auto ite = 0; ite < this->n_ite; ite++)
    {
        this->up_rule.begin_ite(ite);
        this->_decode_single_ite(this->var_nodes[cur_wave], this->messages[cur_wave]);
        this->up_rule.end_ite();

        packed_synd = this->_check_syndrome_soft_status(this->var_nodes[cur_wave]);
        if (packed_synd == spu::runtime::status_t::SUCCESS && this->enable_syndrome) break;
    }

    this->up_rule.end_decoding();

    return packed_synd;
}

template<typename B, typename R, class Update_rule>
void
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_decode_single_ite(mipp::vector<mipp::Reg<R>>& var_nodes,
                                                                              mipp::vector<mipp::Reg<R>>& messages)
{
    // vertical layered scheduling
    const auto n_var_nodes = (int)this->H.get_n_rows();
    for (auto vv = 0; vv < n_var_nodes; vv++)
    {
        auto msg_acc = mipp::Reg<R>((R)0);
        const auto var_degree = (int)this->H.get_cols_from_row(vv).size();
        for (auto c = 0; c < var_degree; c++)
        {
            auto v_out = -1;
            const auto cc = (int)this->H.get_cols_from_row(vv)[c];
            const auto off_msg = (int)this->messages_offsets[cc];
            const auto chk_degree = (int)this->H[cc].size();
            this->up_rule.begin_chk_node_in(cc, chk_degree);
            for (auto v = 0; v < chk_degree; v++)
            {
                const auto var_id = this->H[cc][v];
                v_out = (var_id == (unsigned)vv) ? v : v_out;
                this->contributions[v] = var_nodes[var_id] - messages[off_msg + v];
                this->up_rule.compute_chk_node_in(v, this->contributions[v]);
            }
            this->up_rule.end_chk_node_in();

            msg_acc -= messages[off_msg + v_out];
            this->up_rule.begin_chk_node_out(cc, chk_degree);
            messages[off_msg + v_out] = this->up_rule.compute_chk_node_out(v_out, this->contributions[v_out]);
            this->up_rule.end_chk_node_out();
            msg_acc += messages[off_msg + v_out];
        }
        var_nodes[vv] += msg_acc;
    }
}

template<typename B, typename R, class Update_rule>
bool
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_check_syndrome_soft(
  const mipp::vector<mipp::Reg<R>>& var_nodes)
{
    if (this->enable_syndrome)
    {
        const auto zero = mipp::Msk<mipp::N<B>()>(false);
        auto syndrome = zero;

        auto n_chk_nodes = (int)H.get_n_cols();
        auto c = 0;
        auto syndrome_scalar = true;
        while (c < n_chk_nodes && (syndrome_scalar = mipp::testz(syndrome)))
        {
            auto sign = zero;
            const auto chk_degree = (int)this->H[c].size();
            for (auto v = 0; v < chk_degree; v++)
            {
                const auto value = var_nodes[this->H[c][v]];
                sign ^= mipp::sign(value);
            }

            syndrome |= sign;
            c++;
        }

        this->cur_syndrome_depth = syndrome_scalar ? (this->cur_syndrome_depth + 1) % this->syndrome_depth : 0;
        return syndrome_scalar && (this->cur_syndrome_depth == 0);
    }
    else
        return false;
}

template<typename B, typename R, class Update_rule>
int
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::_check_syndrome_soft_status(
  const mipp::vector<mipp::Reg<R>>& var_nodes)
{
    if (this->enable_syndrome)
    {
        const auto zero = mipp::Msk<mipp::N<B>()>(false);
        auto syndrome = zero;

        auto n_chk_nodes = (int)H.get_n_cols();
        auto c = 0;
        auto syndrome_scalar = true;
        while (c < n_chk_nodes)
        {
            auto sign = zero;
            const auto chk_degree = (int)this->H[c].size();
            for (auto v = 0; v < chk_degree; v++)
            {
                const auto value = var_nodes[this->H[c][v]];
                sign ^= mipp::sign(value);
            }

            syndrome |= sign;
            c++;
        }

        syndrome_scalar = mipp::testz(syndrome);

        this->cur_syndrome_depth = syndrome_scalar ? (this->cur_syndrome_depth + 1) % this->syndrome_depth : 0;
        if (this->cur_syndrome_depth == 0)
        {
            if (!syndrome_scalar)
            {
                int packed_synd = 0;
                for (auto n = 0; n < mipp::N<B>(); n++)
                {
                    packed_synd <<= 1;
                    packed_synd |= syndrome[n];
                }
                return packed_synd;
            }
            else
                return spu::runtime::status_t::SUCCESS;
        }
        return spu::runtime::status_t::FAILURE;
    }
    else
        return spu::runtime::status_t::SUCCESS;
}

template<typename B, typename R, class Update_rule>
void
Decoder_LDPC_BP_vertical_layered_inter<B, R, Update_rule>::set_n_frames(const size_t n_frames)
{
    const auto old_n_frames = this->get_n_frames();
    if (old_n_frames != n_frames)
    {
        Decoder_SISO<B, R>::set_n_frames(n_frames);

        const auto vec_size = this->var_nodes[0].size();
        this->var_nodes.resize(this->get_n_waves(), mipp::vector<mipp::Reg<R>>(vec_size));

        const auto vec_size2 = this->messages[0].size();
        this->messages.resize(this->get_n_waves(), mipp::vector<mipp::Reg<R>>(vec_size2));
    }
}

}
}
