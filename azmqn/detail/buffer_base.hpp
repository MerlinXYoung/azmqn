/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_BUFFER_BASE_HPP
#define AZMQN_DETAIL_BUFFER_BASE_HPP

#include "wire.hpp"
#include "metadata.hpp"

#include <boost/hana.hpp>

#include <type_traits>

namespace  azmqn::detail::transport {
    namespace wire {
        struct buffer_base_t {
            buffer_t buf;
            metadata meta;

            buffer_base_t(size_t len)
                : buf(len)
            { }

            buffer_base_t(const_buffer_t b)
                : buf(boost::asio::buffer_size(b))
            {
                auto p = boost::asio::buffer_cast<octet_t const*>(b);
                buf.assign(p, p+boost::asio::buffer_size(b));
            }

            mutable_buffer_t buffer() { return wire::buffer(buf); }
            const_buffer_t buffer() const { return wire::buffer(buf); }
        };

        template<typename T, flags selector>
        struct buffer_type_t : buffer_base_t {
            using self_type = T;

            using buffer_base_t::buffer_base_t;

            mutable_buffer_t set_frame_size(at_least_mutable_buffer<sizeof(max_framing_octets)> b) const {
                b.front() = tag_type::value | get_flags() | is_long();
                auto bb = b.consume();
                return is_long() ? put_uint64(bb, buf.size())
                                 : put_uint8(bb, buf.size());
            }

        private:
            using flags_type = std::underlying_type<flags>::type;
            using tag_type = std::integral_constant<flags_type, +selector>;

            static constexpr auto has_flags = boost::hana::is_valid(
                                    [](auto&& x) -> decltype(x.get_flags()) { });

            self_type const& self() const
            { return *reinterpret_cast<self_type const*>(this); }

            self_type& self()
            { return *reinterpret_cast<self_type*>(this); }

            using el_t = typename decltype(buf)::value_type;
            el_t get_flags() const {
                return boost::hana::if_(has_flags(self()),
                        [&](auto&& x) { return static_cast<el_t>(x.get_flags()); },
                        [](auto&&) { return 0; })
                    (self());
            }

            el_t is_long() const {
                return buf.size() > max_small_size ? +flags::is_long
                                                   : +flags::none;
            }
        };

        struct command_t : buffer_type_t<command_t, flags::is_command>
        { using buffer_type_t::buffer_type_t; };

        struct message_t : buffer_type_t<message_t, flags::is_message> {
            bool more;
            message_t(size_t len, bool m)
                : buffer_type_t(len)
                , more(m)
            { }

            message_t(const_buffer_t b, bool m)
                : buffer_type_t(b)
                , more(m)
            { }

            flags get_flags() const
            { return more ? flags::is_more : flags::none; }
        };
    } // namespace wire
} // namespace namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_BUFFER_BASE_HPP

