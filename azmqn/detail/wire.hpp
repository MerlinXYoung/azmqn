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
    using mutable_buffers_1_t = boost::asio::mutable_buffers_1;
    using const_buffer_t = boost::asio::const_buffer;
    using const_buffers_1_t = boost::asio::const_buffers_1;

    template<typename Buffer, size_t min_size>
    struct at_least_buffer {
        at_least_buffer(Buffer b) : b_(b)
        { BOOST_ASSERT(boost::asio::buffer_size(b) >= min_size); }

        at_least_buffer(mutable_buffers_1_t b) : b_{ *std::begin(b) }
        { }

        at_least_buffer(const_buffers_1_t b) : b_{ *std::begin(b) }
        { }

        Buffer& operator*() { return b_; }
        Buffer const& operator*() const { return b_; }

        octet_t const& front() { return *octets(); }

        octet_t const& operator[](unsigned i) const
        {
            BOOST_ASSERT(i < boost::asio::buffer_size(b_));
            return octets()[i];
        }

        Buffer consume() const
        { return b_ + min_size; }

    protected:
        Buffer b_;

        octet_t const* octets() const
        { return boost::asio::buffer_cast<octet_t const*>(b_); }
    };

    template<size_t min_size>
    struct at_least_mutable_buffer : at_least_buffer<mutable_buffer_t, min_size> {
        using base_type = at_least_buffer<mutable_buffer_t, min_size>;
        using base_type::base_type;

        octet_t& front() {  return *octets(); }

        octet_t& operator[](unsigned i) {
            BOOST_ASSERT(i < boost::asio::buffer_size(base_type::b_));
            return octets()[i];
        }

    protected:
        octet_t* octets()
        { return boost::asio::buffer_cast<octet_t*>(base_type::b_); }
    };

    template<size_t min_size>
    struct at_least_const_buffer : at_least_buffer<const_buffer_t, min_size> {
        using base_type = at_least_buffer<const_buffer_t, min_size>;
        using base_type::base_type;
    };

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

        mutable_buffer_t put_uint8(at_least_mutable_buffer<sizeof(uint8_t)> b,
                                   uint8_t val) {
            b[0] = val;
            return b.consume();
        }

        result_t<uint8_t> get_uint8(at_least_const_buffer<sizeof(uint8_t)> b) {
            return std::make_pair(b.front(), b.consume());
        }

        mutable_buffer_t put_uint16(at_least_mutable_buffer<sizeof(uint16_t)> b,
                                    uint16_t val) {;
            b[0] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[1] = static_cast<octet_t>(val & 0xff);
            return b.consume();
        }

        result_t<uint16_t> get_uint16(at_least_const_buffer<sizeof(uint16_t)> b) {
            auto const r = static_cast<uint16_t>(b[0]) << 8 |
                           static_cast<uint16_t>(b[1]);
            return std::make_pair(r, b.consume());
        }

        mutable_buffer_t put_uint32(at_least_mutable_buffer<sizeof(uint32_t)> b,
                                    uint32_t val) {
            b[0] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[3] = static_cast<octet_t>(val & 0xff);
            return b.consume();
        }

        result_t<uint32_t> get_uint32(at_least_const_buffer<sizeof(uint32_t)> b) {
            auto const r = static_cast<uint32_t>(b[0]) << 24 |
                           static_cast<uint32_t>(b[1]) << 16 |
                           static_cast<uint32_t>(b[2]) << 8  |
                           static_cast<uint32_t>(b[3]);
            return std::make_pair(r, b.consume());
        }

        mutable_buffer_t put_uint64(at_least_mutable_buffer<sizeof(uint64_t)> b,
                                    uint64_t val) {
            b[0] = static_cast<octet_t>(((val) >> 56) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 48) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 40) & 0xff);
            b[3] = static_cast<octet_t>(((val) >> 32) & 0xff);
            b[4] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[5] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[6] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[7] = static_cast<octet_t>(val & 0xff);
            return b.consume();
        }

        result_t<uint64_t> get_uint64(at_least_const_buffer<sizeof(uint64_t)> b) {
            auto const r = static_cast<uint64_t>(b[0]) << 56 |
                           static_cast<uint64_t>(b[1]) << 48 |
                           static_cast<uint64_t>(b[2]) << 40 |
                           static_cast<uint64_t>(b[3]) << 32 |
                           static_cast<uint64_t>(b[4]) << 24 |
                           static_cast<uint64_t>(b[5]) << 16 |
                           static_cast<uint64_t>(b[6]) << 8  |
                           static_cast<uint64_t>(b[7]);
            return std::make_pair(r, b.consume());
        }
    } // namespace wire
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_DETAIL_WIRE_HPP
