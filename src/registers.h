#pragma once

#include <cstdint>
#include <type_traits>

template <typename VALUE_TYPE, std::uintptr_t ADDRESS>
struct constptr_t
{
    using value_type = std::remove_reference<VALUE_TYPE>;
    using type = VALUE_TYPE;
    constexpr static std::uintptr_t address = ADDRESS;

    constexpr operator auto() const { return *reinterpret_cast<type *>(address); }
    constexpr void operator=(type v) const { *reinterpret_cast<type *>(address) = v; }
    constexpr type *operator&() const { return reinterpret_cast<type *>(address); }
    constexpr type &operator*() const { return *operator&(); }
    constexpr type &operator[](std::size_t idx) const { return operator&()[idx]; }
};

template <typename VALUE_TYPE, std::uintptr_t ADDRESS>
using reg_t = constptr_t<volatile VALUE_TYPE, ADDRESS>;
