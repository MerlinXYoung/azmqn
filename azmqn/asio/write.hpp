/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_ASIO_WRITE_HPP
#define AZMQN_ASIO_WRITE_HPP

#include "../utility/expected.hpp"

#include <boost/asio/write.hpp>

namespace azmqn::asio {
    using write_result_type = utility::expected<size_t, boost::system::error_code>;

    template<typename SyncWriteStream,
             typename ConstBufferSequence>
    write_result_type write(SyncWriteStream & s, ConstBufferSequence const & buffers) {
        boost::system::error_code ec;
        auto res = boost::asio::write(s, buffers, ec);
        if (ec)
            return ec;
        return res;
    }

    template<typename AsyncWriteStream,
             typename ConstBufferSequence,
             typename WriteHandler>
    void async_write(AsyncWriteStream & s, ConstBufferSequence const& buffers,
                     WriteHandler handler) {
        boost::asio::async_write(s, buffers,
                [handler](boost::system::error_code& ec, size_t bytes_transferred) {
                if (ec) {
                    handler(utility::make_unexpected(ec));
                } else {
                    handler(utility::make_expected(bytes_transferred));
                }
        });
    }
} // namespace azmqn::asio
#endif // AZMQN_ASIO_WRITE_HPP


