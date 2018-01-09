/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_SERVICE_BASE_HPP
#define AZMQN_DETAIL_SERVICE_BASE_HPP

#include <boost/asio/io_service.hpp>

namespace azmqn::detail {
    template <typename T>
    class service_id
        : public boost::asio::io_service::id
    { };

    template<typename T>
    class service_base
        : public boost::asio::io_service::service {
    public :
        static azmqn::detail::service_id<T> id;

        // Constructor.
        service_base(boost::asio::io_service& io_service)
            : boost::asio::io_service::service(io_service)
        { }
    };

    template <typename T>
    azmqn::detail::service_id<T> service_base<T>::id;
} // namespace azmq::detail
#endif // AZMQN_DETAIL_SERVICE_BASE_HPP

