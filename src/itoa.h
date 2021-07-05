#pragma once

#include <cstdint>

char *itoa(uint32_t value, char *result, uint32_t base = 10);
char *itoa(int32_t value, char *result, uint32_t base = 10);
char *fptoa(int32_t value, char *result, uint32_t BITSF, uint32_t precision = 0);
