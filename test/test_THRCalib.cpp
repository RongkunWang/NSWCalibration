/// Test suite for testing THRCalib class

#include <iostream>

#include "NSWCalibration/THRCalib.h"

#define BOOST_TEST_MODULE THRCalib_tests
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

void ptree_sort_vmms(pt::ptree& input, const std::string& board, const std::size_t vmms)
{
  // Ensure that the ordering of the keys we're comparing is identical
  input.sort();
  auto& this_feb = input.get_child(board);
  this_feb.sort();

  for (std::size_t vmmid{0}; vmmid < vmms; ++vmmid) {
    if (this_feb.count(fmt::format("vmm{}", vmmid)) == 0) {
      continue;
    }
    input.get_child(fmt::format("{}.vmm{}", board, vmmid)).sort();
  }
}

BOOST_AUTO_TEST_CASE(UpdateTrimmerConfig_VmmChannelArrayToValue)
{
  // Test that replacing a VMM channel array with a single value works
  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0":{
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,0,0,0,0,0,0,0]
        }
    }
})");
  pt::read_json(input, input_ptree);


  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId": "FEB_NAME",
    "vmm0": {
        "channel_st": 1,
        "channel_sm": 0
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream( R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0": {
            "channel_st": 1,
            "channel_sm": 0
        }
    }
})");
  pt::read_json(expected, expected_ptree);

  nsw::THRCalib::updatePtreeWithFeb(input_ptree, update_ptree);
  ptree_sort_vmms(input_ptree, "BOARD_NAME", 4);
  ptree_sort_vmms(expected_ptree, "BOARD_NAME", 4);

  // Test that updated ptree now contains the VMM we added
  auto& test_feb = input_ptree.get_child("BOARD_NAME");
  BOOST_TEST(test_feb.count("vmm0") == 1);

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_st") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_st"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_sm") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_sm"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0") == expected_ptree.get_child("BOARD_NAME.vmm0"));

  BOOST_TEST(input_ptree == expected_ptree);
}

BOOST_AUTO_TEST_CASE(UpdateTrimmerConfig_VmmChannelValueToArray)
{
  // Test that replacing a VMM channel single value setting with and array works
  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0":{
            "channel_st": 1,
            "channel_sm": 0
        }
    }
})");
  pt::read_json(input, input_ptree);


  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId": "FEB_NAME",
    "vmm0": {
        "channel_st": [1,1,1,1,1,1,1,1],
        "channel_sm": [0,0,0,0,0,0,0,0]
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream( R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0": {
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,0,0,0,0,0,0,0]
        }
    }
})");
  pt::read_json(expected, expected_ptree);

  nsw::THRCalib::updatePtreeWithFeb(input_ptree, update_ptree);
  ptree_sort_vmms(input_ptree, "BOARD_NAME", 4);
  ptree_sort_vmms(expected_ptree, "BOARD_NAME", 4);

  // Test that updated ptree now contains the vmm we added
  auto& test_feb = input_ptree.get_child("BOARD_NAME");
  BOOST_TEST(test_feb.count("vmm0") == 1);

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_st") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_st"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_sm") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_sm"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0") == expected_ptree.get_child("BOARD_NAME.vmm0"));

  BOOST_TEST(input_ptree == expected_ptree);
}

BOOST_AUTO_TEST_CASE(UpdateTrimmerConfig_VmmChannelArrayToArray)
{
  // Test that modifying the VMM channel array setting works
  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0":{
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,0,0,0,0,0,0,0]
        }
    }
})");
  pt::read_json(input, input_ptree);


  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId": "FEB_NAME",
    "vmm0": {
        "channel_st": [1,1,1,1,1,1,1,1],
        "channel_sm": [0,1,0,1,0,1,0,0]
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream( R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0": {
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,1,0,1,0,1,0,0]
        }
    }
})");
  pt::read_json(expected, expected_ptree);

  nsw::THRCalib::updatePtreeWithFeb(input_ptree, update_ptree);
  ptree_sort_vmms(input_ptree, "BOARD_NAME", 4);
  ptree_sort_vmms(expected_ptree, "BOARD_NAME", 4);

  // Test that updated ptree now contains the vmm we added
  auto& test_feb = input_ptree.get_child("BOARD_NAME");
  BOOST_TEST(test_feb.count("vmm0") == 1);

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_st") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_st"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_sm") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_sm"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0") == expected_ptree.get_child("BOARD_NAME.vmm0"));

  BOOST_TEST(input_ptree == expected_ptree);
}

BOOST_AUTO_TEST_CASE(UpdateTrimmerConfig_MultipleVmmUpdates)
{

  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD1_NAME": {
        "OpcNodeId":"FEB1_NAME"
    },
    "BOARD2_NAME": {
        "OpcNodeId":"FEB2_NAME"
    },
    "BOARD3_NAME": {
        "OpcNodeId":"FEB3_NAME"
    },
    "BOARD_NAME": {
        "OpcNodeId":"FEB_NAME",
        "vmm0":{
            "st": 235,
            "channel_st": 1,
            "channel_sm": 0
        },
        "vmm2":{
            "st": 300,
            "channel_st": 1,
            "channel_sm": 0
        },
        "vmm3":{
            "st": 250,
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,0,0,0,0,0,0,0]
        }
    }
})");
  pt::read_json(input, input_ptree);

  // Test that original ptree does not contain the VMM we will add
  auto& test_feb = input_ptree.get_child("BOARD_NAME");
  BOOST_TEST(test_feb.count("vmm1") == 0);

  // Add "vmm1"
  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId":"FEB_NAME",
    "vmm0":{
            "st": 275,
            "channel_st": [1,1,0,1,1,1,1,1],
            "channel_sm": [0,0,1,0,0,0,0,0]
    },
    "vmm1":{
            "st": 278,
            "sdp_dac": 15,
            "channel_st": [1,1,0,1,0,1,1,1],
            "channel_sm": [0,0,1,0,1,0,0,0]
    },
    "vmm2":{
            "sdp_dac": 35,
            "channel_sm": [0,0,1,0,0,0,0,0]
    },
    "vmm3":{
            "channel_st": [1,1,1,1,1,1,1,0],
            "channel_sm": [0,0,0,0,0,0,0,1]
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream(R"({
    "BOARD1_NAME": {
        "OpcNodeId":"FEB1_NAME"
    },
    "BOARD2_NAME": {
        "OpcNodeId":"FEB2_NAME"
    },
    "BOARD3_NAME": {
        "OpcNodeId":"FEB3_NAME"
    },
    "BOARD_NAME": {
        "OpcNodeId":"FEB_NAME",
        "vmm0":{
            "st": 275,
            "channel_st": [1,1,0,1,1,1,1,1],
            "channel_sm": [0,0,1,0,0,0,0,0]
        },
        "vmm1":{
            "st": 278,
            "sdp_dac": 15,
            "channel_st": [1,1,0,1,0,1,1,1],
            "channel_sm": [0,0,1,0,1,0,0,0]
        },
        "vmm2":{
            "st": 300,
            "channel_st": 1,
            "channel_sm": [0,0,1,0,0,0,0,0],
            "sdp_dac": 35
        },
        "vmm3":{
            "st": 250,
            "channel_st": [1,1,1,1,1,1,1,0],
            "channel_sm": [0,0,0,0,0,0,0,1]
        }
    }
})");
  pt::read_json(expected, expected_ptree);

  nsw::THRCalib::updatePtreeWithFeb(input_ptree, update_ptree);
  ptree_sort_vmms(input_ptree, "BOARD_NAME", 4);
  ptree_sort_vmms(expected_ptree, "BOARD_NAME", 4);

  // Test that updated ptree now contains exactly one of the VMMs we expect
  BOOST_TEST(test_feb.count("vmm0") == 1);
  BOOST_TEST(test_feb.count("vmm1") == 1);
  BOOST_TEST(test_feb.count("vmm2") == 1);
  BOOST_TEST(test_feb.count("vmm3") == 1);

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0") == expected_ptree.get_child("BOARD_NAME.vmm0"));
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm1") == expected_ptree.get_child("BOARD_NAME.vmm1"));
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm2") == expected_ptree.get_child("BOARD_NAME.vmm2"));
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm3") == expected_ptree.get_child("BOARD_NAME.vmm3"));

  // Test that the updated ptree and expected ptree are the same
  BOOST_TEST(input_ptree == expected_ptree);
}

BOOST_AUTO_TEST_CASE(UpdateTrimmerConfig_AddVmmSpecific)
{
  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME"
    }
})");
  pt::read_json(input, input_ptree);


  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId": "FEB_NAME",
    "vmm0": {
        "channel_st": [1,1,1,1,1,1,1,1],
        "channel_sm": [0,1,0,1,0,1,0,0]
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId": "FEB_NAME",
        "vmm0": {
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,1,0,1,0,1,0,0]
        }
    }
})");
  pt::read_json(expected, expected_ptree);

  nsw::THRCalib::updatePtreeWithFeb(input_ptree, update_ptree);
  ptree_sort_vmms(input_ptree, "BOARD_NAME", 4);
  ptree_sort_vmms(expected_ptree, "BOARD_NAME", 4);

  // Test that updated ptree now contains the vmm we added
  auto& test_feb = input_ptree.get_child("BOARD_NAME");
  BOOST_TEST(test_feb.count("vmm0") == 1);

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_st") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_st"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0.channel_sm") == expected_ptree.get_child("BOARD_NAME.vmm0.channel_sm"));

  // Test that the content in the updated ptree matches the expectation
  BOOST_TEST(input_ptree.get_child("BOARD_NAME.vmm0") == expected_ptree.get_child("BOARD_NAME.vmm0"));

  BOOST_TEST(input_ptree == expected_ptree);
}

BOOST_AUTO_TEST_CASE(MergeConfig_ModifyVmm)
{
  pt::ptree input_ptree{};
  auto input = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId":"FEB_NAME",
        "vmm0":{
            "st": 235,
            "channel_st": 1,
            "channel_sm": 0
        },
        "vmm2":{
            "st": 300,
            "channel_st": 1,
            "channel_sm": 0
        },
        "vmm3":{
            "st": 250,
            "channel_st": [1,1,1,1,1,1,1,1],
            "channel_sm": [0,0,0,0,0,0,0,0]
        }
    }
})");
  pt::read_json(input, input_ptree);

  pt::ptree update_ptree{};
  auto update = std::istringstream(R"({
    "OpcNodeId":"FEB_NAME",
    "vmm0":{
            "st": 275,
            "channel_st": [1,1,0,1,1,1,1,1],
            "channel_sm": [0,0,1,0,0,0,0,0]
    },
    "vmm1":{
            "channel_st": [1,1,0,1,0,1,1,1],
            "channel_sm": [0,0,1,0,1,0,0,0]
    },
    "vmm2":{
            "sdp_dac": 35,
            "channel_sm": [0,0,1,0,0,0,0,0]
    },
    "vmm3":{
            "channel_st": [1,1,1,1,1,1,1,0],
            "channel_sm": [0,0,0,0,0,0,0,1]
    }
})");
  pt::read_json(update, update_ptree);

  pt::ptree expected_ptree{};
  auto expected = std::istringstream(R"({
    "BOARD_NAME": {
        "OpcNodeId":"FEB_NAME",
        "vmm0":{
            "st": 275,
            "channel_st": [1,1,0,1,1,1,1,1],
            "channel_sm": [0,0,1,0,0,0,0,0]
        },
        "vmm1":{
            "channel_st": [1,1,0,1,0,1,1,1],
            "channel_sm": [0,0,1,0,1,0,0,0]
        },
        "vmm2":{
            "st": 300,
            "sdp_dac": 35,
            "channel_st": 1,
            "channel_sm": [0,0,1,0,0,0,0,0]
        },
        "vmm3":{
            "st": 250,
            "channel_st": [1,1,1,1,1,1,1,0],
            "channel_sm": [0,0,0,0,0,0,0,1]
        }
    }
})");
  pt::read_json(expected, expected_ptree);

}

bool operator==(const nsw::THRCalib::RunParameters& lhs, const nsw::THRCalib::RunParameters& rhs)
{
  return (lhs.samples == rhs.samples) && (lhs.factor == rhs.factor) && (lhs.type == rhs.type) &&
         (lhs.debug == rhs.debug);
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_THRParamsNoDebug_Correct)
{
  BOOST_TEST((nsw::THRCalib::parseCalibParams("THR,10,9,0") ==
              nsw::THRCalib::RunParameters{10, 9, "thresholds", false}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_THRParamsDebug_Correct)
{
  BOOST_TEST((nsw::THRCalib::parseCalibParams("THR,10,9,1") ==
              nsw::THRCalib::RunParameters{10, 9, "thresholds", true}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_BLNParamsNoDebug_Correct)
{
  BOOST_TEST((nsw::THRCalib::parseCalibParams("BLN,10,9,0") ==
              nsw::THRCalib::RunParameters{10, 9, "baselines", false}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_BLNParamsDebug_Correct)
{
  BOOST_TEST((nsw::THRCalib::parseCalibParams("BLN,10,9,1") ==
              nsw::THRCalib::RunParameters{10, 9, "baselines", true}));
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_WrongNumberOfParametersTooFew_Exception)
{
  BOOST_CHECK_EXCEPTION(nsw::THRCalib::parseCalibParams("BLN,10,9"),
                        nsw::THRParameterIssue,
                        [](const nsw::THRParameterIssue& ex) -> bool {
                          std::string error{ex.what()};
                          return error.find("Expected to parse") != std::string::npos;
                        });
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_WrongNumberOfParametersTooMany_Exception)
{
  BOOST_CHECK_EXCEPTION(nsw::THRCalib::parseCalibParams("BLN,10,9,0,123"),
                        nsw::THRParameterIssue,
                        [](const nsw::THRParameterIssue& ex) -> bool {
                          std::string error{ex.what()};
                          return error.find("Expected to parse") != std::string::npos;
                        });
}

BOOST_AUTO_TEST_CASE(ParseCalibParams_InvalidType_Exception)
{
  BOOST_CHECK_EXCEPTION(nsw::THRCalib::parseCalibParams("INV,10,9,0"),
                        nsw::THRParameterIssue,
                        [](const nsw::THRParameterIssue& ex) -> bool {
                          std::string error{ex.what()};
                          return error.find("Invalid calibration type specified") !=
                                 std::string::npos;
                        });
}
