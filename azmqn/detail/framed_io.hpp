/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_FRAMED_IO_HPP
#define AZMQN_DETAIL_FRAMED_IO_HPP

#include "../utility/expected.hpp"
#include "wire.hpp"
#include "traffic.hpp"

#include "../asio/read.hpp"

#include <boost/asio/write.hpp>

#include <array>

namespace  azmqn::detail::transport {
    using message_or_command_result_type = utility::expected<
                                                              wire::message_or_command
                                                            , boost::system::error_code
                                                            >;
    template<typename SyncReadStream>
    message_or_command_result_type read(SyncReadStream& s, wire::read_limit rlimit) {
        wire::readable_message_or_command r;
        if (auto const rr = r.framing().read(s); !rr) {
            return rr.error();
        }

        if (auto const rr = rlimit.check(r.framing()); !rr) {
            return rr.error();
        }

        if (auto const rr = asio::read(s, boost::asio::buffer(r.mutable_buffer())); !rr) {
            return rr.error();
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
    //      void handler(message_or_command_result_type)
    //                   
    template<typename AsyncReadStream,
             typename CompletionHandler>
    void async_read(AsyncReadStream& s, wire::readable_message_or_command& r,
                        CompletionHandler handler, wire::read_limit rlimit) {
        BOOST_ASSERT(r.empty());
        auto& framing = r.framing();
        framing.async_read(s,
            [&, handler{ std::move(handler) }, rlimit](asio::read_result_type rr) {
                if (rr) {
                    if (auto const rr = rlimit.check(r.framing()); !rr) {
                        handler(rr.error());
                    } else {
                        asio::async_read(s, boost::asio::buffer(r.mutable_buffer()),
                            [&, handler{ std::move(handler) }](asio::read_result_type rr) {
                                if (rr) {
                                    handler(r.detach());
                                } else {
                                    handler(rr.error());
                                }
                            });
                    }
                } else {
                    handler(rr.error());
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
