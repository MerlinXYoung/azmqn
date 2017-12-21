/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_WIRE_HPP
#define AZMQN_DETAIL_WIRE_HPP

#include "../asio/buffer.hpp"
#include "../asio/read.hpp"

#include <boost/assert.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/system/error_code.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/range/adaptor/transformed.hpp>
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
    constexpr uint8_t default_vmajor = 0x03;
    constexpr uint8_t default_vminor = 0x01;

    namespace wire {
        struct mechanism_name {
            static constexpr auto max_size = 20ul;

            mechanism_name(boost::string_view val) noexcept
                : val_{ val }
            { BOOST_ASSERT(val_.size() <= max_size); }

            boost::string_view value() const { return val_; }
            template<typename Iterator>
            Iterator fill(Iterator it) {
                auto filler = max_size - val_.size();
                return std::fill_n(it, filler, utility::octet(0));
            }

            bool operator==(mechanism_name const& lhs) const { return val_ == lhs.val_; }
            bool operator<(mechanism_name const& lhs) const { return val_ < lhs.val_; }
            bool operator>(mechanism_name const& lhs) const { return val_ > lhs.val_; }
            bool operator<=(mechanism_name const& lhs) const { return val_ <= lhs.val_; }
            bool operator>=(mechanism_name const& lhs) const { return val_ >= lhs.val_; }
            bool operator!=(mechanism_name const& lhs) const { return val_ != lhs.val_; }
        private:
            boost::string_view val_;
        };

        utility::octet as_server(bool v) { return utility::octet(as_server ?  0x1 : 0x0); }

        struct greeting {
            static constexpr auto size = 64;
            using version_t = std::underlying_type_t<utility::octet>;

            greeting(mechanism_name mechanism, bool is_server,
                         version_t vmajor = default_vmajor,
                         version_t vminor = default_vminor) noexcept {
                using namespace boost::adaptors;
                using namespace utility;
                static constexpr auto tfn = [](auto x) { return utility::octet(x); };
                auto it = boost::copy(signature | transformed(tfn), std::begin(buf_));

                *it++ = octet(vmajor);
                *it++ = octet(vminor);

                it = boost::copy(mechanism.value() | transformed(tfn), it);

                it = mechanism.fill(it);

                *it++ = as_server(is_server);
                boost::fill(boost::make_iterator_range(it, std::end(buf_)), octet(0));
            }

            greeting(boost::asio::const_buffer buf) noexcept
            { boost::copy(asio::buffer_range(buf), std::begin(buf_)); }

            boost::asio::const_buffer buffer() const { return boost::asio::buffer(buf_); }

            bool valid() const {
                auto [r, _] = boost::range::mismatch(signature, buf_,
                                    [](auto const& x, auto const& y) {
                                        return utility::octet(x) == y;
                                    });
                return r == std::end(signature);
            }

            using version_result_t = std::pair<version_t, version_t>;
            version_result_t version() const {
                BOOST_ASSERT(valid());
                auto const it = std::begin(buf_) + signature.size();
                return std::make_pair(static_cast<version_t>(*it), static_cast<version_t>(*(it + 1)));
            }

            mechanism_name mechanism() const {
                auto const it = std::begin(buf_) + signature.size() + 2;
                auto const c = reinterpret_cast<char const*>(&*it);
                auto len = std::min(::strlen(c), mechanism_name::max_size);
                return boost::string_view(c, len);
            }

            bool is_server() const {
                auto const it = std::begin(buf_) + signature.size() + 2 + mechanism_name::max_size;
                return *it == utility::octet(0x1);
            }

            friend std::ostream& operator<<(std::ostream& stm, greeting const& that) {
                using namespace asio;
                return stm << that.buffer();
            }

        private:
            static constexpr auto signature_size = 10;
            static constexpr auto filler_size = 31;
            static constexpr std::array<std::underlying_type_t<utility::octet>, signature_size> signature =
                { 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7f };

            std::array<utility::octet, size> buf_;
        };

        enum class flags : std::underlying_type_t<utility::octet> {
            is_message = 0x0,
            is_more = 0x1,
            is_long = 0x2,
            is_command = 0x4,
            none = 0x0
        };

        constexpr utility::octet operator+(flags f) noexcept
        { return utility::octet(static_cast<std::underlying_type_t<flags>>(f)); }

        constexpr bool is_set(utility::octet o, flags f) noexcept
        { return utility::to_integer<bool>(o & +f); }

        constexpr static bool is_more(utility::octet o) noexcept
        { return is_set(o, flags::is_more); }

        constexpr static bool is_long(utility::octet o) noexcept
        { return is_set(o, flags::is_long); }

        constexpr static bool is_command(utility::octet o) noexcept
        { return is_set(o, flags::is_command); }

        // ZMTP uses network byte order
        template<typename T>
        using endian_buffer_t = boost::endian::endian_buffer<
                                      boost::endian::order::big
                                    , T
                                    , sizeof(T) * CHAR_BIT>;

        // ZMTP only defines byte order for unsigned types
        template<typename T>
        auto put(asio::at_least_mutable_buffer<sizeof(T)> b, T val) noexcept
            -> typename std::enable_if_t<std::is_unsigned_v<T>, boost::asio::mutable_buffer> {
            *reinterpret_cast<endian_buffer_t<T>*>(b.data()) = val;
            return b.consume();
        }

        template<typename T>
        using get_result_t = std::pair<T, boost::asio::const_buffer>;

        // ZMTP only defines byte order for unsigned types
        template<typename T>
        auto get(asio::at_least_const_buffer<sizeof(T)> b) noexcept
            -> typename std::enable_if_t<std::is_unsigned_v<T>, get_result_t<T>> {
            auto eb = reinterpret_cast<endian_buffer_t<T> const*>(b.data());
            return std::make_pair(eb->value(), b.consume());
        }

        constexpr auto min_framing_octets = 1 + sizeof(utility::octet);
        constexpr auto max_framing_octets = 1 + sizeof(uint64_t);

        static_assert(min_framing_octets > sizeof(utility::octet));
        static_assert(max_framing_octets > sizeof(uint64_t));

        constexpr auto small_size = 64;
        constexpr auto max_small_size = std::numeric_limits<std::underlying_type_t<utility::octet>>::max();
        using buffer_t = asio::backed_buffer<small_size>;

        struct frame {
            frame()
            { framing_[0] = utility::octet(0); }

            size_t bytes_transferred() const noexcept { return bytes_transferred_; }

            bool empty() const noexcept { return bytes_transferred_ == 0; }

            boost::asio::mutable_buffer mutable_buffer() noexcept {
                return boost::asio::buffer(framing_);
            }

            boost::asio::const_buffer const_buffer() const noexcept {
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
            std::array<utility::octet, max_framing_octets> framing_;
            size_t bytes_transferred_ = 0;

            template<typename SyncReadStream>
            asio::read_result_type read_until_long_framed(SyncReadStream& s) {
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
