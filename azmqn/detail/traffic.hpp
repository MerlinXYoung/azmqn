/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_TRAFFIC_HPP
#define AZMQN_DETAIL_TRAFFIC_HPP

#include "wire.hpp"
#include "metadata.hpp"

#include <boost/hana.hpp>
#include <boost/variant.hpp>

#include <type_traits>

namespace  azmqn::detail::transport {
    namespace wire {
        // Base type for traffic
        template<typename T, flags selector>
        struct traffic : buffer_t {
            using self_type = T;
            metadata meta;

            using buffer_t::buffer_t;

            mutable_buffer_t set_framing(at_least_mutable_buffer<sizeof(max_framing_octets)> b) const noexcept {
                *b.data() = tag_type::value | get_flags() | is_long();
                auto bb = b.consume();
                return is_long() ? put<uint64_t>(bb, size())
                                 : put<uint8_t>(bb, size());
            }

        private:
            using flags_type = std::underlying_type<flags>::type;
            using tag_type = std::integral_constant<flags_type, +selector>;

            static constexpr auto has_flags = boost::hana::is_valid(
                                    [](auto&& x) -> decltype(x.get_flags()) { });

            self_type const& self() const noexcept
            { return *reinterpret_cast<self_type const*>(this); }

            self_type& self() noexcept
            { return *reinterpret_cast<self_type*>(this); }

            value_type get_flags() const noexcept {
                return boost::hana::if_(has_flags(self()),
                        [&](auto&& x) { return static_cast<value_type>(x.get_flags()); },
                        [](auto&&) { return 0; })
                    (self());
            }

            value_type is_long() const noexcept {
                return size() > max_small_size ? +flags::is_long
                                               : +flags::none;
            }
        };

        struct command_t : traffic<command_t, flags::is_command>
        { using traffic::traffic; };

        struct message_t : traffic<message_t, flags::is_message> {
            bool more;
            message_t(size_t len, bool m)
                : traffic(len)
                , more(m)
            { }

            message_t(const_buffer_t b, bool m)
                : traffic(b)
                , more(m)
            { }

            flags get_flags() const noexcept
            { return more ? flags::is_more : flags::none; }
        };

        using message_or_command = boost::variant<message_t, command_t>;
        struct maybe_message_or_command {
            maybe_message_or_command() = default;
            maybe_message_or_command(command_t c) noexcept
                : val_{ std::move(c) }
            { }

            maybe_message_or_command(message_t m) noexcept
                : val_{ std::move(m) }
            { }

            bool empty() const noexcept { return val_.which() == 0; } 

            const_buffer_t const_buffer() const noexcept {
                const_buffer_visitor v;
                return boost::apply_visitor(v, val_);
            }

            mutable_buffer_t mutable_buffer() noexcept {
                mutable_buffer_visitor v;
                return boost::apply_visitor(v, val_);
            }

            mutable_buffer_t set_framing(mutable_buffer_t b) const noexcept {
                set_framing_visitor v(b);
                return boost::apply_visitor(v, val_);
            }

            message_or_command detach() {
                BOOST_ASSERT(!empty());
                message_or_command_visitor v;
                return boost::apply_visitor(v, val_);
            }

            static
            maybe_message_or_command from_frame(wire::frame const& f) {
                BOOST_ASSERT(f.valid());
                return f.is_command() ? maybe_message_or_command{ command_t{ f.size() } }
                                      : maybe_message_or_command{ message_t{ f.size(), f.is_more() } };
            }

        private:
            struct none_t { };
            using value_type =  boost::variant<
                  none_t
                , command_t
                , message_t>;
            value_type val_;

            struct const_buffer_visitor
                : boost::static_visitor<const_buffer_t> {
                const_buffer_t operator()(none_t const&) const
                { return const_buffer_t{ }; }

                template<typename T>
                const_buffer_t operator()(T const& t) const
                { return t.const_buffer(); }
            };

            struct mutable_buffer_visitor
                : boost::static_visitor<mutable_buffer_t> {
                mutable_buffer_t operator()(none_t &) const
                { return mutable_buffer_t(); }

                template<typename T>
                mutable_buffer_t operator()(T& t) const
                { return t.mutable_buffer(); }
            };

            struct set_framing_visitor
                : boost::static_visitor<mutable_buffer_t> {
                mutable_buffer_t framing_;

                set_framing_visitor(mutable_buffer_t framing)
                    : framing_(framing) { }

                mutable_buffer_t operator()(none_t const&) const
                { return mutable_buffer_t(); }

                template<typename T>
                mutable_buffer_t operator()(T const& t) const
                { return t.set_framing(framing_); }
            };

            struct message_or_command_visitor
                : boost::static_visitor<message_or_command> {
                message_or_command operator()(none_t&) const {
                    throw std::logic_error("no message or command");
                }

                message_or_command operator()(message_t& m) const {
                    return std::move(m);
                }

                message_or_command operator()(command_t& c) const {
                    return std::move(c);
                }
            };
        };

        struct writable_message_or_command {
            using bufs_t = std::array<const_buffer_t, 2>;

            writable_message_or_command(maybe_message_or_command data)
                : data_{ std::move(data) }
            { }

            bool empty() const { data_.empty(); }

            bufs_t buffer_sequence() const {
                wire::frame f;
                data_.set_framing(f.mutable_buffer());
                return bufs_t{ f.const_buffer(), data_.const_buffer() };
            }

        private:
            maybe_message_or_command data_;
        };

        struct readable_message_or_command {
            readable_message_or_command() = default;

            bool empty() const { return f_.empty() && data_.empty(); }
            frame& framing() { return f_; }

            mutable_buffer_t mutable_buffer() {
                BOOST_ASSERT(f_.valid());
                data_ = maybe_message_or_command::from_frame(f_);
                return data_.mutable_buffer();
            }

            message_or_command detach() { return data_.detach(); }

        private:
            wire::frame f_;
            maybe_message_or_command data_;
        };
    } // namespace wire
} // namespace namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_TRAFFIC_HPP
