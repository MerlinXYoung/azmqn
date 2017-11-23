/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_FRAMED_IO_HPP
#define AZMQN_DETAIL_FRAMED_IO_HPP

#include "wire.hpp"
#include "traffic.hpp"

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <array>

namespace  azmqn::detail::transport {
    template<typename SyncReadStream>
    wire::message_or_command read(SyncReadStream& s, boost::system::error_code& ec) {
        wire::readable_message_or_command r;
        r.framing().read(s, ec);
        if (ec) {
            return wire::message_or_command{};
        }
        boost::asio::read(s, r.mutable_buffer(), ec);
        if (ec) {
            return wire::message_or_command{};
        }
        return r.detach();
    }

    template<typename SyncWriteStream>
    size_t write(SyncWriteStream& s, wire::writable_message_or_command const& w,
                        boost::system::error_code& ec) {
        return boost::asio::write(s, w.buffer_sequence(), ec);
    }

    // The readable_message_or_command argument must remain valid until the
    // completion handler is called.
    // Completion handler's signature is -
    //      void handler(boost::system::error_code const&,
    //                   wire::readable_message_or_command&)
    template<typename AsyncReadStream,
             typename CompletionHandler>
    void async_read(AsyncReadStream& s, wire::readable_message_or_command& r,
                        CompletionHandler handler) {
        BOOST_ASSERT(r.empty());
        auto& framing = r.framing();
        framing.async_read(s,
            [&, handler{ std::move(handler) }](auto const& ec, size_t) {
                if (ec) {
                    handler(ec, r);
                } else {
                    boost::asio::async_read(s, boost::asio::buffer(r.mutable_buffer()),
                        [&, handler{ std::move(handler) }](auto const& ec, size_t) {
                            handler(ec, r);
                        });
                }
            });
    }

    // The writable_message_or_command argument must remain valid until the
    // completion handler is called. Completion handler's signature is the same
    // as for boost::asio::async_write()
    template<typename AsyncWriteStream,
             typename CompletionHandler>
    void async_write(AsyncWriteStream& s, wire::writable_message_or_command const& w,
                        CompletionHandler&& handler) {
        BOOST_ASSERT(!w.empty());
        boost::asio::async_write(s, w.buffer_sequence(),
                                 std::forward<CompletionHandler>(handler));
    }
} // namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_FRAMED_IO_HPP
