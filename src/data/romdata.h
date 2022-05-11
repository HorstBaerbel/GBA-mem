// Note that the _Alignas specifier will need C11, as a workaround use __attribute__((aligned(4)))

#pragma once
#include <stdint.h>

#define ROM_DATA_SIZE (24 * 1024) // size of ROM data in DWORDs
extern const uint32_t ROM_DATA[ROM_DATA_SIZE];
