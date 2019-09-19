#ifndef ENCODER_POLAR_MK_HPP_
#define ENCODER_POLAR_MK_HPP_

#include <vector>

#include "Tools/Code/Polar/Frozenbits_notifier.hpp"
#include "Tools/Code/Polar/Polar_code.hpp"
#include "Module/Encoder/Encoder.hpp"

namespace aff3ct
{
namespace module
{
template <typename B = int>
class Encoder_polar_MK : public Encoder<B>, public tools::Frozenbits_notifier
{
protected:
	const tools::Polar_code           code;
	const std::vector<bool>&          frozen_bits;
	      std::vector<std::vector<B>> Ke;
	      std::vector<uint32_t>       idx;
	      std::vector<B>              u;

public:
	Encoder_polar_MK(const int& K, const int& N, const tools::Polar_code& code, const std::vector<bool>& frozen_bits,
	                 const int n_frames = 1);

	virtual ~Encoder_polar_MK() = default;

	// bool is_codeword(const B *X_N);

	virtual void notify_frozenbits_update();

protected:
	virtual void _encode(const B *U_K, B *X_N, const int frame_id);
	void convert(const B *U_K, B *U_N);
	void light_encode(B *X_N);
};
}
}

#endif // ENCODER_POLAR_MK_HPP_
