/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_WIRE_HPP
#define AZMQN_DETAIL_WIRE_HPP

#include <boost/assert.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/container/small_vector.hpp>

#include <cstddef>
#include <utility>
#include <array>
#include <limits>

namespace  azmqn::detail::transport {
    using octet_t = unsigned char; // TODO std::byte
    using mutable_buffer_t = boost::asio::mutable_buffer;
    using const_buffer_t = boost::asio::const_buffer;

    namespace wire {
        constexpr auto small_size = 64;
        constexpr auto max_small_size = std::numeric_limits<octet_t>::max();
        constexpr auto min_framing_octets = 1 + sizeof(octet_t);
        constexpr auto max_framing_octets = 1 + sizeof(uint64_t);

        static_assert(small_size > max_framing_octets);
        static_assert(min_framing_octets > sizeof(octet_t));
        static_assert(max_framing_octets > sizeof(uint64_t));

        using buffer_t = boost::container::small_vector<octet_t, small_size>;
        const_buffer_t buffer(buffer_t const& buf) {
            return boost::asio::buffer(buf.data(), buf.size());
        }

        mutable_buffer_t buffer(buffer_t& buf) {
            return boost::asio::buffer(buf.data(), buf.size());
        }

        using framing_t = std::array<octet_t, max_framing_octets>;

        enum class flags : octet_t {
            is_message = 0x0,
            is_more = 0x1,
            is_long = 0x2,
            is_command = 0x4,
            none = 0x0
        };
        constexpr octet_t operator+(flags f) { return static_cast<octet_t>(f); }
        constexpr bool is_set(octet_t o, flags f) { return o & +f; }

        constexpr static bool is_more(octet_t o) { return is_set(o, flags::is_more); }
        constexpr static bool is_long(octet_t o) { return is_set(o, flags::is_long); }
        constexpr static bool is_command(octet_t o) { return is_set(o, flags::is_command); }

        template<typename T>
        using result_t = std::pair<T, const_buffer_t>;

        mutable_buffer_t put_uint8(boost::asio::mutable_buffer buf, uint8_t val) {
            *boost::asio::buffer_cast<octet_t*>(buf) = val;
            return buf + sizeof(val);
        }

        result_t<uint8_t> get_uint8(boost::asio::const_buffer buf) {
            BOOST_ASSERT(boost::asio::buffer_size(buf) >= sizeof(uint8_t));

            auto const r = *boost::asio::buffer_cast<octet_t const*>(buf);
            return std::make_pair(r, buf + sizeof(uint8_t));
        }

        mutable_buffer_t put_uint16(boost::asio::mutable_buffer buf, uint16_t val) {
            auto b = boost::asio::buffer_cast<octet_t*>(buf);
            b[0] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[1] = static_cast<octet_t>(val & 0xff);
            return buf + sizeof(val);
        }

        result_t<uint16_t> get_uint16(boost::asio::const_buffer buf) {
            BOOST_ASSERT(boost::asio::buffer_size(buf) >= sizeof(uint16_t));

            auto b = boost::asio::buffer_cast<octet_t const*>(buf);
            auto const r = static_cast<uint16_t>(b[0]) << 8 |
                           static_cast<uint16_t>(b[1]);
            return std::make_pair(r, buf + sizeof(uint16_t));
        }

        mutable_buffer_t put_uint32(boost::asio::mutable_buffer buf, uint32_t val) {
            auto b = boost::asio::buffer_cast<octet_t*>(buf);
            b[0] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[3] = static_cast<octet_t>(val & 0xff);
            return buf + sizeof(val);
        }

        result_t<uint32_t> get_uint32(boost::asio::const_buffer buf) {
            BOOST_ASSERT(boost::asio::buffer_size(buf) >= sizeof(uint32_t));

            auto b = boost::asio::buffer_cast<octet_t const*>(buf);
            auto const r = static_cast<uint32_t>(b[0]) << 24 |
                           static_cast<uint32_t>(b[1]) << 16 |
                           static_cast<uint32_t>(b[2]) << 8  |
                           static_cast<uint32_t>(b[3]);
            return std::make_pair(r, buf + sizeof(uint32_t));
        }

        mutable_buffer_t put_uint64(boost::asio::mutable_buffer buf, uint64_t val) {
            auto b = boost::asio::buffer_cast<octet_t*>(buf);
            b[0] = static_cast<octet_t>(((val) >> 56) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 48) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 40) & 0xff);
            b[3] = static_cast<octet_t>(((val) >> 32) & 0xff);
            b[4] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[5] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[6] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[7] = static_cast<octet_t>(val & 0xff);
            return buf + sizeof(val);
        }

        result_t<uint64_t> get_uint64(boost::asio::const_buffer buf) {
            BOOST_ASSERT(boost::asio::buffer_size(buf) >= sizeof(uint64_t));

            auto b = boost::asio::buffer_cast<octet_t const*>(buf);
            auto const r = static_cast<uint64_t>(b[0]) << 56 |
                           static_cast<uint64_t>(b[1]) << 48 |
                           static_cast<uint64_t>(b[2]) << 40 |
                           static_cast<uint64_t>(b[3]) << 32 |
                           static_cast<uint64_t>(b[4]) << 24 |
                           static_cast<uint64_t>(b[5]) << 16 |
                           static_cast<uint64_t>(b[6]) << 8  |
                           static_cast<uint64_t>(b[7]);
            return std::make_pair(r, buf + sizeof(uint64_t));
        }

    } // namespace wire
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_DETAIL_WIRE_HPP
