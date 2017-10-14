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
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/sub_range.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>

#include <memory>
#include <array>
#include <vector>

namespace azmqn::detail::transport {

    namespace frames {
    struct frame {
        frame(boost::asio::const_buffer buf) {
        }

        static bool complete_frame(boost::asio::const_buffer) {
            return false;
        }

    private:
        enum { small_size = 512 };
        using vector_t = boost::container::small_vector<octet_t, small_size>;
        vector_t buf_;


    };
    }

    struct command_t { };

#if 0
    struct message_t { };

    struct const_frame : frame_base {
        const_frame(boost::asio::const_buffer buf, bool last, message_t m) { }
        const_frame(boost::asio::const_buffer buf, command_t) { }
    };

    struct mutable_frame : frame_base {
        vector_t::iterator cur_ = std::begin(buf_);
        boost::optional<size_t> length_;

        bool valid() const { return static_cast<bool>(length_); }

        bool is_command() const {
            BOOST_ASSERT(valid());
            return frame_base::is_command(buf_[0]);
        }

        bool is_final() const {
            BOOST_ASSERT(valid());
            return !frame_base::is_more(buf_[0]);
        }

        size_t size() {
            BOOST_ASSERT(valid());
            return *length_;
        }

        void reset() {
            buf_.clear();
            buf_.shrink_to_fit();
            cur_ = std::begin(buf_);
            length_ = boost::none;
        }

        using maybe_mutable_buffer_t = boost::optional<boost::asio::mutable_buffer>;
        maybe_mutable_buffer_t read_more() {
            if (cur_ == std::begin(buf_)) {
                // empty? next read is 2 bytes
                buf_.resize(2);
                cur_ = std::begin(buf_) + 2;
                return boost::asio::buffer(buf_.data(), 2);
            } else if (buf_.size() == 2) {
                if (is_long(buf_[0])) {
                    octet_t* p = &(*cur_);
                    cur_ += 7;
                    return boost::asio::buffer(p, 7); // read 7 more bytes
                }
                length_ = buf_[1];
            } else if (!length_ && (buf_.size() == 9)) {
                // TODO network byte order
                length_ = *reinterpret_cast<size_t*>(&buf_[1]);
            }

            BOOST_ASSERT(static_cast<bool>(length_)); // must have obtained a length by here
            if (buf_.size() < *length_) {
                buf_.resize(*length_, 0);
                return boost::asio::buffer(&(*cur_), std::distance(cur_, std::end(buf_)));
            }
            return boost::none;
        }
    };
#endif

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
            virtual boost::optional<command_t> process_command(state_ptr&, command_t const&) const = 0;
        };

    public: 
        template<typename... Mechanisms>
        mechanism_catalog(Mechanisms... mechanisms)
            : mechanisms_(make_catalog(std::forward<Mechanisms>(mechanisms)...))
        { }

        struct mechanism {
            boost::optional<command_t> operator()(command_t const& cmd) {
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
            std::array<octet_t, 20> mbuf_;

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

            boost::optional<command_t> process_command(state_ptr& state, command_t const& cmd) const override {
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
