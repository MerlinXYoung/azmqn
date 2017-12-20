/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_WIRE_HPP
#define AZMQN_DETAIL_WIRE_HPP

#include "buffer.hpp"
#include "../asio/read.hpp"

#include <boost/assert.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/system/error_code.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/mismatch.hpp>

#include <cstddef>
#include <cstring>
#include <utility>
#include <array>
#include <limits>
#include <type_traits>
#include <algorithm>

namespace  azmqn::detail::transport {
    namespace wire {
        constexpr auto max_mechanism_size = 20ul;

        struct greeting {
            static constexpr auto size = 64;

            greeting(boost::string_view mechanism, bool as_server,
                        octet_t vmajor = 0x03, octet_t vminor = 0x01) noexcept {
                auto it = boost::copy(signature, std::begin(buf_));
                *it++ = vmajor;
                *it++ = vminor;

                BOOST_ASSERT(mechanism.size() < max_mechanism_size);
                it = boost::copy(mechanism, it);

                it = std::fill_n(it, max_mechanism_size - mechanism.size(), 0);
                *it++ = as_server ?  0x1 : 0x0;
                boost::fill(boost::make_iterator_range(it, std::end(buf_)), 0);
            }

            greeting(const_buffer_t buf) noexcept {
                auto const it = buffer_data(buf);
                std::copy(it, it + size, std::begin(buf_));
            }

            const_buffer_t buffer() const { return boost::asio::buffer(buf_); }

            bool valid() const {
                auto [f, _] = boost::range::mismatch(signature, buf_);
                return f == std::end(signature);
            }

            using version_t = std::pair<octet_t, octet_t>;
            version_t version() const {
                BOOST_ASSERT(valid());
                auto const it = std::begin(buf_) + signature.size();
                return std::make_pair(*it, *(it + 1));
            }

            boost::string_view mechanism() const {
                auto const it = std::begin(buf_) + signature.size() + 2;
                auto const c = reinterpret_cast<char const*>(&*it);
                auto len = std::min(::strlen(c), max_mechanism_size); 
                return boost::string_view(c, len);
            }

            bool is_server() const {
                auto const it = std::begin(buf_) + signature.size() + 2 + max_mechanism_size;
                return *it == 0x1;
            }

            friend std::ostream& operator<<(std::ostream& stm, greeting const& that) {
                return stm << that.buffer();
            }

        private:
            static constexpr auto signature_size = 10;
            static constexpr auto filler_size = 31;
            static constexpr std::array<octet_t, signature_size> signature =
                { 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7f };

            std::array<octet_t, size> buf_;
        };

        enum class flags : octet_t {
            is_message = 0x0,
            is_more = 0x1,
            is_long = 0x2,
            is_command = 0x4,
            none = 0x0
        };

        constexpr octet_t operator+(flags f) noexcept
        { return static_cast<octet_t>(f); }

        constexpr bool is_set(octet_t o, flags f) noexcept
        { return o & +f; }

        constexpr static bool is_more(octet_t o) noexcept
        { return is_set(o, flags::is_more); }

        constexpr static bool is_long(octet_t o) noexcept
        { return is_set(o, flags::is_long); }

        constexpr static bool is_command(octet_t o) noexcept
        { return is_set(o, flags::is_command); }

        // ZMTP uses network byte order
        template<typename T>
        using endian_buffer_t = boost::endian::endian_buffer<
                                      boost::endian::order::big
                                    , T
                                    , sizeof(T) * CHAR_BIT>;

        // ZMTP only defines byte order for unsigned types
        template<typename T>
        auto put(at_least_mutable_buffer<sizeof(T)> b, T val) noexcept
            -> typename std::enable_if<std::is_unsigned<T>::value, mutable_buffer_t>::type {
            auto eb = reinterpret_cast<endian_buffer_t<T>*>(b.data());
            *eb = val;
            return b.consume();
        }

        template<typename T>
        using result_t = std::pair<T, const_buffer_t>;

        // ZMTP only defines byte order for unsigned types
        template<typename T>
        auto get(at_least_const_buffer<sizeof(T)> b) noexcept
            -> typename std::enable_if<std::is_unsigned<T>::value, result_t<T>>::type {
            auto eb = reinterpret_cast<endian_buffer_t<T> const*>(b.data());
            return std::make_pair(eb->value(), b.consume());
        }

        constexpr auto min_framing_octets = 1 + sizeof(octet_t);
        constexpr auto max_framing_octets = 1 + sizeof(uint64_t);

        static_assert(min_framing_octets > sizeof(octet_t));
        static_assert(max_framing_octets > sizeof(uint64_t));

        constexpr auto small_size = 64;
        constexpr auto max_small_size = std::numeric_limits<octet_t>::max();
        using buffer_t = backed_buffer<small_size>;

        struct frame {
            using framing_t = std::array<octet_t, max_framing_octets>;

            frame()
            { framing_[0] = 0; }

            size_t bytes_transferred() const noexcept { return bytes_transferred_; }

            bool empty() const noexcept { return bytes_transferred_ == 0; }

            mutable_buffer_t mutable_buffer() noexcept {
                return boost::asio::buffer(framing_);
            }

            const_buffer_t const_buffer() const noexcept {
                BOOST_ASSERT(!empty());
                return boost::asio::buffer(framing_.data(), is_long()
                            ? max_framing_octets
                            : min_framing_octets);
            }

            bool is_long() const noexcept {
                BOOST_ASSERT(!empty());
                return wire::is_long(framing_[0]);
            }

            bool valid() const noexcept {
                if (empty()) {
                    return false;
                }

                if (is_long() && bytes_transferred_ < wire::max_framing_octets) {
                    return false;
                }

                return true;
            }

            bool is_more() const noexcept {
                BOOST_ASSERT(!empty());
                return wire::is_more(framing_[0]);
            }

            bool is_command() const noexcept {
                BOOST_ASSERT(!empty());
                return wire::is_command(framing_[0]);
            }

            size_t size() const noexcept {
                BOOST_ASSERT(valid());
                size_t len;
                if (is_long()) {
                    std::tie(len, std::ignore) = wire::get<uint64_t>(
                            boost::asio::buffer(&framing_[1], sizeof(uint64_t)));
                } else {
                    std::tie(len, std::ignore) = wire::get<uint8_t>(
                            boost::asio::buffer(&framing_[1], sizeof(uint8_t)));
                }
                return len;
            }

            template<typename SyncReadStream>
            asio::read_result_type read(SyncReadStream& s) {
                BOOST_ASSERT(empty());
                auto rr = asio::read(s, boost::asio::buffer(framing_.data(), min_framing_octets));
                if (rr) {
                    bytes_transferred_ = *rr;
                    if (is_long()) {
                        rr = read_until_long_framed(s);
                    }
                }
                return rr;
            }

            template<typename AsyncReadStream,
                 typename CompletionHandler>
            void async_read(AsyncReadStream& s, CompletionHandler handler) {
                using namespace boost;
                BOOST_ASSERT(empty());

                asio::async_read(s, boost::asio::buffer(framing_.data(), min_framing_octets),
                                 [&, handler{ std::move(handler) }](asio::read_result_type rr) {
                                        if(rr) {
                                            bytes_transferred_ = *rr;
                                            if (is_long())
                                                rr = read_until_long_framed(s);
                                        }
                                        handler(rr);
                                    });
            }

        private:
            framing_t framing_;
            size_t bytes_transferred_ = 0;

            template<typename SyncReadStream>
            asio::read_result_type read_until_long_framed(SyncReadStream& s) {
                using namespace boost;
                static const auto long_size = max_framing_octets - min_framing_octets;
                auto res = asio::read(s, boost::asio::buffer(&framing_[min_framing_octets], long_size));
                if (res) {
                    bytes_transferred_ += *res;
                    return bytes_transferred_;
                }
                return res;
            }
        };

        struct read_limit {
            constexpr static int32_t max() { return std::numeric_limits<int32_t>::max(); }
            constexpr read_limit(int32_t val) noexcept
                : val_{ std::clamp(val, 0, max()) }
            { }

            using result_t = utility::expected<void, boost::system::error_code>;
            result_t check(frame const& f) const {
                if (f.size() > val_) {
                    using namespace boost::system;
                    return utility::make_unexpected(make_error_code(errc::message_size));
                }
                return result_t();
            }

        private:
            int32_t val_;
        };

    } // namespace wire
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_DETAIL_WIRE_HPP
