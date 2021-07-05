#pragma once

#include <gba_base.h>
#include <cstdint>

/// @brief Use for default 0 initialized static variables
#define STATIC_BSS static EWRAM_BSS
/// @brief Use for initialized static variables
#define STATIC_DATA static EWRAM_DATA

namespace Memory
{

	// PLEASE NOTE: We're on ARM, so word means 32-bits, half-word means 16-bit!

	/// @brief Copy dwords from source to destination.
	/// @param destination Copy destination.
	/// @param source Copy source.
	/// @param nrOfWords Number of dwords to copy.
	extern "C" void memcpy32(void *destination, const void *source, uint32_t nrOfWords) IWRAM_CODE;

	/// @brief Set words in destination to value.
	/// @param destination Set destination.
	/// @param value Value to set destination half-words to.
	/// @param nrOfHwords Number of half-words to set.
	extern "C" void memset16(void *destination, uint16_t value, uint32_t nrOfHwords) IWRAM_CODE;

	/// @brief Set dwords in destination to value.
	/// @param destination Set destination.
	/// @param value Value to set destination dwords to.
	/// @param nrOfWords Number of dwords to set.
	extern "C" void memset32(void *destination, uint32_t value, uint32_t nrOfWords) IWRAM_CODE;

} // namespace Memory
