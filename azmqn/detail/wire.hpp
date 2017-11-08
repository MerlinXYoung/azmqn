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

    namespace codec {
        template<typename T>
        constexpr void put(octet_t*, T);

        template<>
        constexpr void put<uint8_t>(octet_t* b, uint8_t val) { b[0] = val; }

        template<>
        constexpr void put<uint16_t>(octet_t* b, uint16_t val) {
            b[0] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[1] = static_cast<octet_t>(val & 0xff);
        }

        template<>
        constexpr void put<uint32_t>(octet_t* b, uint32_t val) {
            b[0] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[3] = static_cast<octet_t>(val & 0xff);
        }

        template<>
        constexpr void put<uint64_t>(octet_t* b, uint64_t val) {
            b[0] = static_cast<octet_t>(((val) >> 56) & 0xff);
            b[1] = static_cast<octet_t>(((val) >> 48) & 0xff);
            b[2] = static_cast<octet_t>(((val) >> 40) & 0xff);
            b[3] = static_cast<octet_t>(((val) >> 32) & 0xff);
            b[4] = static_cast<octet_t>(((val) >> 24) & 0xff);
            b[5] = static_cast<octet_t>(((val) >> 16) & 0xff);
            b[6] = static_cast<octet_t>(((val) >> 8) & 0xff);
            b[7] = static_cast<octet_t>(val & 0xff);
        }

        template<typename T>
        constexpr T get(octet_t const*);

        template<>
        constexpr uint8_t get(octet_t const* b) { return *b; }

        template<>
        constexpr uint16_t get<uint16_t>(octet_t const* b) {
            return static_cast<uint16_t>(b[0]) << 8 |
                   static_cast<uint16_t>(b[1]);
        }

        template<>
        constexpr uint32_t get<uint32_t>(octet_t const* b) {
            return static_cast<uint32_t>(b[0]) << 24 |
                   static_cast<uint32_t>(b[1]) << 16 |
                   static_cast<uint32_t>(b[2]) << 8  |
                   static_cast<uint32_t>(b[3]);
        }

        template<>
        constexpr uint64_t get<uint64_t>(octet_t const* b) {
            return static_cast<uint64_t>(b[0]) << 56 |
                   static_cast<uint64_t>(b[1]) << 48 |
                   static_cast<uint64_t>(b[2]) << 40 |
                   static_cast<uint64_t>(b[3]) << 32 |
                   static_cast<uint64_t>(b[4]) << 24 |
                   static_cast<uint64_t>(b[5]) << 16 |
                   static_cast<uint64_t>(b[6]) << 8  |
                   static_cast<uint64_t>(b[7]);
        }

        constexpr bool const_expr_check() {
            std::array<octet_t, sizeof(uint64_t)> buf{ 0, 0, 0, 0, 0, 0, 0, 0};
            auto v1 = 12345678ul;
            put(buf.data(), v1);
            auto v2 = get<decltype(v1)>(buf.data());
            return true;
        }
        static_assert(const_expr_check());
    } // namespace codec 

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

        octet_t const* data() const
        { return boost::asio::buffer_cast<octet_t const*>(b_); }

        octet_t const& front() { return *data(); }

        octet_t const& operator[](unsigned i) const
        {
            BOOST_ASSERT(i < boost::asio::buffer_size(b_));
            return data()[i];
        }

        Buffer consume() const
        { return b_ + min_size; }

    protected:
        Buffer b_;
    };

    template<size_t min_size>
    struct at_least_mutable_buffer : at_least_buffer<mutable_buffer_t, min_size> {
        using base_type = at_least_buffer<mutable_buffer_t, min_size>;
        using base_type::base_type;

        octet_t* data()
        { return boost::asio::buffer_cast<octet_t*>(base_type::b_); }

        octet_t& front() {  return *data(); }

        octet_t& operator[](unsigned i) {
            BOOST_ASSERT(i < boost::asio::buffer_size(base_type::b_));
            return data()[i];
        }
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

        template<typename T>
        mutable_buffer_t put(at_least_mutable_buffer<sizeof(T)> b, T val) {
            codec::put(b.data(), val);
            return b.consume();
        }

        template<typename T>
        result_t<T> get(at_least_const_buffer<sizeof(T)> b) {
            return std::make_pair(codec::get<T>(b.data()), b.consume());
        }
    } // namespace wire
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_DETAIL_WIRE_HPP
