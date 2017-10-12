#define BOOST_TEST_BUILD_INFO yes

#include "precompiled.h"
#include <boost/test/unit_test.hpp>
#include "../xutils/io_buff.h"
#include "../xutils/io_buff_reader.h"

using namespace boost::unit_test;
using namespace xutils;

BOOST_AUTO_TEST_SUITE(io_buff_tests)

BOOST_AUTO_TEST_CASE(io_buff_create)
{
    io_buff buf(4_KB);

    BOOST_REQUIRE(!buf.bytes_avail_wr());
    buf.expand_with(4_KB);
    BOOST_REQUIRE(buf.bytes_avail_wr());
}

BOOST_AUTO_TEST_CASE(io_buff_commit_test)
{
    // a simple commit test without reader
    auto bts_avail = 2000;
    io_buff buf(bts_avail);
    uint32_t commit_bytes = 1000;

    // will trigger and assert
    // buf.commit(1);

    buf.expand_with(bts_avail);
    buf.commit(commit_bytes);
    bts_avail -= commit_bytes;
    buf.commit(bts_avail - 1);
    // will trigger an assert
    // buf.commit(1);
}

BOOST_AUTO_TEST_CASE(io_buff_write_block)
{
    uint32_t block_size  = 512;
    uint32_t block_count = 10;
    io_buff buf(block_size);

    BOOST_REQUIRE(buf.begin() == buf.end());

    uint32_t bts_avail = 0;
    for (uint32_t i = 0; i < block_count; ++i)
    {
        buf.expand_with(block_size);
        bts_avail += block_size;
    }
    bts_avail -= 1;

    BOOST_REQUIRE(buf.begin() != buf.end());
    auto block = *buf.begin();
    BOOST_REQUIRE_EQUAL(block.size(), block_size);
    BOOST_REQUIRE(block.data());

    buf.commit(block_size * 2);
    bts_avail -= (block_size * 2);
    BOOST_REQUIRE((*buf.begin()).data());
    BOOST_REQUIRE_EQUAL((*buf.begin()).size(), block_size);
    buf.commit(block_size / 2);
    bts_avail -= (block_size / 2);
    BOOST_REQUIRE((*buf.begin()).data());
    BOOST_REQUIRE_EQUAL((*buf.begin()).size(), (block_size / 2));
    buf.commit(bts_avail);
    BOOST_REQUIRE(buf.begin() == buf.end());
}

BOOST_AUTO_TEST_CASE(io_buff_bytes_avail_wr)
{
    uint32_t block_size  = 512;
    uint32_t block_count = 10;
    io_buff buf(block_size);

    BOOST_CHECK(!buf.bytes_avail_wr());

    for (uint32_t i = 0; i < block_count; ++i)
        buf.expand_with(block_size);

    uint32_t bytes_left = ((block_count * block_size) - 1);
    buf.commit(block_size * 2);
    bytes_left -= (block_size * 2);
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bytes_left);
    buf.commit(1500);
    bytes_left -= 1500;
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bytes_left);
    buf.commit(buf.bytes_avail_wr());
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), 0);
}

BOOST_AUTO_TEST_CASE(io_buff_reg_rdr)
{
    io_buff buf(1);
    io_buff_reader rdr;

    buf.register_reader(rdr);
    // will trigger an assert, trying to register again
    // buf.register_reader(rdr);

    io_buff_reader readers[UINT8_MAX];
    for (uint8_t i = 0; i < (UINT8_MAX - 1); ++i)
        buf.register_reader(readers[i]);

    BOOST_REQUIRE(!buf.register_reader(readers[UINT8_MAX - 1]));
}

BOOST_AUTO_TEST_CASE(io_buff_unreg_rdr)
{
    io_buff buf(1);
    io_buff_reader rdr;

    buf.register_reader(rdr);
    buf.unregister_reader(rdr);

    // will trigger an assert
    // buf.unregister_reader(rdr);
}

BOOST_AUTO_TEST_CASE(io_buff_read_block)
{
    io_buff_reader rdr;
    io_buff buf(4_KB);
    buf.register_reader(rdr);

    buf.expand_with(4_KB);
    BOOST_REQUIRE(rdr.begin() == rdr.end());

    uint32_t commit_bts = 2785;
    buf.commit(commit_bts);
    auto block = *rdr.begin();
    BOOST_REQUIRE_EQUAL(block.size(), commit_bts);
    BOOST_REQUIRE(block.data());

    buf.unregister_reader(rdr);
}

BOOST_AUTO_TEST_CASE(io_buff_consume_commit)
{
    int rw_bytes = 10;
    io_buff buf(100);
    io_buff_reader rdr;
    buf.register_reader(rdr);

    const uint32_t bufsize = 300;
    buf.expand_with(bufsize);

    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bufsize - 1);
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), 0);

    for (int i = 0; i < 10; ++i)
    {
        uint32_t bytes = bufsize;
        while (bytes)
        {
            buf.commit(rw_bytes);
            rdr.consume(rw_bytes);
            bytes -= rw_bytes;
        }
    }
    // assert buffer is empty
    // rdr.consume(1);

    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bufsize - 1);
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), 0);

    // assert r/w head must have one byte diff
    // buf.commit(bufsize);

    buf.commit(bufsize - 1);

    // assert passing writer
    // rdr.consume(bufsize);

    rdr.consume(bufsize - 1);
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bufsize - 1);
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), 0);

    buf.unregister_reader(rdr);
}

BOOST_AUTO_TEST_CASE(io_buff_reader_bytes_avail)
{
    io_buff_reader rdr;
    io_buff buf(4_KB);
    buf.register_reader(rdr);

    buf.expand_with(4_KB);
    uint32_t rd_bts_avail = 2731;
    uint32_t wr_bts_avail = (4_KB - 1);

    buf.commit(rd_bts_avail);
    wr_bts_avail -= rd_bts_avail;
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), rd_bts_avail);

    rdr.consume(1_KB);
    rd_bts_avail -= 1_KB;
    wr_bts_avail += 1_KB;
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), rd_bts_avail);

    // add a second block
    buf.expand_with(4_KB);
    wr_bts_avail += 4_KB;
    buf.commit(wr_bts_avail);
    rd_bts_avail += wr_bts_avail;
    wr_bts_avail = 0;
    BOOST_REQUIRE_EQUAL(rdr.bytes_avail(), rd_bts_avail);

    buf.unregister_reader(rdr);
}

BOOST_AUTO_TEST_CASE(io_buff_expand)
{
    uint32_t blsize = 10;
    io_buff_reader rdr1;
    io_buff_reader rdr2;
    io_buff_reader rdr3;

    io_buff buf(blsize);
    BOOST_CHECK(!buf.bytes_avail_wr());

    buf.register_reader(rdr1);
    buf.register_reader(rdr2);
    buf.register_reader(rdr3);

    uint32_t bufsize = ((2 * blsize) - 1);
    buf.expand_with(blsize * 2);
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), bufsize);
    BOOST_REQUIRE_EQUAL(rdr1.bytes_avail(), 0);
    BOOST_REQUIRE_EQUAL(rdr2.bytes_avail(), 0);
    BOOST_REQUIRE_EQUAL(rdr3.bytes_avail(), 0);

    // move writer to the end
    buf.commit(bufsize);
    BOOST_REQUIRE_EQUAL(buf.bytes_avail_wr(), 0);
    BOOST_REQUIRE_EQUAL(rdr1.bytes_avail(), bufsize);
    BOOST_REQUIRE_EQUAL(rdr2.bytes_avail(), bufsize);
    BOOST_REQUIRE_EQUAL(rdr3.bytes_avail(), bufsize);

    // consume 1-st block from all readers
    rdr1.consume(blsize);
    rdr2.consume(blsize);
    rdr3.consume(blsize);
    auto wr_bytes  = buf.bytes_avail_wr();
    auto rd1_bytes = rdr1.bytes_avail();
    auto rd2_bytes = rdr2.bytes_avail();
    auto rd3_bytes = rdr3.bytes_avail();
    // add new block
    bufsize += blsize;
    buf.expand_with(blsize);
    BOOST_REQUIRE_EQUAL(wr_bytes + blsize, buf.bytes_avail_wr());
    BOOST_REQUIRE_EQUAL(rd1_bytes, rdr1.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd2_bytes, rdr2.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd3_bytes, rdr3.bytes_avail());

    // reverse writer + 8 from 1-st block
    buf.commit(blsize + 8);
    wr_bytes = buf.bytes_avail_wr();
    // reverse readers r1 and r2
    rdr1.consume((bufsize - blsize) + 2);
    rdr2.consume((bufsize - blsize) + 3);
    rd1_bytes = rdr1.bytes_avail();
    rd2_bytes = rdr2.bytes_avail();
    rd3_bytes = rdr3.bytes_avail();
    // add new block
    bufsize += blsize;
    buf.expand_with(blsize);
    BOOST_REQUIRE_EQUAL(wr_bytes + blsize, buf.bytes_avail_wr());
    BOOST_REQUIRE_EQUAL(rd1_bytes, rdr1.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd2_bytes, rdr2.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd3_bytes, rdr3.bytes_avail());

    buf.unregister_reader(rdr1);
    buf.unregister_reader(rdr2);
    buf.unregister_reader(rdr3);

    io_buff buf1(blsize);
    buf1.register_reader(rdr1);
    buf1.register_reader(rdr2);
    buf1.register_reader(rdr3);
    BOOST_CHECK(!buf1.bytes_avail_wr());
    buf1.expand_with(blsize * 2);
    bufsize = ((2 * blsize) - 1);

    std::string data1("abcdefghij");
    std::string data2("klmnopqrst");
    char* bl_ptr1 = (*buf1.begin()).data();
    ::memcpy(bl_ptr1, data1.data(), (*buf1.begin()).size());
    buf1.commit(blsize);
    char* bl_ptr2 = (*buf1.begin()).data();
    BOOST_REQUIRE_NE((void*)bl_ptr1, (void*)bl_ptr2);
    ::memcpy(bl_ptr2, data2.data(), (*buf1.begin()).size());
    buf1.commit(blsize - 1);

    // position readers, must have more than 1 reader bigger than writer
    rdr1.consume(blsize + 5);
    rdr2.consume(blsize + 8);
    rdr3.consume(bufsize);
    // reverse writer
    buf1.commit(blsize - 3);
    rdr3.consume(blsize - 8);

    auto ch1  = (*rdr1.begin()).data()[0];
    auto ch2  = (*rdr2.begin()).data()[0];
    auto ch3  = (*rdr3.begin()).data()[0];
    rd1_bytes = rdr1.bytes_avail();
    rd2_bytes = rdr2.bytes_avail();
    rd3_bytes = rdr3.bytes_avail();
    wr_bytes  = buf1.bytes_avail_wr();
    bufsize += blsize;
    // add new block
    buf1.expand_with(blsize);
    BOOST_REQUIRE_EQUAL(wr_bytes + blsize, buf1.bytes_avail_wr());
    BOOST_REQUIRE_EQUAL(rd1_bytes, rdr1.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd2_bytes, rdr2.bytes_avail());
    BOOST_REQUIRE_EQUAL(rd3_bytes, rdr3.bytes_avail());
    auto test_ch1 = (*rdr1.begin()).data()[0];
    auto test_ch2 = (*rdr2.begin()).data()[0];
    auto test_ch3 = (*rdr3.begin()).data()[0];
    BOOST_REQUIRE_EQUAL(ch1, test_ch1);
    BOOST_REQUIRE_EQUAL(ch2, test_ch2);
    BOOST_REQUIRE_EQUAL(ch3, test_ch3);

    buf1.unregister_reader(rdr1);
    buf1.unregister_reader(rdr2);
    buf1.unregister_reader(rdr3);
}

BOOST_AUTO_TEST_CASE(io_buff_write_iterator)
{
    uint32_t block_count = 5;
    uint32_t block_size = 1000;
    io_buff buf(block_size);

    // buffer is empty
    BOOST_REQUIRE(buf.begin() == buf.end());

    // single block test
    buf.expand_with(block_size);
    auto it = buf.begin();
    BOOST_REQUIRE(it != buf.end());
    BOOST_REQUIRE((*it).data());
    BOOST_REQUIRE_EQUAL((*it).size(), block_size - 1);
    it++;
    BOOST_REQUIRE(buf.end() == it);
    // will trigger an assert
    // it++;

    // multiple blocks test
    for (uint32_t i = 0; i < (block_count - 1); i++)
        buf.expand_with(block_size);

    auto loop_test = [&buf](uint32_t block_count, uint32_t block_size)
    {
        char* prev_ptr = nullptr;
        auto it = buf.begin();
        for (uint32_t i = 0; i < block_count; i++, it++)
        {
            auto size_to_check =
                (i < (block_count - 1) ? block_size : block_size - 1);
            BOOST_REQUIRE(buf.end() != it);
            BOOST_REQUIRE((*it).data());
            BOOST_REQUIRE_EQUAL((*it).size(), size_to_check);
            if (prev_ptr)
            {
                BOOST_REQUIRE_NE((void*)prev_ptr, (void*)(*it).data());
                prev_ptr = (*it).data();
            }
            else
            {
                prev_ptr = (*it).data();
            }
        }
        BOOST_REQUIRE(buf.end() == it);
    };

    loop_test(block_count, block_size);
    // commit 1 block test
    buf.commit(block_size);
    loop_test(block_count - 1, block_size);

    // commit to last block test
    buf.commit(((block_count - 1) * block_size) - 10);
    it = buf.begin();
    BOOST_REQUIRE((*it).data());
    BOOST_REQUIRE_EQUAL((*it).size(), 9);
    it++;
    BOOST_REQUIRE(buf.end() == it);

    // buffer is full
    buf.commit(9);
    it = buf.begin();
    BOOST_REQUIRE(buf.end() == it);
}

BOOST_AUTO_TEST_CASE(io_buff_read_iterator)
{
    uint32_t block_count = 5;
    uint32_t block_size = 10;
    io_buff buf(block_size);
    io_buff_reader rdr;
    std::string test_data = {"abcdef"};

    buf.register_reader(rdr);
    BOOST_REQUIRE(rdr.begin() == rdr.end());

    // single block test
    buf.expand_with(block_size);
    (*buf.begin()).data()[0] = test_data[0];
    buf.commit(block_size - 1);
    auto it = rdr.begin();
    BOOST_REQUIRE(rdr.end() != it);
    BOOST_REQUIRE((*it).data());
    BOOST_REQUIRE_EQUAL((*it).size(), block_size - 1);
    BOOST_REQUIRE_EQUAL((*it).data()[0], test_data[0]);
    it++;
    BOOST_REQUIRE(rdr.end() == it);
    // will trigger an assert
    // it++;

    // multiple blocks test
    for (uint32_t i = 0; i < (block_count - 1); i++)
    {
        buf.expand_with(block_size);
    }

    // jump to next block
    buf.commit(1);
    for (uint32_t i = 0; i < (block_count - 1); i++)
    {
        (*buf.begin()).data()[0] = test_data[i + 1];
        auto to_commit = (i < (block_count - 2) ? block_size : block_size - 1);
        buf.commit(to_commit);
    }

    auto loop_test = [&rdr, &test_data](uint32_t block_count,
                                        uint32_t block_size, int data_off)
    {
        auto it = rdr.begin();
        for (uint32_t i = 0; i < block_count; i++, it++)
        {
            auto size_to_check =
                (i < (block_count - 1) ? block_size : block_size - 1);
            BOOST_REQUIRE(rdr.end() != it);
            BOOST_REQUIRE((*it).data());
            BOOST_REQUIRE_EQUAL((*it).size(), size_to_check);
            BOOST_REQUIRE_EQUAL((*it).data()[0], test_data[i + data_off]);
        }
        BOOST_REQUIRE(rdr.end() == it);
    };

    loop_test(block_count, block_size, 0);
    // consume 1 block test
    rdr.consume(block_size);
    loop_test(block_count - 1, block_size, 1);

    // consume to last block test
    rdr.consume(((block_count - 1) * block_size) - 10);
    it = rdr.begin();
    BOOST_REQUIRE((*it).data());
    BOOST_REQUIRE_EQUAL((*it).size(), 9);
    it++;
    BOOST_REQUIRE(rdr.end() == it);

    rdr.consume(9);
    it = rdr.begin();
    BOOST_REQUIRE(rdr.end() == it);
}

BOOST_AUTO_TEST_SUITE_END()
