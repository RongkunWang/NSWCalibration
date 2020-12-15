#include "NSWCalibration/MMStaircase.h"
using boost::property_tree::ptree;

// Class constructor 
nsw::MMStaircase::MMStaircase(std::string calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

// Setup
void nsw::MMStaircase::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 0;  
  ERS_INFO("m_dry_run " << m_dry_run);

  // parse calib type
  if (m_calibType=="MMStaircase") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for MMStaircase: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }
}

// Configure 
void nsw::MMStaircase::configure() {
  ERS_INFO("MMStaircase::configure " << counter());

  std::vector<std::string> ordered_addcs = {
    "ADDC_L1P6_IPR",
    "ADDC_L1P3_IPL",
    "ADDC_L1P3_IPR",
    "ADDC_L1P6_IPL",
    "ADDC_L4P6_IPR",
    "ADDC_L4P3_IPL",
    "ADDC_L4P3_IPR",
    "ADDC_L4P6_IPL",
    "ADDC_L4P6_HOR",
    "ADDC_L4P3_HOL",
    "ADDC_L4P3_HOR",
    "ADDC_L4P6_HOL",
    "ADDC_L1P6_HOR",
    "ADDC_L1P3_HOL",
    "ADDC_L1P3_HOR",
    "ADDC_L1P6_HOL",
  };

  for (auto & addc: ordered_addcs) {
    ERS_LOG("Writing ADDC config: " << addc.getAddress());
    auto cs = std::make_unique<nsw::ConfigSender>();

    // set art input phase to 0xf, top_patt.put("art_input_phase", 0xf);
    // set tp latency to -1, top_patt.put("tp_latency", -1);
    ERS_INFO("Configure " << addc << " with ART phase = " << 0xf << " and TP L1A latency = " << -1);
    if (!simulation()){
      cs->sendAddcConfig(addc);
      cs->sendTpConfig(tp);
      sleep(5);      
    }
 }
}

// Unconfigure button
void nsw::MMStaircase::unconfigure() {
  ERS_INFO("MMStaircase::unconfigure " << counter());
  ERS_INFO("Nothing to unconfigure.")
}
