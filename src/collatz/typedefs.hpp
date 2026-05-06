#pragma once
#include <stdint.h>



/// @brief Unsigned 128-bit integer.
typedef __uint128_t uint128_t;



/// @brief Signed 128-bit integer.
typedef __int128_t int128_t;



/// @brief Level type is set at 32-bits, which is ~4B levels.  It is also fixed size, not int or size_t.
typedef uint32_t level_t;



/// @brief Collatz sequences are 32-bit because they shouldn't exceed 4B steps.  They need to be fixed size, not int or size_t.
typedef uint32_t seq_size_t;
