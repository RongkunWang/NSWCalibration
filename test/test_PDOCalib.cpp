/// Test suite for testing PDOCalib class

#include "NSWCalibration/PDOCalib.h"

#define BOOST_TEST_MODULE PDOCalib_tests
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

using namespace std::chrono_literals;

bool operator==(const nsw::PDOCalib::RunParameters& lhs, const nsw::PDOCalib::RunParameters& rhs)
{
  return (lhs.trecord == rhs.trecord) && (lhs.channels == rhs.channels) && (lhs.values == rhs.values);
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_Correct)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("4,1,2,3,4,5,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {1,2,3,4,5}, 4}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_PDOParams_Correct)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("4,100,200,300,400,500,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {100,200,300,400,500}, 4}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidGroup)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("INV,1,2,3,4,5,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {1,2,3,4,5}, nsw::PDOCalib::DEFAULT_NUM_CH_PER_GROUP}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_GroupOutOfRange)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("800000000000000000000000,1,2,3,4,5,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {1,2,3,4,5}, nsw::PDOCalib::DEFAULT_NUM_CH_PER_GROUP}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidRegisterValue)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,INV,4,5,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {1,2,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_RegisterValueOutOfRange)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,300000000000000000000,4,5,*6000*") ==
              nsw::PDOCalib::RunParameters{6000ms, {1,2,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_TimeOutOfRange)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,*600000000000000*") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_NegativeTime)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,*-6000*") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidTime1)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,*6000") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidTime2)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,6000*") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidTime3)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,**") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidTime4)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,*") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_TDOParams_InvalidTime5)
{
  BOOST_TEST((nsw::PDOCalib::parseCalibParams("8,1,2,3,4,5,*BADTIME*") ==
              nsw::PDOCalib::RunParameters{nsw::PDOCalib::DEFAULT_TRECORD, {1,2,3,4,5}, 8}));
}
