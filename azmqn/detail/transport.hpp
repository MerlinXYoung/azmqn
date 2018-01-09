/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQN_DETAIL_TRANSPORT_HPP
#define AZMQN_DETAIL_TRANSPORT_HPP

#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/hana.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/sub_range.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>

#include <memory>
#include <array>
#include <vector>

namespace azmqn::detail::transport {
    struct transport_event_base {
        template<typename T>
        transport_event_base(T& transport)
        { p_ = new (d_.data()) model<T>(transport); }

        ~transport_event_base() {
            if (p_)
                p_->~concept();
        }

        using const_buffer_t = boost::asio::const_buffer;
        using const_buffer_sequence_t = boost::sub_range<const_buffer_t>;

    private:
        struct concept {
            virtual ~concept() = default;
        };

        template<typename T>
        struct model final : concept {
            T& data_;

            model(T& t) : data_(t) { }
        };
        std::array<uint8_t, sizeof(model<int>)> d_;
        concept* p_;
    };

    namespace events {
        struct connect : transport_event_base { };
        struct rcv_signature { };
        struct disconnect { };
        struct hello { };
        struct handshake { };
    } // namespace events

    struct zmtp_ : boost::msm::front::state_machine_def<zmtp_> {
        bool is_server_;
        mechanism_catalog const& mechanisms_;

        enum { greeting_length = 64 };
        std::array<octet_t, greeting_length> buf_;

        zmtp_(bool is_server, mechanism_catalog const& mechanisms)
            : is_server_(is_server)
            , mechanisms_(mechanisms)
        { }
        // states
        struct Disconnected : boost::msm::front::state<> { };
        struct Connecting : boost::msm::front::state<> { };
        struct Handshaking : boost::msm::front::state<> { };
        struct Connected : boost::msm::front::state<> { };
        struct Error : boost::msm::front::state<> { };

        static boost::asio::const_buffer signature() {
            static std::array<octet_t, 10> res{ 0xff, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x7f };
            return boost::asio::buffer(res);
        }

        static boost::asio::const_buffer version() {
            static std::array<octet_t, 2> res{ 0x03, 0x01 };
            return boost::asio::buffer(res);
        }

        // actions
        template<typename E>
        void send_greeting(E const&) {
        /*
            BOOST_ASSERT(mechanism_);
            std::array<octet_t, 31> fill;
            boost::range::fill(fill, 0);

            if (is_server_)
                fill[0] = 0x01;

            std::array<boost::asio::const_buffer, 4> bufs{{ 
                signature(),
                version(),
                mechanism_->get_mechanism(),
                boost::asio::buffer(fill)
            }};
        */
            // TODO send
        }

        template<typename E>
        void handshake(E const&) { }

        using z = zmtp_;
        struct transition_table : boost::mpl::vector<
            //      Start               Event                   Next            Action                Guard
            //     +-------------------+-----------------------+---------------+---------------------+-----------------+
            a_row< Disconnected        , events::connect       , Connecting    , &z::send_greeting                      >,
            a_row< Connecting          , events::rcv_signature , Handshaking   , &z::handshake                          >
        > { };

        using initial_state = Disconnected;
    };

    struct zmtp_stream_engine {
        using zmtp = boost::msm::back::state_machine<zmtp_>;
        using socket_type = boost::asio::ip::tcp::socket;

        struct connection {
            zmtp_stream_engine& owner_;
//            zmtp state_machine_;

            explicit connection(zmtp_stream_engine& owner, socket_type socket, bool is_server)
                : owner_(owner)
                , sock_(std::move(socket))
            { }

            void start() {
                // TODO implement
            }

            void stop() {
                // TODO implement
            }
        private:
            socket_type sock_;
        };

        zmtp_stream_engine(boost::asio::io_service& ios,
                           mechanism_catalog const& mechanisms)
            : ios_(ios)
            , mechanisms_(mechanisms)
        { }

        using endpoint_type = socket_type::endpoint_type;
        template<typename AcceptHandler>
        boost::system::error_code accept(endpoint_type const& endpoint,
                                         bool is_server,
                                         boost::system::error_code & ec,
                                         AcceptHandler&& handler) {
            socket_type sock(ios_);
            if (sock.bind(endpoint, ec))
                return ec;


            // TODO implement
            return ec;
        }

        boost::system::error_code connect(endpoint_type const& peer_endpoint,
                                          bool is_server,
                                          boost::system::error_code & ec) {
            socket_type sock(ios_);
            if (sock.connect(peer_endpoint, ec))
                return ec;

            // TODO implement

            return ec;
        }

    private:
        boost::asio::io_service& ios_;
        mechanism_catalog const& mechanisms_;
    };
} // namespace azmqn::detail::transport

#endif // AZMQN_DETAIL_TRANSPORT_HPP
