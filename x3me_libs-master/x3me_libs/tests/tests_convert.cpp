#include <string>

#include <boost/test/unit_test.hpp>

#include <endian.h>
#include "../convert.h"

using namespace x3me::convert;

BOOST_AUTO_TEST_SUITE(tests_convert)

BOOST_AUTO_TEST_CASE(max_num_digits)
{
    using x3me::mpl::max_num_digits;
    BOOST_CHECK_EQUAL(max_num_digits<int8_t>(), 3);
    BOOST_CHECK_EQUAL(max_num_digits<uint8_t>(), 3);
    BOOST_CHECK_EQUAL(max_num_digits<int16_t>(), 5);
    BOOST_CHECK_EQUAL(max_num_digits<uint16_t>(), 5);
    BOOST_CHECK_EQUAL(max_num_digits<int32_t>(), 10);
    BOOST_CHECK_EQUAL(max_num_digits<uint32_t>(), 10);
    BOOST_CHECK_EQUAL(max_num_digits<int64_t>(), 19);
    BOOST_CHECK_EQUAL(max_num_digits<uint64_t>(), 20);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T, size_t BuffSize>
void test_int_to_str(T rand_value_1, T rand_value_2)
{
    const T min_value = std::numeric_limits<T>::min();
    const T max_value = std::numeric_limits<T>::max();

    auto min_value_str    = std::to_string(min_value);
    auto max_value_str    = std::to_string(max_value);
    auto rand_value_1_str = std::to_string(rand_value_1);
    auto rand_value_2_str = std::to_string(rand_value_2);

    char buff_1[BuffSize];

    auto chars = int_to_str_l(min_value, buff_1);
    BOOST_CHECK_EQUAL(chars, min_value_str.size());
    BOOST_CHECK_EQUAL(min_value_str, buff_1);

    chars = int_to_str_l(max_value, buff_1);
    BOOST_CHECK_EQUAL(chars, max_value_str.size());
    BOOST_CHECK_EQUAL(max_value_str, buff_1);

    chars = int_to_str_l(rand_value_1, buff_1);
    BOOST_CHECK_EQUAL(chars, rand_value_1_str.size());
    BOOST_CHECK_EQUAL(rand_value_1_str, buff_1);

    chars = int_to_str_l(rand_value_2, buff_1);
    BOOST_CHECK_EQUAL(chars, rand_value_2_str.size());
    BOOST_CHECK_EQUAL(rand_value_2_str, buff_1);

    std::array<char, BuffSize> buff_2;

    chars = int_to_str_l(min_value, buff_2);
    BOOST_CHECK_EQUAL(chars, min_value_str.size());
    BOOST_CHECK_EQUAL(min_value_str, buff_2.data());

    chars = int_to_str_l(max_value, buff_2);
    BOOST_CHECK_EQUAL(chars, max_value_str.size());
    BOOST_CHECK_EQUAL(max_value_str, buff_2.data());

    chars = int_to_str_l(rand_value_1, buff_2);
    BOOST_CHECK_EQUAL(chars, rand_value_1_str.size());
    BOOST_CHECK_EQUAL(rand_value_1_str, buff_2.data());

    chars = int_to_str_l(rand_value_2, buff_2);
    BOOST_CHECK_EQUAL(chars, rand_value_2_str.size());
    BOOST_CHECK_EQUAL(rand_value_2_str, buff_2.data());
}

BOOST_AUTO_TEST_CASE(int8_to_str)
{
    test_int_to_str<int8_t, 5>(-78, 56);
}

BOOST_AUTO_TEST_CASE(uint8_to_str)
{
    test_int_to_str<uint8_t, 4>(178, 5);
}

BOOST_AUTO_TEST_CASE(int16_to_str)
{
    test_int_to_str<int16_t, 7>(-31345, 12345);
}

BOOST_AUTO_TEST_CASE(uint16_to_str)
{
    test_int_to_str<uint16_t, 6>(31345, 62345);
}

BOOST_AUTO_TEST_CASE(int32_to_str)
{
    test_int_to_str<int32_t, 12>(-1987654321, 2000000000);
}

BOOST_AUTO_TEST_CASE(uint32_to_str)
{
    test_int_to_str<uint32_t, 11>(2123456789, 4000111222);
}

BOOST_AUTO_TEST_CASE(int64_to_str)
{
    test_int_to_str<int64_t, 21>(-1987654345321, 2000);
}

BOOST_AUTO_TEST_CASE(uint64_to_str)
{
    test_int_to_str<uint64_t, 21>(2123456783456789, 6);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename TestedFunc, typename CorrectFunc>
bool test_htobe_num(T beg, T end, TestedFunc tfunc, CorrectFunc cfunc)
{
    for (T i = beg; i < end; ++i)
    {
        if (tfunc(i) != cfunc(i))
            return false;
    }
    return true;
}

template <typename T, typename TestedFunc, typename CorrectFunc>
bool test_betoh_num(T beg, T end, TestedFunc tfunc, CorrectFunc cfunc)
{
    for (T i = beg; i < end; ++i)
    {
        if (tfunc(i) != cfunc(i))
            return false;
    }
    return true;
}

template <typename T, typename Buff, typename TestedFunc, typename CorrectFunc>
bool test_write_htobe(T beg, T end, Buff& buff, TestedFunc tfunc,
                      CorrectFunc cfunc)
{
    for (T i = beg; i < end; ++i)
    {
        tfunc(i, buff);
        T n;
        std::memcpy(&n, x3me::convert::detail::data(buff), sizeof(T));
        if (i != cfunc(n))
            return false;
    }
    return true;
}

template <typename T, typename Buff, typename TestedFunc, typename CorrectFunc>
bool test_write_htobe_unsafe(T beg, T end, Buff* ptr, TestedFunc tfunc,
                             CorrectFunc cfunc)
{
    for (T i = beg; i < end; ++i)
    {
        tfunc(i, ptr);
        T n;
        std::memcpy(&n, ptr, sizeof(T));
        if (i != cfunc(n))
            return false;
    }
    return true;
}

template <typename T, typename Buff, typename TestedFunc, typename CorrectFunc>
bool test_read_betoh(T beg, T end, Buff* ptr, TestedFunc tfunc,
                     CorrectFunc cfunc)
{
    for (T i = beg; i < end; ++i)
    {
        cfunc(i, ptr);
        auto n = tfunc(ptr);
        if (i != n)
            return false;
    }
    return true;
}

BOOST_AUTO_TEST_CASE(host_to_network_number)
{
    BOOST_CHECK(test_htobe_num<uint16_t>(12345, 13321, &htobe16_num<uint16_t>,
                                         [](uint16_t n) -> uint16_t
                                         {
                                             return htobe16(n);
                                         }));
    BOOST_CHECK(test_htobe_num<int16_t>(-2345, 2345, &htobe16_num<int16_t>,
                                        [](int16_t n) -> int16_t
                                        {
                                            return htobe16(n);
                                        }));
    BOOST_CHECK(test_htobe_num<int32_t>(
        -2222444, -2222222, &htobe32_num<int32_t>, [](int32_t n) -> int32_t
        {
            return htobe32(n);
        }));
    BOOST_CHECK(test_htobe_num<uint32_t>(
        2222222, 2222555, &htobe32_num<uint32_t>, [](uint32_t n) -> uint32_t
        {
            return htobe32(n);
        }));
    BOOST_CHECK(test_htobe_num<int64_t>(-45562222444, -45562222222,
                                        &htobe64_num<int64_t>,
                                        [](int64_t n) -> int64_t
                                        {
                                            return htobe64(n);
                                        }));
    BOOST_CHECK(test_htobe_num<uint64_t>(78992222222, 78992222555,
                                         &htobe64_num<uint64_t>,
                                         [](uint64_t n) -> uint64_t
                                         {
                                             return htobe64(n);
                                         }));
}

BOOST_AUTO_TEST_CASE(network_to_host_number)
{
    BOOST_CHECK(test_betoh_num<uint16_t>(12345, 13321, &be16toh_num<uint16_t>,
                                         [](uint16_t n) -> uint16_t
                                         {
                                             return be16toh(n);
                                         }));
    BOOST_CHECK(test_betoh_num<int16_t>(-2345, 2345, &be16toh_num<int16_t>,
                                        [](int16_t n) -> int16_t
                                        {
                                            return be16toh(n);
                                        }));
    BOOST_CHECK(test_betoh_num<int32_t>(
        -2222444, -2222222, &be32toh_num<int32_t>, [](int32_t n) -> int32_t
        {
            return be32toh(n);
        }));
    BOOST_CHECK(test_betoh_num<uint32_t>(
        2222222, 2222555, &be32toh_num<uint32_t>, [](uint32_t n) -> uint32_t
        {
            return be32toh(n);
        }));
    BOOST_CHECK(test_betoh_num<int64_t>(-45562222444, -45562222222,
                                        &be64toh_num<int64_t>,
                                        [](int64_t n) -> int64_t
                                        {
                                            return be64toh(n);
                                        }));
    BOOST_CHECK(test_betoh_num<uint64_t>(78992222222, 78992222555,
                                         &be64toh_num<uint64_t>,
                                         [](uint64_t n) -> uint64_t
                                         {
                                             return be64toh(n);
                                         }));
}

BOOST_AUTO_TEST_CASE(host_to_network_number_buff_safe)
{
    char c_arr_buff[8];
    std::array<char, 8> cpp_arr_buff;
    BOOST_CHECK(
        test_write_htobe<uint16_t>(12345, 13321, cpp_arr_buff,
                                   [](uint16_t n, decltype(cpp_arr_buff)& b)
                                   {
                                       write_htobe16(n, b);
                                   },
                                   [](uint16_t n) -> uint16_t
                                   {
                                       return htobe16(n);
                                   }));
    BOOST_CHECK(test_write_htobe<int16_t>(-2345, 2345, c_arr_buff,
                                          [](int16_t n, decltype(c_arr_buff)& b)
                                          {
                                              write_htobe16(n, b);
                                          },
                                          [](int16_t n) -> int16_t
                                          {
                                              return htobe16(n);
                                          }));

    BOOST_CHECK(
        test_write_htobe<int32_t>(-2222444, -2222222, cpp_arr_buff,
                                  [](int32_t n, decltype(cpp_arr_buff)& b)
                                  {
                                      write_htobe32(n, b);
                                  },
                                  [](int32_t n) -> int32_t
                                  {
                                      return htobe32(n);
                                  }));

    BOOST_CHECK(
        test_write_htobe<uint32_t>(2222222, 2222555, c_arr_buff,
                                   [](uint32_t n, decltype(c_arr_buff)& b)
                                   {
                                       write_htobe32(n, b);
                                   },
                                   [](uint32_t n) -> uint32_t
                                   {
                                       return htobe32(n);
                                   }));

    BOOST_CHECK(
        test_write_htobe<int64_t>(-45562222444, -45562222222, cpp_arr_buff,
                                  [](int64_t n, decltype(cpp_arr_buff)& b)
                                  {
                                      write_htobe64(n, b);
                                  },
                                  [](int64_t n) -> int64_t
                                  {
                                      return htobe64(n);
                                  }));

    BOOST_CHECK(
        test_write_htobe<uint64_t>(78992222222, 78992222555, c_arr_buff,
                                   [](uint64_t n, decltype(c_arr_buff)& b)
                                   {
                                       write_htobe64(n, b);
                                   },
                                   [](uint64_t n) -> uint64_t
                                   {
                                       return htobe64(n);
                                   }));
}

BOOST_AUTO_TEST_CASE(host_to_network_number_buff_unsafe)
{
    char buff[8];
    BOOST_CHECK(test_write_htobe_unsafe<uint16_t>(12345, 13321, buff,
                                                  [](uint16_t n, char* b)
                                                  {
                                                      write_htobe16_unsafe(n,
                                                                           b);
                                                  },
                                                  [](uint16_t n) -> uint16_t
                                                  {
                                                      return htobe16(n);
                                                  }));
    BOOST_CHECK(test_write_htobe_unsafe<int16_t>(-2345, 2345, buff,
                                                 [](int16_t n, char* b)
                                                 {
                                                     write_htobe16_unsafe(n, b);
                                                 },
                                                 [](int16_t n) -> int16_t
                                                 {
                                                     return htobe16(n);
                                                 }));
    BOOST_CHECK(test_write_htobe_unsafe<int32_t>(-2222444, -2222222, buff,
                                                 [](int32_t n, char* b)
                                                 {
                                                     write_htobe32_unsafe(n, b);
                                                 },
                                                 [](int32_t n) -> int32_t
                                                 {
                                                     return htobe32(n);
                                                 }));
    BOOST_CHECK(test_write_htobe_unsafe<uint32_t>(2222222, 2222555, buff,
                                                  [](uint32_t n, char* b)
                                                  {
                                                      write_htobe32_unsafe(n,
                                                                           b);
                                                  },
                                                  [](uint32_t n) -> uint32_t
                                                  {
                                                      return htobe32(n);
                                                  }));
    BOOST_CHECK(test_write_htobe_unsafe<int64_t>(-45562222444, -45562222222,
                                                 buff,
                                                 [](int64_t n, char* b)
                                                 {
                                                     write_htobe64_unsafe(n, b);
                                                 },
                                                 [](int64_t n) -> int64_t
                                                 {
                                                     return htobe64(n);
                                                 }));
    BOOST_CHECK(
        test_write_htobe_unsafe<uint64_t>(78992222222, 78992222555, buff,
                                          [](uint64_t n, char* b)
                                          {
                                              write_htobe64_unsafe(n, b);
                                          },
                                          [](uint64_t n) -> uint64_t
                                          {
                                              return htobe64(n);
                                          }));
}

BOOST_AUTO_TEST_CASE(network_buff_to_host_number)
{
    char buff[8];
    BOOST_CHECK(
        test_read_betoh<uint16_t>(12345, 13321, buff,
                                  [](const char* b)
                                  {
                                      return read_be16toh_unsafe<uint16_t>(b);
                                  },
                                  [](uint16_t n, char* b)
                                  {
                                      write_htobe16_unsafe(n, b);
                                  }));
    BOOST_CHECK(
        test_read_betoh<int16_t>(-2345, 2345, buff,
                                 [](const char* b)
                                 {
                                     return read_be16toh_unsafe<int16_t>(b);
                                 },
                                 [](int16_t n, char* b)
                                 {
                                     write_htobe16_unsafe(n, b);
                                 }));
    BOOST_CHECK(
        test_read_betoh<int32_t>(-2222444, -2222222, buff,
                                 [](const char* b)
                                 {
                                     return read_be32toh_unsafe<int32_t>(b);
                                 },
                                 [](int32_t n, char* b)
                                 {
                                     write_htobe32_unsafe(n, b);
                                 }));
    BOOST_CHECK(
        test_read_betoh<uint32_t>(2222222, 2222555, buff,
                                  [](const char* b)
                                  {
                                      return read_be32toh_unsafe<uint32_t>(b);
                                  },
                                  [](uint32_t n, char* b)
                                  {
                                      write_htobe32_unsafe(n, b);
                                  }));
    BOOST_CHECK(
        test_read_betoh<int64_t>(-45562222444, -45562222222, buff,
                                 [](const char* b)
                                 {
                                     return read_be64toh_unsafe<int64_t>(b);
                                 },
                                 [](int64_t n, char* b)
                                 {
                                     write_htobe64_unsafe(n, b);
                                 }));
    BOOST_CHECK(
        test_read_betoh<uint64_t>(78992222222, 78992222555, buff,
                                  [](const char* b)
                                  {
                                      return read_be64toh_unsafe<uint64_t>(b);
                                  },
                                  [](uint64_t n, char* b)
                                  {
                                      write_htobe64_unsafe(n, b);
                                  }));
}

BOOST_AUTO_TEST_SUITE_END()
