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
            using type = T;
            using tag_type = std::integral_constant<std::underlying_type<flags>::type, +selector>;

            using buffer_base_t::buffer_base_t;

            mutable_buffer_t set_frame_size(mutable_buffer_t b) const {
                using el_t = typename decltype(buf)::value_type;
                auto f = boost::asio::buffer_cast<el_t*>(b);
                auto const lf = +(buf.size() > max_small_size ? flags::is_long : flags::none);
                f[0] = tag_type::value
                       | boost::hana::if_(has_flags(self()),
                                 [&](auto&& x) { return static_cast<el_t>(x.get_flags()); },
                                 [](auto&&) { return 0; })
                            (self())
                       | lf;
                b + sizeof(el_t);
                return lf ? put_uint64(b, buf.size())
                          : put_uint8(b, buf.size());
            }

        private:
            static constexpr auto has_flags = boost::hana::is_valid([](auto&& x) -> decltype(x.get_flags()) { });

            type const& self() const { return *reinterpret_cast<type const*>(this); }
            type& self() { return *reinterpret_cast<type*>(this); }
        };

        struct command_t : buffer_type_t<command_t, flags::is_command>
        { using buffer_type_t::buffer_type_t; };

        struct message_t : buffer_type_t<message_t, flags::is_message> {
            bool more;
            message_t(size_t len, bool m)
                : buffer_type_t(len)
                , more(m)
            { }

            flags get_flags() const { return more ? flags::is_more : flags::none; }
        };
    } // namespace wire
} // namespace namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_BUFFER_BASE_HPP

