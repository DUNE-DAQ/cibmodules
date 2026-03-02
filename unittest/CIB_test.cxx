/**
 * @file CIB_test.cxx
 *
 * Minimal unit tests for cibmodules utility helpers.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE CIB_test // NOLINT

#include "cib_utilities.h"

#include <cstdint>

#include "boost/test/unit_test.hpp"

BOOST_AUTO_TEST_SUITE(CIB_test)

BOOST_AUTO_TEST_CASE(BitmaskHandlesBounds)
{
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(7, 0), 0xFFU);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(0, 7), 0xFFU);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(5, 3), 0x38U);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(31, 31), 0x80000000U);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(31, 0), 0xFFFFFFFFU);
}

BOOST_AUTO_TEST_CASE(CastToSignedHandlesPositiveAndNegative)
{
  constexpr std::uint32_t m1_mask = dunedaq::cib::daq::iols_trigger_t::bitmask_m1;

  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::cast_to_signed(0x000123U, m1_mask), 0x123);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::cast_to_signed(0x3FFFFFU, m1_mask), -1);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::cast_to_signed(0x200001U, m1_mask), -2097151);
}

BOOST_AUTO_TEST_CASE(GetMHelpersDecodeTriggerFields)
{
  dunedaq::cib::daq::iols_trigger_t t{};

  t.pos_m1 = 0x3FFFFF;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m1(t), -1);

  t.pos_m1 = 0x000123;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m1(t), 0x123);

  t.pos_m3 = 0x1FFFF;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m3(t), -1);

  t.pos_m3 = 0x01234;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m3(t), 0x1234);

  constexpr std::uint32_t m2_negative = 0x3FFFFF;
  t.pos_m2_lsb = m2_negative & 0x7FFF;
  t.pos_m2_msb = (m2_negative >> 15) & 0x7F;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m2(t), -1);

  constexpr std::uint32_t m2_positive = 0x001234;
  t.pos_m2_lsb = m2_positive & 0x7FFF;
  t.pos_m2_msb = (m2_positive >> 15) & 0x7F;
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::get_m2(t), 0x1234);
}

BOOST_AUTO_TEST_CASE(ParseHexValidInputs)
{
  std::uint32_t value = 0;

  BOOST_REQUIRE(dunedaq::cibmodules::util::parse_hex("0x1A2B", value));
  BOOST_REQUIRE_EQUAL(value, 0x1A2BU);

  BOOST_REQUIRE(dunedaq::cibmodules::util::parse_hex("0Xabc", value));
  BOOST_REQUIRE_EQUAL(value, 0xABCU);

  BOOST_REQUIRE(dunedaq::cibmodules::util::parse_hex("ff", value));
  BOOST_REQUIRE_EQUAL(value, 0xFFU);
}

BOOST_AUTO_TEST_CASE(ParseHexRejectsInvalidInputs)
{
  std::uint32_t value = 0;

  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("0x", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("0x1G", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("1F ", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex(" 1F", value));
}

BOOST_AUTO_TEST_SUITE_END()
