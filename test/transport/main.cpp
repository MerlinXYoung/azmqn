#include <azmqn/detail/wire.hpp>
#include <azmqn/detail/frames.hpp>

#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <vector>
#include <iterator>

#define CATCH_CONFIG_MAIN
#include <test/catch.hpp>

namespace asio = boost::asio;
namespace range = boost::range;

using namespace azmqn::detail::transport;

TEST_CASE("Round Trip uint8_t", "[wire]") {
    std::array<octet_t, 1> b{ 0 };
    wire::put_uint8(asio::buffer(b), 42);
    REQUIRE(b[0] == 42);

    const auto [r, _] = wire::get_uint8(asio::buffer(b));
    REQUIRE(r == 42);
}

TEST_CASE("Round Trip uint16_t", "[wire]") {
    std::array<octet_t, sizeof(uint16_t)> b{ 0, 0 };
    wire::put_uint16(boost::asio::buffer(b), 0xabba);
    REQUIRE(*reinterpret_cast<uint16_t const*>(b.data()) == 0xbaab);
    const auto [r, _] = wire::get_uint16(asio::buffer(b));
    REQUIRE(r == 0xabba);
}

TEST_CASE("Round Trip uint32_t", "[wire]") {
    std::array<octet_t, sizeof(uint32_t)> b{ 0, 0, 0, 0 };
    wire::put_uint32(boost::asio::buffer(b), 0xabcdef01);
    REQUIRE(*reinterpret_cast<uint32_t const*>(b.data()) == 0x01efcdab);
    const auto [r, _] = wire::get_uint32(asio::buffer(b));
    REQUIRE(r == 0xabcdef01);
}

TEST_CASE("Round Trip uint64_t", "[wire]") {
    std::array<octet_t, sizeof(uint64_t)> b{ 0, 0, 0, 0 };
    wire::put_uint64(boost::asio::buffer(b), 0xabcdef0102030405);
    REQUIRE(*reinterpret_cast<uint64_t const*>(b.data()) == 0x0504030201efcdab);
    const auto [r, _] = wire::get_uint64(asio::buffer(b));
    REQUIRE(r == 0xabcdef0102030405);
}

TEST_CASE("Static frame operations", "[frames]") {
    std::array<octet_t, 5> b{ 0x00, 0x03, 'f', 'o', 'o' };
    REQUIRE(!wire::is_more(b[0]));
    REQUIRE(!wire::is_long(b[0]));
    REQUIRE(!wire::is_command(b[0]));

    std::array<octet_t, 5> c{ 0x06, 0x03, 'f', 'o', 'o' };
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

TEST_CASE("read short frame", "[frames]") {
    sync_read_stream s;
    std::array<octet_t, 5> b{ 0x00, 0x03, 'f', 'o', 'o' };
    s.reset(asio::const_buffers_1{boost::asio::buffer(b)});

    boost::system::error_code ec;
    auto res = framing::read(s, ec);
    REQUIRE_NOTHROW(boost::get<framing::message_t>(res));
}

TEST_CASE("read long frame", "[frames]") {
    sync_read_stream s;
    std::array<octet_t, 512 + wire::max_framing_octets> b{ 0x02, 0};
    auto buf = asio::buffer(b) + sizeof(octet_t);
    wire::put_uint64(buf, b.size() - wire::max_framing_octets);

    std::fill(b.begin() + wire::max_framing_octets, b.end(), 'o');
    s.reset(asio::const_buffers_1{boost::asio::buffer(b)});

    boost::system::error_code ec;
    auto res = framing::read(s, ec);
    //REQUIRE_NOTHROW(boost::get<framing::message_t>(res));
}
