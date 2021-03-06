/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    This is an implementation of Andrei Alexandrescu's ScopeGuard type from
    the "Systematic Error Handling in C++" talk given at C++ And Beyond 2012
*/
#ifndef AZMQN_SCOPE_GUARD_HPP_
#define AZMQN_SCOPE_GUARD_HPP_

#include <utility>

namespace azmqn::utility {
template<class F>
struct scope_guard_t {
    scope_guard_t(F func) : func_(std::move(func)), active_(true) { }
    ~scope_guard_t() {
        if (active_) func_();
    }

    void dismiss() { active_ = false; }

    scope_guard_t() = delete;
    scope_guard_t(const scope_guard_t &) = delete;
    scope_guard_t& operator=(const scope_guard_t&) = delete;
    scope_guard_t(scope_guard_t && rhs) :
        func_(std::move(rhs.func_)),
        active_(rhs.active_) {
            rhs.dismiss();
        }
private:
    F func_;
    bool active_;
};

template<class F>
scope_guard_t<F> scope_guard(F func) {
    return scope_guard_t<F>(std::move(func));
}

enum class scope_guard_on_exit {};
template<class F>
scope_guard_t<F> operator+(scope_guard_on_exit, F && func) {
    return scope_guard<F>(std::forward<F>(func));
}
} // namespace azmqn::utility

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
    #define ANONYMOUS_VARIABLE(str)\
        CONCATENATE(str,__COUNTER__)
#else
    #define ANONYMOUS_VARIABLE(str)\
        CONCATENATE(str,__LINE__)
#endif // __COUNTER__

#define SCOPE_EXIT\
    auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = azmqn::utility::scope_guard_on_exit() + [&]()
#endif // AZMQN_SCOPE_GUARD_HPP_
