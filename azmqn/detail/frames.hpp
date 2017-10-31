/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_FRAMES_HPP
#define AZMQN_DETAIL_FRAMES_HPP

#include "wire.hpp"
#include "buffer_base.hpp"
#include <boost/variant.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <array>

namespace  azmqn::detail::transport {
    struct framing {
        framing() = default;

        using command_t = wire::command_t;
        using message_t = wire::message_t;

        // Initialize from a command
        framing(command_t c)
            : res_(std::move(c))
        { }

        // Initialize from a message
        framing(message_t m)
            : res_(std::move(m))
        { }

        struct none_t { };
        using result_t = boost::variant<none_t, command_t, message_t>;

        template<typename SyncReadStream>
        static result_t read(SyncReadStream& s, boost::system::error_code& ec) {
            framing f;
            f.read_until_framed(s, ec);
            if (ec) {
                return result_t{};
            }

            auto b = f.init_buffer();
            boost::asio::read(s, boost::asio::buffer(b), ec);
            if (ec) {
                return result_t{};
            }
            return std::move(f.res_);
        }

        template<typename SyncWriteStream,
                 typename Writable>
        static size_t write(SyncWriteStream& s, Writable const& w,
                            boost::system::error_code& ec) {
            framing f(w, ref_only);
            return boost::asio::write(s, f.get_write_buffers(w.buf), ec);
        }

        // The framing argument must remain valid until the completion handler
        // is called. Completion handler's signature is -
        //      void handler(boost::system::error_code const&, result_t)
        template<typename AsyncReadStream,
                 typename CompletionHandler>
        static void async_read(AsyncReadStream& s, framing& f,
                               CompletionHandler handler) {
            BOOST_ASSERT(f.empty());
            f.async_read_until_framed(s,
                [&, handler{ std::move(handler) }](auto const& ec, size_t) {
                    if (ec) {
                        handler(ec, none_t{ });
                    } else {
                        auto b = f.init_buffer();
                        boost::asio::async_read(s, boost::asio::buffer(b),
                            [&, handler{ std::move(handler) }](auto const& ec, size_t) {
                            if (ec) {
                                handler(ec, result_t{});
                            } else {
                                handler(ec, std::move(f.res_));
                                f.res_ = none_t{};
                            }
                        });
                    }
            });
        }

        // The framing argument must remain valid until the completion handler
        // is called. Completion handler's signature is the same as for
        // boost::asio::async_write()
        template<typename AsyncWriteStream,
                 typename CompletionHandler>
        static void async_write(AsyncWriteStream& s, framing const& f,
                                CompletionHandler&& handler) {
            BOOST_ASSERT(!f.empty());
            boost::asio::async_write(s, f.get_write_buffers(),
                                     std::forward<CompletionHandler>(handler));
        }

    private:
        wire::framing_t framing_;
        size_t bytes_transferred_ = 0;
        result_t res_;

        static constexpr struct ref_only_t { } ref_only;

        template<typename BufferBase>
        framing(BufferBase const& b, ref_only_t)
        { b.set_frame_size(boost::asio::buffer(framing_)); }

        mutable_buffer_t mutable_framing_buffer() {
            return boost::asio::buffer(&framing_[0], wire::min_framing_octets);
        }

        const_buffer_t const_framing_buffer() const {
            return boost::asio::buffer(&framing_[0], is_long()
                        ? wire::max_framing_octets
                        : wire::min_framing_octets);
        }

        mutable_buffer_t long_mutable_framing_buffer() {
            static const auto long_size = wire::max_framing_octets - wire::min_framing_octets;
            return boost::asio::buffer(&framing_[wire::min_framing_octets],
                                        long_size);
        }

        bool empty() const { return bytes_transferred_ == 0; }
        bool valid() const {
            if (empty()) {
                return false;
            }

            if (is_long() && bytes_transferred_ < wire::max_framing_octets) {
                return false;
            }

            return true;
        }

        bool is_more() const {
            BOOST_ASSERT(!empty());
            return wire::is_more(framing_[0]);
        }

        bool is_long() const {
            BOOST_ASSERT(!empty());
            return wire::is_long(framing_[0]);
        }

        bool is_command() const {
            BOOST_ASSERT(!empty());
            return wire::is_command(framing_[0]);
        }

        size_t framed_length() const {
            BOOST_ASSERT(valid());
            size_t len;
            if (is_long()) {
                std::tie(len, std::ignore) = wire::get_uint64(
                        boost::asio::buffer(&framing_[1], sizeof(uint64_t)));
            } else {
                std::tie(len, std::ignore) = wire::get_uint8(
                        boost::asio::buffer(&framing_[1], sizeof(uint8_t)));
            }
            return len;
        }

        struct mutable_buffer_visitor
            : boost::static_visitor<mutable_buffer_t> {
            mutable_buffer_t operator()(none_t &) const
            { return mutable_buffer_t(); }

            template<typename T>
            mutable_buffer_t operator()(T& t) const
            { return wire::buffer(t.buf); }
        };

        mutable_buffer_t init_buffer() {
            BOOST_ASSERT(valid());
            if (is_command()) {
                res_ = command_t{ framed_length() };
            } else {
                res_ = message_t{ framed_length(), is_more() };
            }
            mutable_buffer_visitor v;
            return boost::apply_visitor(v, res_);
        }

        struct set_buffer_size_visitor
            : boost::static_visitor<mutable_buffer_t> {
            mutable_buffer_t framing_;
            set_buffer_size_visitor(wire::framing_t& framing)
                : framing_(boost::asio::buffer(framing)) { }

            mutable_buffer_t operator()(none_t &) const
            { return mutable_buffer_t(); }

            template<typename T>
            mutable_buffer_t operator()(T& t) const
            { return t.set_frame_size(framing_); }
        };

        mutable_buffer_t set_frame_size() {
            set_buffer_size_visitor v(framing_);
            return boost::apply_visitor(v, res_);
        }

        using write_bufs_t = std::array<const_buffer_t, 2>;
        write_bufs_t get_write_buffers(wire::buffer_t const& buf) const {
            return write_bufs_t{ const_framing_buffer(), wire::buffer(buf) };
        }

        struct const_buffer_visitor
            : boost::static_visitor<const_buffer_t> {
            const_buffer_t operator()(none_t const&) const
            { return const_buffer_t(); }

            template<typename T>
            const_buffer_t operator()(T const& t) const
            { return wire::buffer(t.buf); }
        };

        write_bufs_t get_write_buffers() const {
            const_buffer_visitor v;
            return write_bufs_t{
                const_framing_buffer(),
                boost::apply_visitor(v, res_)
            };
        }

        template<typename SyncReadStream>
        size_t read_until_framed(SyncReadStream& s,
                                 boost::system::error_code& ec) {
            BOOST_ASSERT(empty());
            bytes_transferred_ = boost::asio::read(s, boost::asio::buffer(mutable_framing_buffer()), ec);
            if (ec) {
                return 0;
            }

            if (is_long()) {
                bytes_transferred_ += boost::asio::read(s, boost::asio::buffer(long_mutable_framing_buffer()), ec);
                if (ec) {
                    return 0;
                }
            }
            return framed_length();
        }

        template<typename AsyncReadStream,
                 typename CompletionHandler>
        void async_read_until_long_framed(AsyncReadStream& s,
                                          CompletionHandler&& handler) {
            BOOST_ASSERT(!empty());

            boost::asio::async_read(s, boost::asio::buffer(long_mutable_framing_buffer()),
                [&, handler{ std::move(handler) }](auto const& ec,
                                                   size_t bytes_transferred) {
                                        if (ec) {
                                            handler(ec, 0);
                                        } else {
                                            bytes_transferred_ += bytes_transferred;
                                            handler(ec, framed_length());
                                        }
                                    });
        }

        template<typename AsyncReadStream,
                 typename CompletionHandler>
        void async_read_until_framed(AsyncReadStream& s, CompletionHandler handler) {
            BOOST_ASSERT(empty());

            boost::asio::async_read(s, boost::asio::buffer(mutable_framing_buffer()),
                [&, handler{ std::move(handler) }](auto const& ec,
                                                   size_t bytes_transferred) {
                                        if(ec) {
                                            handler(ec, 0);
                                            return;
                                        }

                                        bytes_transferred_ = bytes_transferred;
                                        if (is_long()) {
                                            async_read_until_long_framed(s, std::move(handler));
                                        } else {
                                            handler(ec, framed_length());
                                        }
                                    });
        }
    };
} // namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_FRAMES_HPP
