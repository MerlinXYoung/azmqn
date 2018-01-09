/*
    Copyright (c) 2013-2018 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#define BOOST_ENABLE_ASSERT_HANDLER

#include <azmqn/utility/octet.hpp>

#include <azmqn/asio/read.hpp>

#include <azmqn/detail/wire.hpp>
#include <azmqn/detail/framed_io.hpp>
#include <azmqn/detail/mechanism_catalog.hpp>

#include <boost/utility/string_view.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/numeric.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <vector>
#include <iterator>

#define CATCH_CONFIG_MAIN
#include <test/catch.hpp>

namespace boost {
    void assertion_failed(char const* expr, char const* function, char const* file, long line) {
        FAIL("Assertion: " << expr << " in " << function << "\n\t" << file << ":" << line);
    }
} // namespace boost

namespace asio = boost::asio;
namespace range = boost::range;

using octet = azmqn::utility::octet;

TEST_CASE("Signature Round Trip", "[wire]") {
    using namespace azmqn::detail::transport;

    wire::mechanism_name_view mechanism("NULL");
    wire::greeting g{ mechanism, true };
    REQUIRE(g.valid());
    {
        auto [vmajor, vminor] = g.version();
        REQUIRE(vmajor == 0x3);
        REQUIRE(vminor == 0x1);
    }
    REQUIRE(mechanism == g.mechanism());
    REQUIRE(g.is_server());

    std::array<octet, wire::greeting::size> a;
    auto const buf = asio::buffer(a.data(), a.size());
    asio::buffer_copy(buf, g.buffer());

    wire::greeting gg{ buf };
    REQUIRE(gg.valid());
    {
        auto [vmajor, vminor] = gg.version();
        REQUIRE(vmajor == 0x3);
        REQUIRE(vminor == 0x1);
    }
    REQUIRE(mechanism == gg.mechanism());
    REQUIRE(gg.is_server());
}

TEST_CASE("Round Trip uint8_t", "[wire]") {
    using namespace azmqn::detail::transport;

    std::array<octet, 1> b;
    b.fill(octet(0));

    wire::put<uint8_t>(asio::buffer(b), 42);
    REQUIRE(b[0] == octet(42));

    const auto [r, _] = wire::get<uint8_t>(asio::buffer(b));
    REQUIRE(r == 42);
}

TEST_CASE("Round Trip uint16_t", "[wire]") {
    using namespace azmqn::detail::transport;

    std::array<octet, sizeof(uint16_t)> b;
    b.fill(octet(0));

    wire::put<uint16_t>(boost::asio::buffer(b), 0xabba);
    REQUIRE(*reinterpret_cast<uint16_t const*>(b.data()) == 0xbaab);
    const auto [r, _] = wire::get<uint16_t>(asio::buffer(b));
    REQUIRE(r == 0xabba);
}

TEST_CASE("Round Trip uint32_t", "[wire]") {
    using namespace azmqn::detail::transport;

    std::array<octet, sizeof(uint32_t)> b;
    b.fill(octet(0));

    wire::put<uint32_t>(boost::asio::buffer(b), 0xabcdef01);
    REQUIRE(*reinterpret_cast<uint32_t const*>(b.data()) == 0x01efcdab);
    const auto [r, _] = wire::get<uint32_t>(asio::buffer(b));
    REQUIRE(r == 0xabcdef01);
}

TEST_CASE("Round Trip uint64_t", "[wire]") {
    using namespace azmqn::detail::transport;

    std::array<octet, sizeof(uint64_t)> b;
    b.fill(octet(0));

    wire::put<uint64_t>(boost::asio::buffer(b), 0xabcdef0102030405);
    REQUIRE(*reinterpret_cast<uint64_t const*>(b.data()) == 0x0504030201efcdab);
    const auto [r, _] = wire::get<uint64_t>(asio::buffer(b));
    REQUIRE(r == 0xabcdef0102030405);
}

TEST_CASE("Static frame operations", "[frames]") {
    using namespace azmqn::detail::transport;

    std::array<octet, 5> b{ octet(0x00), octet(0x03),
                              octet('f'), octet('o'), octet('o') };
    REQUIRE(!wire::is_more(b[0]));
    REQUIRE(!wire::is_long(b[0]));
    REQUIRE(!wire::is_command(b[0]));

    std::array<octet, 5> c{ octet(0x06), octet(0x03),
                              octet('f'), octet('o'), octet('o') };
    REQUIRE(!wire::is_more(c[0]));
    REQUIRE(wire::is_long(c[0]));
    REQUIRE(wire::is_command(c[0]));
}

struct sync_read_stream {
    using bufs_t = std::vector<asio::const_buffer>;
    bufs_t bufs_;
    bufs_t::const_iterator it_;
    asio::const_buffer buf_;

    template<typename BufferSequence>
    void reset(BufferSequence const& bufs) {
        bufs_.clear();
        range::copy(bufs, std::back_inserter(bufs_));
        it_ = std::begin(bufs_);
        buf_ = *it_;
    }

    template<typename MutableBuffer>
    size_t read_some(MutableBuffer buf, boost::system::error_code& ec) {
        size_t bytes_transferred = 0;
        if (it_ != std::end(bufs_)) {
            bytes_transferred = asio::buffer_copy(buf, buf_);
            buf_ = buf_ + bytes_transferred;
            if (asio::buffer_size(buf_) == 0)
                buf_ = *++it_;
        }
        return bytes_transferred;
    }
};

struct sync_write_stream {
    using buf_t = std::vector<octet>;
    buf_t buf_;

    template<typename BufferSequence>
    size_t write_some(BufferSequence const& bufs, boost::system::error_code& ec) {
        size_t bytes_transferred = boost::accumulate(bufs | boost::adaptors::transformed([](auto const& b) {
                                                                    return boost::asio::buffer_size(b);
                                                                }), 0);
        buf_.reserve(buf_.size() + bytes_transferred);
        boost::for_each(bufs, [this](auto const& b) {
                std::copy_n(boost::asio::buffer_cast<octet const*>(b), boost::asio::buffer_size(b),
                                std::back_inserter(buf_));
            });
        return bytes_transferred;
    }
};

TEST_CASE("Framing operations", "[frames]") {
    using namespace azmqn::detail::transport;

    auto REQUIRE_f_is_empty = [](auto& f) {
        REQUIRE(f.bytes_transferred() == 0);
        REQUIRE(f.empty());
        REQUIRE(!f.valid());
    };

    std::array<octet, 5> b{ octet(0x00), octet(0x03),
                              octet('f'), octet('o'), octet('o') };
    {
        wire::frame f;
        REQUIRE_f_is_empty(f);

        sync_read_stream s;
        s.reset(asio::const_buffers_1{boost::asio::buffer(b)});
        auto res = f.read(s);
        REQUIRE(res.has_value());
        REQUIRE(res.value() == 2);
        REQUIRE(f.valid());
        REQUIRE(f.bytes_transferred() == 2);
    }

    {
        wire::frame f;
        REQUIRE_f_is_empty(f);

        auto buf = f.mutable_buffer();
        auto bdata = azmqn::asio::buffer_data(buf);
        bdata[0] = b[0];
        bdata[1] = b[1];
        f.set_size(2);
        REQUIRE(f.valid());
        REQUIRE(!f.empty());
        REQUIRE(f.bytes_transferred() == 2);
        auto const cbuf = f.const_buffer();
        auto const cdata = azmqn::asio::buffer_data(cbuf);
        REQUIRE(cdata[0] == b[0]);
        REQUIRE(cdata[1] == b[1]);
    }
}

TEST_CASE("read short frame", "[frames]") {
    using namespace azmqn::detail;

    sync_read_stream s;
    std::array<octet, 5> b{ octet(0x00), octet(0x03),
                              octet('f'), octet('o'), octet('o') };
    s.reset(asio::const_buffers_1{boost::asio::buffer(b)});

    auto res = transport::read(s, 1024);
    REQUIRE(res.has_value());
}

TEST_CASE("read long frame", "[frames]") {
    using namespace azmqn::detail;

    sync_read_stream s;
    std::array<octet, 512 + transport::wire::max_framing_octets> b{ octet(0x02), octet(0) };
    auto buf = asio::buffer(b) + sizeof(octet);
    transport::wire::put<uint64_t>(buf, b.size() - transport::wire::max_framing_octets);

    std::fill(b.begin() + transport::wire::max_framing_octets, b.end(), octet('o'));
    s.reset(asio::const_buffers_1{boost::asio::buffer(b)});

    auto res = transport::read(s, 1024);
    //REQUIRE(res.message());
}


TEST_CASE("write short frame", "[frames]") {
    using namespace azmqn::detail::transport;

    sync_write_stream s;
    boost::string_view payload{ "foo" };
    wire::writable_message_or_command w{ wire::message_t{ boost::asio::buffer(payload.data(), payload.size()), false } };

    auto res = write(s, w);
    REQUIRE(res.has_value());
    REQUIRE(res.value() == wire::min_framing_octets + payload.size());

    auto buf = *boost::asio::buffer(s.buf_).begin();
    auto bdata = azmqn::asio::buffer_data(buf);
    REQUIRE(wire::is_message(bdata[0]));
    REQUIRE(!wire::is_long(bdata[0]));
    auto [v, _] = wire::get<uint8_t>(boost::asio::buffer(buf + 1));
    REQUIRE(v == payload.size());
}
