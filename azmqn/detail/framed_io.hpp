/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_FRAMED_IO_HPP
#define AZMQN_DETAIL_FRAMED_IO_HPP

#include "../utility/expected.hpp"
#include "wire.hpp"
#include "traffic.hpp"

#include "../xasio/read.hpp"
#include "../xasio/write.hpp"

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

        if (auto const rr = xasio::read(s, boost::asio::buffer(r.mutable_buffer())); !rr) {
            return rr.error();
        }
        return r.detach();
    }

    template<typename SyncWriteStream>
    xasio::write_result_type write(SyncWriteStream& s, wire::writable_message_or_command const& w) {
        return xasio::write(s, w.buffer_sequence());
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
            [&, handler{ std::move(handler) }, rlimit](xasio::read_result_type rr) {
                if (rr) {
                    if (auto const rr = rlimit.check(r.framing()); !rr) {
                        handler(rr.error());
                    } else {
                        xasio::async_read(s, boost::asio::buffer(r.mutable_buffer()),
                            [&, handler{ std::move(handler) }](xasio::read_result_type rr) {
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
    // completion handler is called. Completion handler's signature is -
    //      void handler(asio::write_result_type)
    template<typename AsyncWriteStream,
             typename CompletionHandler>
    void async_write(AsyncWriteStream& s, wire::writable_message_or_command const& w,
                        CompletionHandler&& handler) {
        BOOST_ASSERT(!w.empty());
        xasio::async_write(s, w.buffer_sequence(), std::forward<CompletionHandler>(handler));
    }
} // namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_FRAMED_IO_HPP
