/*
    Copyright (c) 2013-2017`Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef AZMQN_DETAIL_MECHANISM_CATALOG_HPP
#define AZMQN_DETAIL_MECHANISM_CATALOG_HPP

#include "traffic.hpp"

#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/find.hpp>

namespace azmqn::detail::transport {
    class mechanism_catalog {
        struct mechanism_state {
            virtual ~mechanism_state() = default;
        };
        using state_ptr = std::unique_ptr<mechanism_state>;

        struct mechanism_concept {
            virtual ~mechanism_concept() = default;
            virtual boost::asio::const_buffer get_mechanism() const = 0;
            virtual boost::string_view get_name() const = 0;

            virtual state_ptr init_state() const = 0;
            virtual boost::optional<wire::command_t> process_command(state_ptr&, wire::command_t const&) const = 0;
        };

    public: 
        template<typename... Mechanisms>
        mechanism_catalog(Mechanisms... mechanisms)
            : mechanisms_(make_catalog(std::forward<Mechanisms>(mechanisms)...))
        { }

        struct mechanism {
            boost::optional<wire::command_t> operator()(wire::command_t const& cmd) {
                BOOST_ASSERT_MSG(state_, "reusing moved instance");
                return m_.process_command(state_, cmd);
            }

        private:
            friend mechanism_catalog;
            mechanism_concept const& m_;
            state_ptr state_;
            mechanism(mechanism_concept const& m)
                : m_(m)
                , state_(m.init_state())
            { }
        };

        boost::optional<mechanism> find_mechanism(boost::string_view name) const {
            auto it = boost::range::find_if(mechanisms_, [&](auto const& m) {
                    return m->get_name() == name;
                });
            if (it == std::end(mechanisms_))
                return boost::none;
            return mechanism(*(it->get()));
        }

    private:
        using ptr_t = std::unique_ptr<mechanism_concept>;
        using vector_t = std::vector<ptr_t>;
        vector_t mechanisms_;

        template<typename T>
        struct mechanism_model final : mechanism_concept {
            using state_type = typename T::state_type;
            T data_;
            std::array<octet_t, wire::max_mechanism_size> mbuf_;

            mechanism_model(T t) : data_(std::move(t)) {
                boost::range::fill(mbuf_, 0);
                data_.fill_mechanism(boost::asio::buffer(mbuf_));
            }

            boost::asio::const_buffer get_mechanism() const override {
                return boost::asio::buffer(mbuf_);
            }

            boost::string_view get_name() const override {
                auto const it = boost::range::find(mbuf_, 0);
            }

            struct state_t : mechanism_state {
                state_type data_;
                state_t(state_type st) : data_(std::move(st)) { }
            };

            state_ptr init_state() const override { return std::make_unique<state_t>(data_.init_state()); }

            boost::optional<wire::command_t> process_command(state_ptr& state, wire::command_t const& cmd) const override {
                BOOST_ASSERT(state);
                auto p = static_cast<state_t*>(state.get());
                return data_.process_command(p->data_, cmd);
            }
        };

        template<typename... Mechanisms>
        vector_t make_catalog(Mechanisms... mechanisms) {
            vector_t res;
            res.resize(sizeof(mechanisms)...);
            boost::hana::fold(std::forward<Mechanisms>(mechanisms)..., [&](auto&& m) {
                using model_t = mechanism_model<decltype(m)>;
                res.emplace_back(std::make_unique<model_t>(std::forward<decltype(m)>(m)));
            });
            return res;
        }
    };
} // namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_MECHANISM_CATALOG_HPP
