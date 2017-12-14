/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmqn

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

*/

// This implementation is adapted from the WebKit's WTF::Expected, however it does
// support throwing of unexpected_access<> as per http://wg21.link/p0323r1

/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AZMQN_EXPECTED_HPP_
#define AZMQN_EXPECTED_HPP_

#include <boost/assert.hpp>

#include <functional>
#include <type_traits>
#include <stdexcept>

namespace azmqn::utility {
    template<typename E>
    struct bad_expected_access : std::logic_error {
        using error_type = E;

        explicit bad_expected_access(E err)
            : err_{ std::move(err) }
        { }

        constexpr error_type const& error() const { return err_; }
        error_type& error() { return err_; }

    private:
        error_type err_;
    };

    template<typename E>
    struct unexpected {
        unexpected() = delete;
        constexpr explicit unexpected(E const& e) : val_{ e }
        { }

        constexpr explicit unexpected(E&& e) : val_{ std::forward<E>(e) } 
        { }

        constexpr E const& value() const& { return val_; }
        constexpr E& value() & { return val_; }
        constexpr E&& value() && { return std::move(val_); }

    private:
        E val_;
    };

    template<class E>
    constexpr bool operator==(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() == rhs.value(); }

    template<class E>
    constexpr bool operator!=(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() != rhs.value(); }

    template<class E>
    constexpr bool operator<(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() < rhs.value(); }

    template<class E>
    constexpr bool operator>(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() > rhs.value(); }

    template<class E>
    constexpr bool operator<=(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() <= rhs.value(); }

    template<class E>
    constexpr bool operator>=(unexpected<E> const& lhs, unexpected<E> const& rhs)
    { return lhs.value() >= rhs.value(); }

    template<typename E>
    constexpr unexpected<std::decay_t<E>> make_unexpected(E&& e) {
        return unexpected<std::decay_t<E>>{ std::forward<E>(e) };
    }

    struct unexpect_tag {
        unexpect_tag() = delete;
    };
    constexpr unexpect_tag unexpect { };

namespace expected_detail {
#ifdef HAS_STD_OPTIONAL
    using nullopt_t = std::nullopt_t;
#else
    struct nullopt_t {
        explicit constexpr nullopt_t(int) {}
    };
#endif

    template<
              typename T
              , std::enable_if_t<std::is_trivially_destructible<T>::value>* = nullptr
            >
    void destroy(T&) { }

    template<
              typename T
              , std::enable_if_t<!std::is_trivially_destructible<T>::value
                                 && (std::is_class<T>::value
                                         || std::is_union<T>::value)>* = nullptr
            >
    void destroy(T& t) { t.~T(); }

    static constexpr enum class value_tag_t{ } value_tag{ };
    static constexpr enum class error_tag_t{ } error_tag{ };

    template<typename T, typename E>
    union constexpr_storage {
        using value_type = T;
        using error_type = E;
        value_type val_;
        error_type err_;
        char dummy_;

        constexpr constexpr_storage()
            : dummy_{}
        { }

        constexpr constexpr_storage(value_tag_t)
            : val_{ }
        { }

        constexpr constexpr_storage(value_tag_t, value_type const& val)
            : val_{ val }
        { }

        constexpr constexpr_storage(error_tag_t)
            : err_{ }
        { }

        constexpr constexpr_storage(error_tag_t, error_type const& err)
            : err_{ err }
        { }

        ~constexpr_storage() = default;
    };

    template<typename T, typename E>
    union storage {
        using value_type = T;
        using error_type = E;
        value_type val_;
        error_type err_;
        char dummy_;

        constexpr storage()
            : dummy_{}
        { }

        constexpr storage(value_tag_t)
            : val_{ }
        { }

        constexpr storage(value_tag_t, value_type const& val)
            : val_{ val }
        { }

        constexpr storage(value_tag_t, value_type&& val)
            : val_{ std::forward<value_type>(val) }
        { }

        constexpr storage(error_tag_t)
            : err_{ }
        { }

        constexpr storage(error_tag_t, error_type const& err)
            : err_{ err }
        { }

        constexpr storage(error_tag_t, error_type&& err)
            : err_{ std::forward<error_type>(err) }
        { }

        ~storage() { }
    };

    template<typename E>
    union constexpr_storage<void, E> {
        using value_type = void;
        using error_type = E;
        error_type err_;
        char dummy_;

        constexpr constexpr_storage()
            : dummy_{}
        { }

        constexpr constexpr_storage(value_tag_t)
            : dummy_{ }
        { }

        constexpr constexpr_storage(error_tag_t)
            : err_{ }
        { }

        constexpr constexpr_storage(error_tag_t, error_type const& err)
            : err_{ err }
        { }

        ~constexpr_storage() = default;
    };

    template<typename E>
    union storage<void, E> {
        using value_type = void;
        using error_type = E;
        error_type err_;
        char dummy_;

        constexpr storage()
            : dummy_{}
        { }

        constexpr storage(value_tag_t)
            : dummy_{ }
        { }

        constexpr storage(error_tag_t)
            : err_{ }
        { }

        constexpr storage(error_tag_t, error_type const& err)
            : err_{ err }
        { }

        constexpr storage(error_tag_t, error_type&& err)
            : err_{ std::forward<error_type>(err) }
        { }

        ~storage() { }
    };

    template<typename T, typename E>
    struct constexpr_base {
        using value_type = T;
        using error_type = E;
        constexpr_storage<value_type, error_type> s_;
        bool has_;

        constexpr constexpr_base()
            : s_{}
            , has_{true}
        { }

        constexpr constexpr_base(value_tag_t tag)
            : s_{ tag }
            , has_{ true }
        { }

        constexpr constexpr_base(value_tag_t tag, value_type const& val)
            : s_{ tag, val }
            , has_{ true }
        { }

        constexpr constexpr_base(error_tag_t tag)
            : s_{ tag }
            , has_{ false }
        { }

        constexpr constexpr_base(error_tag_t tag, error_type const& err)
            : s_{ tag, err }
            , has_{ false }
        { }

        ~constexpr_base() = default;
    };

    template<typename T, typename E>
    struct base {
        using value_type = T;
        using error_type = E;
        storage<value_type, error_type> s_;
        bool has_;

        base()
            : s_{}
            , has_{true}
        { }

        constexpr base(value_tag_t tag)
            : s_{ tag }
            , has_{ true }
        { }

        constexpr base(value_tag_t tag, value_type const& val)
            : s_{ tag, val }
            , has_{ true }
        { }

        constexpr base(value_tag_t tag, value_type&& val)
            : s_{ tag, std::forward<value_type>(val) }
            , has_{ true }
        { }

        constexpr base(error_tag_t tag)
            : s_{ tag }
            , has_{ false }
        { }

        constexpr base(error_tag_t tag, error_type const& err)
            : s_{ tag, err }
            , has_{ false }
        { }

        constexpr base(error_tag_t tag, error_type&& err)
            : s_{ tag, std::forward<error_type>(err) }
            , has_{ false }
        { }

        base(base const& other)
            : has_{ other.has_ }
        {
            if (has_) {
                ::new (&s_.val_) value_type{ other.s_.val_ };
            } else {
                ::new (&s_.err_) error_type{ other.s_.err_ };
            }
        }

        base(base&& other)
            : has_{ other.has_ }
        {
            if (has_) {
                ::new (&s_.val_) value_type{ std::move(other.s_.val_) };
            } else {
                ::new (&s_.err_) error_type{ std::move(other.s_.err_) };
            }
        }

        ~base() {
            if (has_) {
                destroy(s_.val_);
            } else {
                destroy(s_.err_);
            }
        }
    };

    template<class E>
    struct constexpr_base<void, E> {
        using value_type = void;
        using error_type = E;
        constexpr_storage<value_type, error_type> s_;
        bool has_;

        constexpr constexpr_base()
            : s_{}
            , has_{true}
        { }

        constexpr constexpr_base(value_tag_t tag)
            : s_{ tag }
            , has_{ true }
        { }

        constexpr constexpr_base(error_tag_t tag)
            : s_{ tag }
            , has_{ false }
        { }

        constexpr constexpr_base(error_tag_t tag, error_type const& err)
            : s_{ tag, err }
            , has_{ false }
        { }

        constexpr constexpr_base(error_tag_t tag, error_type&& err)
            : s_{ tag, std::forward<error_type>(err) }
            , has_{ false }
        { }

        ~constexpr_base() = default;
    };

    template<typename E>
    struct base<void, E> {
        using value_type = void;
        using error_type = E;
        storage<value_type, error_type> s_;
        bool has_;

        base()
            : s_{}
            , has_{true}
        { }

        constexpr base(value_tag_t tag)
            : s_{ tag }
            , has_{ true }
        { }

        constexpr base(error_tag_t tag)
            : s_{ tag }
            , has_{ false }
        { }

        constexpr base(error_tag_t tag, error_type const& err)
            : s_{ tag, err }
            , has_{ false }
        { }

        constexpr base(error_tag_t tag, error_type&& err)
            : s_{ tag, std::forward<error_type>(err) }
            , has_{ false }
        { }

        base(base const& other)
            : has_{ other.has_ }
        {
            if (!has_) {
                ::new (&s_.err_) error_type{ other.s_.err_ };
            }
        }

        base(base&& other)
            : has_{ other.has_ }
        {
            if (!has_) {
                ::new (&s_.err_) error_type{ std::move(other.s_.err_) };
            }
        }

        ~base() {
            if (!has_) {
                destroy(s_.err_);
            }
        }
    };

    template<typename T, typename E>
    using base_select = typename std::conditional<
                (std::is_void<T>::value || std::is_trivially_destructible<T>::value)
                    && std::is_trivially_destructible<E>::value
            , constexpr_base<
                  typename std::remove_const<T>::type
                , typename std::remove_const<E>::type>
            , base<
                  typename std::remove_const<T>::type
                , typename std::remove_const<E>::type>
        >::type;
} // namespace expected_detail

template<typename T, typename E>
class expected : private expected_detail::base_select<T, E> {
    using base = expected_detail::base_select<T, E>;
public:
    using value_type = typename base::value_type;
    using error_type = typename base::error_type;

private:
    using type = expected<value_type, error_type>;
    
public:
    expected()
        : base{ expected_detail::value_tag }
    { }

    expected(expected const&) = default;
    expected(expected&&) = default;
    
    constexpr expected(value_type const& val)
        : base{ expected_detail::value_tag, val }
    { }

    constexpr expected(value_type&& val)
        : base{ expected_detail::value_tag, std::forward<value_type>(val) }
    { }

    constexpr expected(error_type const& err)
        : base{ expected_detail::error_tag, err }
    { }

    constexpr expected(error_type&& err)
        : base{ expected_detail::error_tag, std::forward<error_type>(err) }
    { }

    constexpr expected(unexpected<error_type> const& u)
        : base{ expected_detail::error_tag, u.value() }
    { }

    constexpr expected(unexpected<error_type>&& u)
        : base{ expected_detail::error_tag, std::forward<unexpected<error_type>>(u).value() }
    { }

    template<typename U>
    constexpr expected(unexpected<U> const& u)
        : base{ expected_detail::error_tag, u.value() }
    { }

    ~expected() = default;

    expected& operator=(expected const& lhs)
    { type(lhs).swap(*this); return *this; }

    expected& operator=(expected&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    template<typename U>
    expected& operator=(U&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    expected& operator=(unexpected<error_type> const& lhs)
    { type(lhs).swap(*this); return *this; }

    expected& operator=(unexpected<error_type>&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    void swap(expected& other) {
        if (base::has_ && other.has_) {
            swap(base::s_.val_, other.s_.val_);
        } else if (base::has_ && !other.has_) {
            error_type e{ std::move(other.s_.err_) };
            expected_detail::destroy(other.s_.err_);
            ::new (&other.s_.val_) value_type{ std::move(base::s_.val_) };
            expected_detail::destroy(base::s_.val_);
            ::new (&base::s_.err_) error_type{ std::move(e) };
            swap(base::has_, other.has_);
        } else if (!base::has_ && other.has_) {
            value_type v{ std::move(other.s_.val_) };
            expected_detail::destroy(other.s_.val_);
            ::new (&other.s_.err_) error_type{ std::move(base::s_.err_) };
            expected_detail::destroy(base::s_.err_);
            ::new (&base::s_.val_) value_type{ std::move(v) };
            swap(base::has_, other.has_);
        } else {
            swap(base::s_.err_, other.s_.error_);
        }
    }

    constexpr value_type const* operator->() const { return &base::s_.val_; }
    value_type* operator->() { return &base::s_.val_; }
    constexpr value_type const& operator*() const & { return base::s_.val_; }
    value_type& operator*() & { return base::s_.val_; }
    constexpr value_type const&& operator*() const && { return std::move(base::s_.val_); }
    constexpr value_type && operator*() && { return std::move(base::s_.val_); }

    constexpr explicit operator bool() const { return base::has_; }
    constexpr bool has_value() const { return base::has_; }
    constexpr value_type const& value() const & {
        if (base::has_) {
            return base::s_.val_;
        }
        throw bad_expected_access<error_type>(base::s_.err_);
    }

    constexpr value_type& value() & {
        if (base::has_) {
            return base::s_.val_;
        }
        throw bad_expected_access<error_type>(base::s_.err_);
    }

    constexpr value_type const&& value() const && {
        if (base::has_) {
            return base::s_.val_;
        }
        throw bad_expected_access<error_type>(base::s_.err_);
    }

    constexpr value_type&& value() && {
        if (base::has_) {
            return base::s_.val_;
        }
        throw bad_expected_access<error_type>(base::s_.err_);
    }

    constexpr error_type const& error() const & {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type & error() & {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type const&& error() const && {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type && error() && {
        BOOST_ASSERT(!base::has_);
        return std::move(!base::has_ ? base::s_.err_
                                     : std::terminate(), base::s_.err_);
    }

    constexpr unexpected<error_type> get_unexpected() const {
        BOOST_ASSERT(!base::has_);
        return unexpected<error_type>(!base::has_ ? base::s_.err_
                                                  : std::terminate(), base::s_.err_);
    }

    template<typename U>
    constexpr value_type value_or(U&& u) const & {
        return base::has_ ? **this
                          : static_cast<value_type>(std::forward<U>(u));
    }

    template<typename U>
    value_type value_or(U&& u) && {
        return base::has_ ? std::move(**this)
                          : static_cast<value_type>(std::forward<U>(u));
    }
};

template<class E>
class expected<void, E> : private expected_detail::base_select<void, E> {
    using base = expected_detail::base_select<void, E>;
public:
    using value_type = typename base::value_type;
    using error_type = typename base::error_type;

private:
    using type = expected<value_type, error_type>;

public:
    expected()
        : base{ expected_detail::value_tag }
    { }

    expected(expected const&) = default;
    expected(expected&&) = default;

    constexpr expected(unexpected<error_type> const& u)
        : base{ expected_detail::error_tag, u.value() }
    { }

    constexpr expected(unexpected<error_type>&& u)
        : base{ expected_detail::error_tag, std::forward<unexpected<error_type>>(u).value() }
    { }

    template<typename U>
    constexpr expected(unexpected<U> const& u)
        : base{ expected_detail::error_tag, u.value() }
    { }

    ~expected() = default;

    expected& operator=(expected const& lhs)
    { type(lhs).swap(*this); return *this; }

    expected& operator=(expected&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    template<typename U>
    expected& operator=(U&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    expected& operator=(unexpected<error_type> const& lhs)
    { type(lhs).swap(*this); return *this; }

    expected& operator=(unexpected<error_type>&& lhs)
    { type(std::move(lhs)).swap(*this); return *this; }

    void swap(expected& other) {
        if (base::has_ && other.has_) {
            // do nothing
        } else if (base::has_ && !other.has_) {
            error_type e{ std::move(other.s_.err_) };
            ::new (&base::s_.err_) error_type{ std::move(e) };
            swap(base::has_, other.has_);
        } else if (!base::has_ && other.has_) {
            ::new (&other.s_.err_) error_type{ std::move(base::s_.err_) };
            swap(base::has_, other.has_);
        } else {
            swap(base::s_.err_, other.s_.error_);
        }
    }

    constexpr explicit operator bool() const { return base::has_; }
    constexpr bool has_value() const { return base::has_; }
    void value() const { 
        if (!base::has_)
            throw bad_expected_access<error_type>(base::err_);
        return void();
    }

    constexpr error_type const& error() const & {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type & error() & {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type const&& error() const && {
        BOOST_ASSERT(!base::has_);
        return !base::has_ ? base::s_.err_
                           : std::terminate(), base::s_.err_;
    }

    constexpr error_type && error() && {
        BOOST_ASSERT(!base::has_);
        return std::move(!base::has_ ? base::s_.err_
                                     : std::terminate(), base::s_.err_);
    }

    constexpr unexpected<error_type> get_unexpected() const {
        BOOST_ASSERT(!base::has_);
        return unexpected<error_type>(!base::has_ ? base::s_.err_
                                                  : std::terminate(), base::s_.err_);
    }
};

template <typename T, typename E>
constexpr bool operator==(expected<T, E> const& lhs, expected<T, E> const& rhs)
{ return bool(lhs) == bool(rhs) && (lhs ? lhs.value() == rhs.value() : lhs.error() == rhs.error()); }

template <typename T, typename E>
constexpr bool operator!=(expected<T, E> const& lhs, expected<T, E> const& rhs)
{ return !(lhs == rhs); }

template <typename T, typename E>
constexpr bool operator<(expected<T, E> const& lhs, expected<T, E> const& rhs) {
    return (!bool(lhs) && bool(rhs)) ? false
                                     : ((bool(lhs) && !bool(rhs)) ? true
                                                                  : ((bool(lhs) && bool(rhs)) ? lhs.value() < rhs.value()
                                                                                              : lhs.error() < rhs.error()));
}

template <typename T, typename E>
constexpr bool operator>(expected<T, E> const& lhs, expected<T, E> const& rhs)
{ return !(lhs == rhs) && !(lhs < rhs); }

template <typename T, typename E>
constexpr bool operator<=(expected<T, E> const& lhs, expected<T, E> const& rhs)
{ return (lhs == rhs) || (lhs < rhs); }

template <typename T, typename E>
constexpr bool operator>=(expected<T, E> const& lhs, expected<T, E> const& rhs)
{ return (lhs == rhs) || (lhs > rhs); }

template <typename E>
constexpr bool operator==(expected<void, E> const& lhs, expected<void, E> const& rhs)
{ return bool(lhs) == bool(rhs) && (lhs ? true : lhs.error() == rhs.error()); } // Not in the current paper.

template <typename E>
constexpr bool operator<(expected<void, E> const& lhs, expected<void, E> const& rhs) { // Not in the current paper.
    return (!bool(lhs) && bool(rhs)) ? false
                                     : ((bool(lhs) && !bool(rhs)) ? true
                                                                  : ((bool(lhs) && bool(rhs)) ? false
                                                                                              : lhs.error() < rhs.error()));
}

template <typename T, typename E>
constexpr bool operator==(expected<T, E> const& lhs, T const& rhs)
{ return lhs == expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator==(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } == rhs; }

template <typename T, typename E>
constexpr bool operator!=(expected<T, E> const& lhs, T const& rhs)
{ return lhs != expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator!=(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } != rhs; }

template <typename T, typename E>
constexpr bool operator<(expected<T, E> const& lhs, T const& rhs)
{ return lhs < expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator<(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } < rhs; }

template <typename T, typename E>
constexpr bool operator<=(expected<T, E> const& lhs, T const& rhs)
{ return lhs <= expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator<=(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } <= rhs; }

template <typename T, typename E>
constexpr bool operator>(expected<T, E> const& lhs, T const& rhs)
{ return lhs > expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator>(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } > rhs; }

template <typename T, typename E>
constexpr bool operator>=(expected<T, E> const& lhs, T const& rhs)
{ return lhs >= expected<T, E>{ rhs }; }

template <typename T, typename E>
constexpr bool operator>=(T const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>{ lhs } >= rhs; }

template <typename T, typename E>
constexpr bool operator==(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs == expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator==(unexpected<E> const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>(lhs) == rhs; }

template <typename T, typename E>
constexpr bool operator!=(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs != expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator!=(unexpected<E> const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>(lhs) != rhs; }

template <typename T, typename E>
constexpr bool operator<(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs < expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator<(unexpected<E> const& lhs, expected<T, E> const& rhs)
{  return expected<T, E>(lhs) < rhs; }

template <typename T, typename E>
constexpr bool operator<=(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs <= expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator<=(unexpected<E> const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>(lhs) <= rhs; }

template <typename T, typename E>
constexpr bool operator>(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs > expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator>(unexpected<E> const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>(lhs) > rhs; }

template <typename T, typename E>
constexpr bool operator>=(expected<T, E> const& lhs, unexpected<E> const& rhs)
{ return lhs >= expected<T, E>(rhs); }

template <typename T, typename E>
constexpr bool operator>=(unexpected<E> const& lhs, expected<T, E> const& rhs)
{ return expected<T, E>(lhs) >= rhs; }

template <typename T, typename E>
void swap(expected<T, E>& lhs, expected<T, E>& rhs) { lhs.swap(rhs); }

template <typename T, typename E = expected_detail::nullopt_t>
constexpr expected<std::decay_t<T>, E> make_expected(T&& v) {
    return expected<std::decay_t<T>, E>{ std::forward<T>(v) };
}

template <typename T, typename E>
constexpr expected<T, std::decay_t<E>> make_expected_from_error(E&& e) {
    return expected<T, std::decay_t<E>>{ make_unexpected(std::forward<E>(e)) };
}

template <typename T, typename E, typename U>
constexpr expected<T, E> make_expected_from_error(U&& u) {
    return make_expected_from_error<T>(E{ std::forward<U>(u) });
}
} // namespace azmqn::utility

namespace std {
template<typename T, typename E>
struct hash<azmqn::utility::expected<T, E>> {
    using argument_type = azmqn::utility::expected<T, E>;
    using result_type = size_t;
    
    result_type operator()(argument_type const& e) {
        return e ? hash<typename argument_type::value_type>{ } (e.value())
                 : hash<typename argument_type::error_type>{ } (e.error());
    }
};

template<typename E>
struct hash<azmqn::utility::expected<void, E>> {
    using argument_type = azmqn::utility::expected<void, E>;
    using result_type = size_t;
    
    result_type operator()(argument_type const& e) {
        return e ? 0
                 : hash<typename argument_type::error_type>{ } (e.error());
    }
};
} // namespace std
#endif // AZMQN_EXPECTED_HPP_
