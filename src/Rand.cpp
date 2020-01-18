#include "Rand.hpp"
#include "Assert.hpp"
#include <cstring>


namespace KK
{

LongRand::LongRand(const RandResultType seed)
	: generator_(seed)
{}

LongRand::RandResultType LongRand::Rand()
{
	return generator_();
}

float LongRand::RandValue(const float next_value_after_max)
{
	return float(double(Rand()) / double(c_max_rand_plus_one_) * double(next_value_after_max));
}

float LongRand::RandValue(const float min_value, const float next_value_after_max)
{
	KK_ASSERT(min_value <= next_value_after_max);
	return RandValue(next_value_after_max - min_value) + min_value;
}

bool LongRand::RandBool()
{
	return (Rand() & 1u) != 0u;
}

bool LongRand::RandBool(const RandResultType chance_denominator)
{
	return RandBool(1u, chance_denominator);
}

bool LongRand::RandBool(const RandResultType chance_numerator, const RandResultType chance_denominator)
{
	KK_ASSERT(chance_numerator <= chance_denominator);

	return
		ExtendedType(Rand()) * ExtendedType(chance_denominator) <
		ExtendedType(c_max_rand_plus_one_) * ExtendedType(chance_numerator);
}

uint32_t LongRand::GetInnerState() const
{
	uint32_t result;
	// We assume, that std::linear_congruential_engine is simple struct without pointers.
	std::memcpy(&result, &generator_, sizeof(Generator));
	return result;
}

void LongRand::SetInnerState(const uint32_t state)
{
	// We assume, that std::linear_congruential_engine is simple struct without pointers.
	std::memcpy(&generator_, &state, sizeof(Generator));
}

} // namespace KK
