#pragma once

#include <cstdint>

namespace DMA
{

    // PLEASE NOTE: We're on ARM, so word means 32-bits, half-word means 16-bit!

    /// @brief General DMA fill routine.
    void dma_fill32(void *destination, uint32_t value, uint16_t nrOfWords, uint16_t channel = 3, uint16_t mode = 0);

    /// @brief General DMA copier.
    void dma_copy32(void *destination, const uint32_t *source, uint16_t nrOfWords, uint16_t channel = 3, uint16_t mode = 0);

} // namespace DMA