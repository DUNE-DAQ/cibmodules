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

#include "boost/test/unit_test.hpp"

BOOST_AUTO_TEST_SUITE(CIB_test)

BOOST_AUTO_TEST_CASE(BitmaskHandlesBounds)
{
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(7, 0), 0xFFU);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(0, 7), 0xFFU);
  BOOST_REQUIRE_EQUAL(dunedaq::cibmodules::util::bitmask(5, 3), 0x38U);
}

BOOST_AUTO_TEST_CASE(ParseHexValidInputs)
{
  std::uint32_t value = 0;

  BOOST_REQUIRE(dunedaq::cibmodules::util::parse_hex("0x1A2B", value));
  BOOST_REQUIRE_EQUAL(value, 0x1A2BU);

  BOOST_REQUIRE(dunedaq::cibmodules::util::parse_hex("ff", value));
  BOOST_REQUIRE_EQUAL(value, 0xFFU);
}

BOOST_AUTO_TEST_CASE(ParseHexRejectsInvalidInputs)
{
  std::uint32_t value = 0;

  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("0x", value));
  BOOST_REQUIRE(!dunedaq::cibmodules::util::parse_hex("0x1G", value));
}

BOOST_AUTO_TEST_SUITE_END()
