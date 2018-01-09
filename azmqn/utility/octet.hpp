/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_UTILITY_OCTET_HPP
#define AZMQN_UTILITY_OCTET_HPP

#include <type_traits>
#include <cstddef>
#include <ostream>

namespace azmqn::utility {
    // like std::byte, but for the wire
    enum class octet : uint8_t { };

    template<typename IntegerType>
    constexpr auto to_integer(octet o) noexcept
        -> std::enable_if_t<std::is_integral_v<IntegerType>, IntegerType>
    { return IntegerType(o); }

    template<typename IntegerType>
    constexpr auto operator<<=(octet& o, IntegerType shift) noexcept
        -> std::enable_if_t<std::is_integral_v<IntegerType>, octet&>
    { return o = octet(static_cast<uint8_t>(o) << shift); }

    template<typename IntegerType>
    constexpr auto operator<<(octet o, IntegerType shift) noexcept
        -> std::enable_if_t<std::is_integral_v<IntegerType>, octet>
    { return octet(static_cast<uint8_t>(o) << shift); }

    template<typename IntegerType>
    constexpr auto operator>>=(octet& o, IntegerType shift) noexcept
        -> std::enable_if_t<std::is_integral_v<IntegerType>, octet&>
    { return o = octet(static_cast<uint8_t>(o) >> shift); }

    template<typename IntegerType>
    constexpr auto operator>>(octet o, IntegerType shift) noexcept
        -> std::enable_if_t<std::is_integral_v<IntegerType>, octet>
    { return octet(static_cast<uint8_t>(o) >> shift); }

    constexpr octet& operator|=(octet& l, octet r) noexcept
    { return l = octet(static_cast<uint8_t>(l) | static_cast<uint8_t>(r)); }

    constexpr octet& operator&=(octet& l, octet r) noexcept
    { return l = octet(static_cast<uint8_t>(l) & static_cast<uint8_t>(r)); }

    constexpr octet& operator^=(octet& l, octet r) noexcept
    { return l = octet(static_cast<uint8_t>(l) ^ static_cast<uint8_t>(r)); }

    constexpr octet operator|(octet l, octet r) noexcept
    { return octet(static_cast<uint8_t>(l) | static_cast<uint8_t>(r)); }

    constexpr octet operator&(octet l, octet r) noexcept
    { return octet(static_cast<uint8_t>(l) & static_cast<uint8_t>(r)); }

    constexpr octet operator^(octet l, octet r) noexcept
    { return octet(static_cast<uint8_t>(l) ^ static_cast<uint8_t>(r)); }

    constexpr octet operator~(octet o) noexcept
    { return octet(~static_cast<uint8_t>(o)); }

    std::ostream& operator<<(std::ostream& stm, octet that) {
        static auto const hex = "0123456789abcdef";
        return stm << hex[static_cast<uint8_t>(that) / 16]
                   << hex[static_cast<uint8_t>(that) % 16];
    }
}

#endif // AZMQN_UTILITY_OCTET_HPP
