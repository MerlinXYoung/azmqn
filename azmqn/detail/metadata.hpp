/*
    Copyright (c) 2013-2017 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_METADATA_HPP
#define AZMQN_DETAIL_METADATA_HPP

#include "wire.hpp"

#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/numeric.hpp>

#include <tuple>
#include <memory>

namespace azmqn::detail::transport {
    struct metadata 
        : std::enable_shared_from_this<metadata> {

        metadata() noexcept = default;

        boost::asio::const_buffer to_buffer() const noexcept {
            if (!pimpl_)
                return boost::asio::const_buffer{ };
            return pimpl_->buf_.const_buffer();
        }

        using result_t = boost::optional<boost::asio::const_buffer>;
        result_t operator[](boost::string_view name) const noexcept {
            if (pimpl_) {
                auto it = boost::range::find_if(pimpl_->props_, [&](auto const x) { return name == x.name; });
                if (it != std::end(pimpl_->props_)) {
                    return it->value;
                }
            }
            return boost::none;
        }

        static
        metadata from_buffer(wire::buffer_t buf, boost::system::error_code& ec) {
            try {
                return metadata{ std::move(buf) };
            } catch (boost::system::system_error const& e) {
                ec = e.code();
            }
            return metadata{ };
        }

        struct prop_t {
            boost::string_view name;
            boost::asio::const_buffer value;
        };

        static
        metadata from_props(std::initializer_list<prop_t> const& props) {
            auto size = boost::accumulate(props, 0ul, [](auto r, auto const& v) {
                            return r + sizeof(uint8_t) + v.name.size()
                                     + sizeof(uint32_t) + buffer_size(v.value);
                    });

            wire::buffer_t buf{ size };
            auto b = buf.mutable_buffer();
            boost::for_each(props, [&](auto const& v) {
                b = wire::put(b, v.name.size());
                boost::asio::buffer_copy(b, boost::asio::buffer(v.name.data(), v.name.size()));
                b = wire::put(b, static_cast<uint32_t>(buffer_size(v.value)));
                boost::asio::buffer_copy(b, v.value);
            });
            return metadata{ std::move(buf), size };
        }

    private:
        struct prop : prop_t {
            using name_res_t = std::pair<boost::string_view, boost::asio::const_buffer>;
            static name_res_t get_name(boost::asio::const_buffer buf, boost::system::error_code& ec) noexcept {
                uint8_t len;
                std::tie(len, buf) = wire::get<uint8_t>(buf);
                if (asio::buffer_size(buf) < len) {
                    ec = make_error_code(boost::system::errc::message_size);
                    return std::make_pair(boost::string_view{ }, buf);
                }

                return std::make_pair(boost::string_view{ boost::asio::buffer_cast<char const*>(buf), len },
                                         buf + len);
            }

            using value_res_t = std::pair<boost::asio::const_buffer, boost::asio::const_buffer>;
            static value_res_t get_value(boost::asio::const_buffer buf, boost::system::error_code& ec) noexcept {
                uint32_t len;
                std::tie(len, buf) = wire::get<uint32_t>(buf);
                if (asio::buffer_size(buf) < len) {
                    ec = make_error_code(boost::system::errc::message_size);
                    return std::make_pair(boost::asio::const_buffer{ }, boost::asio::const_buffer{ });
                }

                auto const value = asio::buffer_data(buf);
                return std::make_pair(boost::asio::buffer(value, len), buf + len);
            }

            using prop_res_t = std::pair<prop, boost::asio::const_buffer>;
            static
            prop_res_t from_buffer(boost::asio::const_buffer buf, boost::system::error_code& ec) noexcept {
                boost::string_view name;
                std::tie(name, buf) = get_name(buf, ec);
                if (ec)
                    return std::make_pair(prop{ }, buf);

                boost::asio::const_buffer value;
                std::tie(value, buf) = get_value(buf, ec);
                if (ec)
                    return std::make_pair(prop{ }, buf);
                return std::make_pair(prop{ name, value }, buf);
            }
        };

        struct props_vec_t : boost::container::small_vector<prop, 16> {
            static props_vec_t from_buffer(boost::asio::const_buffer buf, size_t hint) {
                props_vec_t res;
                res.reserve(hint);
                while (boost::asio::buffer_size(buf)) {
                    boost::system::error_code ec;
                    prop p;
                    std::tie(p, buf) = prop::from_buffer(buf, ec);
                    if (ec)
                        throw boost::system::system_error(ec);
                    res.push_back(p);
                }
                return res;
            }
        };

        struct impl {
            wire::buffer_t buf_;
            props_vec_t props_;

            impl(wire::buffer_t buf, size_t hint)
                : buf_{ std::move(buf) }
                , props_{ props_vec_t::from_buffer(buf_.const_buffer(), hint) }
            { }
        };
        using ptr = std::shared_ptr<impl>;
        ptr pimpl_;

        metadata(wire::buffer_t buf, size_t hint = 0)
            : pimpl_{ std::make_shared<impl>(std::move(buf), hint) }
        { }

    };
} // namespace azmqn::detail::transport 
#endif // AZMQN_DETAIL_METADATA_HPP

