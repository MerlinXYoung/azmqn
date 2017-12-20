/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_BUFFER_HPP
#define AZMQN_DETAIL_BUFFER_HPP

#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/asio/buffer.hpp>

#include <array>
#include <algorithm>
#include <ostream>

namespace  azmqn::detail::transport {
    using octet_t = std::byte;

    using mutable_buffer_t = boost::asio::mutable_buffer;
    using mutable_buffers_1_t = boost::asio::mutable_buffers_1;
    using const_buffer_t = boost::asio::const_buffer;
    using const_buffers_1_t = boost::asio::const_buffers_1;

    octet_t* buffer_data(mutable_buffer_t buf) noexcept { return boost::asio::buffer_cast<octet_t*>(buf); }
    octet_t const* buffer_data(const_buffer_t buf) noexcept { return boost::asio::buffer_cast<octet_t const*>(buf); }

    template<typename T>
    size_t buffer_size(T buf) noexcept { return boost::asio::buffer_size(buf); }

    namespace detail {
        template<typename Buffer>
        std::ostream& dump_buffer(std::ostream& stm, Buffer const& buf) {
            stm << "{ ";
            char const* sep = "";
            auto c = buffer_data(buf);
            auto sz = buffer_size(buf);
            do {
                stm << sep
                    << "0123456790abcdef"[std::to_integer<short>(*c) / 16]
                    << "0123456790abcdef"[std::to_integer<short>(*c) % 16];
                sep = " ";
                ++c;
            } while (--sz);
            return stm << " }";
        }
    }
    std::ostream& operator<<(std::ostream& stm, mutable_buffer_t const& buf) {
        return detail::dump_buffer(stm, buf);
    }

    std::ostream& operator<<(std::ostream& stm, const_buffer_t const& buf) {
        return detail::dump_buffer(stm, buf);
    }


    // A buffer who's size is asserted to be at least min_size
    template<typename Buffer, size_t min_size>
    struct at_least_buffer {
        at_least_buffer(Buffer b) noexcept
            : b_{ b }
        { assert_precondition(); }

        at_least_buffer(mutable_buffers_1_t b) noexcept
            : b_{ *std::begin(b) }
        { assert_precondition(); }

        at_least_buffer(const_buffers_1_t b) noexcept
            : b_{ *std::begin(b) }
        { assert_precondition(); }

        octet_t const* data() const noexcept { return buffer_data(b_); }

        Buffer consume() const noexcept
        { return b_ + min_size; }

    protected:
        Buffer b_;

        void assert_precondition()
        { BOOST_ASSERT(buffer_size(b_) >= min_size); }
    };

    template<size_t min_size>
    struct at_least_mutable_buffer : at_least_buffer<mutable_buffer_t, min_size> {
        using base_type = at_least_buffer<mutable_buffer_t, min_size>;
        using base_type::base_type;

        octet_t* data() noexcept { return buffer_data(base_type::b_); }
    };

    template<size_t min_size>
    struct at_least_const_buffer : at_least_buffer<const_buffer_t, min_size> {
        using base_type = at_least_buffer<const_buffer_t, min_size>;
        using base_type::base_type;
    };

    // a storage backed_buffer
    template<size_t small_size>
    struct backed_buffer {
        backed_buffer(size_t capacity = 0)
            : v_(capacity, boost::container::default_init_t{})
        { }

        backed_buffer(const_buffer_t b)
            : backed_buffer{ buffer_size(b) }
        { std::copy_n(buffer_data(b), buffer_size(b), std::begin(v_)); }

        size_t size() const noexcept { return v_.size(); }

        mutable_buffer_t mutable_buffer() noexcept
        { return boost::asio::buffer(v_.data(), size()); }

        const_buffer_t const_buffer() const noexcept
        { return boost::asio::buffer(v_.data(), size()); }

    private:
        using backing_type = boost::container::small_vector<octet_t, small_size>;
        backing_type v_;

    public:
        using value_type = typename backing_type::value_type;
    };
} // namespace namespace azmqn::detail::transport
#endif // AZMQN_DETAIL_BUFFER_HPP
