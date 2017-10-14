#ifndef AZMQN_DETAIL_METADATA_HPP
#define AZMQN_DETAIL_METADATA_HPP

#include "wire.hpp"

#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/range/algorithm/find_if.hpp>

namespace azmqn::detail::transport {
    struct metadata 
        : std::enable_shared_from_this<metadata> {

        metadata() = default;
        metadata(wire::buffer_t buf)
            : pimpl_{std::make_shared<impl>(buf)}
        { }

        using result_t = boost::optional<const_buffer_t>;
        result_t operator[](boost::string_view name) {
            if (pimpl_) {
                auto it = boost::range::find_if(pimpl_->props_, [&](auto const x) { return name == x.name_; });
                if (it != std::end(pimpl_->props_)) {
                    return it->value_;
                }
            }
            return boost::none;
        }

    private:
        struct prop {
            boost::string_view name_;
            const_buffer_t value_;
        };
        using props_vec_t = boost::container::small_vector<prop, 16>;
        static props_vec_t get_props(const_buffer_t buf) {
            props_vec_t res;
            while (boost::asio::buffer_size(buf)) {
                uint8_t name_len;
                std::tie(name_len, buf) = wire::get_uint8(buf);
                boost::string_view name(boost::asio::buffer_cast<char const*>(buf), name_len);
                buf = buf + name_len;

                uint32_t prop_len;
                std::tie(prop_len, buf) = wire::get_uint32(buf);
                auto const value = boost::asio::buffer_cast<octet_t const*>(buf);
                res.push_back(prop{name, boost::asio::buffer(value, prop_len)});
                buf = buf + prop_len;
            }
            return res;
        }

        struct impl {
            wire::buffer_t buf_;
            props_vec_t props_;

            impl(wire::buffer_t buf)
                : buf_{ buf }
                , props_{ get_props(boost::asio::buffer(buf.data(), buf.size())) }
            { }
        };
        using ptr = std::shared_ptr<impl>;
        ptr pimpl_;
    };
} // namespace azmqn::detail::transport 
#endif // AZMQN_DETAIL_METADATA_HPP

