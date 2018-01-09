/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_ASIO_BUFFER_HPP
#define AZMQN_ASIO_BUFFER_HPP

#include "../utility/octet.hpp"

#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/range/iterator_range_core.hpp>

#include <array>
#include <algorithm>
#include <ostream>

namespace  azmqn::asio {
    utility::octet* buffer_data(boost::asio::mutable_buffer buf) noexcept
    { return boost::asio::buffer_cast<utility::octet*>(buf); }

    utility::octet const* buffer_data(boost::asio::const_buffer buf) noexcept
    { return boost::asio::buffer_cast<utility::octet const*>(buf); }

    template<typename T>
    size_t buffer_size(T buf) noexcept
    { return boost::asio::buffer_size(buf); }

    template<typename T>
    auto buffer_range(T buf) {
        auto begin = buffer_data(buf);
        return boost::make_iterator_range(begin, begin + buffer_size(buf));
    }

    namespace detail {
        template<typename Buffer>
        std::ostream& dump_buffer(std::ostream& stm, Buffer const& buf) {
            stm << "{ ";
            char const* sep = "";
            auto begin = buffer_data(buf);
            auto end = begin + buffer_size(buf);
            for (; begin != end; ++begin) {
                stm << sep << *begin;
                sep = " ";
            }
            return stm << " }";
        }
    }

    std::ostream& operator<<(std::ostream& stm, boost::asio::mutable_buffer const& buf)
    { return detail::dump_buffer(stm, buf); }

    std::ostream& operator<<(std::ostream& stm, boost::asio::const_buffer const& buf)
    { return detail::dump_buffer(stm, buf); }

    // A buffer who's size is asserted to be at least min_size
    template<typename Buffer, size_t min_size>
    struct at_least_buffer {
        at_least_buffer(Buffer b) noexcept
            : b_{ b }
        { assert_precondition(); }

        at_least_buffer(boost::asio::mutable_buffers_1 b) noexcept
            : b_{ *std::begin(b) }
        { assert_precondition(); }

        at_least_buffer(boost::asio::const_buffers_1 b) noexcept
            : b_{ *std::begin(b) }
        { assert_precondition(); }

        utility::octet const* data() const noexcept { return buffer_data(b_); }

        Buffer consume(size_t size = min_size) const noexcept
        { return b_ + size; }

    protected:
        Buffer b_;

        void assert_precondition()
        { BOOST_ASSERT(buffer_size(b_) >= min_size); }
    };

    template<size_t min_size>
    struct at_least_mutable_buffer : at_least_buffer<boost::asio::mutable_buffer, min_size> {
        using base_type = at_least_buffer<boost::asio::mutable_buffer, min_size>;
        using base_type::base_type;

        utility::octet* data() noexcept { return buffer_data(base_type::b_); }
    };

    template<size_t min_size>
    struct at_least_const_buffer : at_least_buffer<boost::asio::const_buffer, min_size> {
        using base_type = at_least_buffer<boost::asio::const_buffer, min_size>;
        using base_type::base_type;
    };

    // a storage backed_buffer
    template<size_t small_size>
    class backed_buffer {
        using backing_type = boost::container::small_vector<utility::octet, small_size>;
        backing_type v_;

    public:
        using value_type = typename backing_type::value_type;
        using iterator = typename backing_type::iterator;
        using const_iterator = typename backing_type::const_iterator;

        backed_buffer(size_t capacity = 0)
            : v_(capacity, boost::container::default_init_t{})
        { }

        backed_buffer(boost::asio::const_buffer b)
            : backed_buffer{ buffer_size(b) }
        { std::copy_n(buffer_data(b), buffer_size(b), std::begin(v_)); }

        size_t size() const noexcept { return v_.size(); }

        iterator begin() { return std::begin(v_); }
        iterator end() { return std::end(v_); }
        const_iterator begin() const { return std::begin(v_); }
        const_iterator end() const { return std::end(v_); }

        boost::asio::mutable_buffer mutable_buffer() noexcept
        { return boost::asio::buffer(v_.data(), size()); }

        boost::asio::const_buffer const_buffer() const noexcept
        { return boost::asio::buffer(v_.data(), size()); }
    };
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_ASIO_BUFFER_HPP
