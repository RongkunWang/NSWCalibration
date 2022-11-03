#include "NSWCalibration/MMTriggerCalib.h"

#include "NSWCalibration/Issues.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Utility.h"
#include "NSWConfiguration/Constants.h"

#include <unistd.h>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <chrono>

#include <boost/property_tree/json_parser.hpp>  // for write_json

#include "ers/ers.h"

#include <is/infodynany.h>
#include <is/infodictionary.h>

#include "TROOT.h"

using boost::property_tree::ptree;
using namespace std::chrono_literals;

nsw::MMTriggerCalib::MMTriggerCalib(std::string calibType, const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager)
{
  if (m_calibType=="MMARTConnectivityTest" ||
      m_calibType=="MMARTConnectivityTestAllChannels") {
    m_phases = {-1};
    m_connectivity = true;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = false;
    m_staircase    = false;
  } else if (m_calibType=="MMTrackPulserTest") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = true;
    m_noise        = false;
    m_latency      = false;
    m_staircase    = false;
  } else if (m_calibType=="MMCableNoise") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = false;
    m_noise        = true;
    m_latency      = false;
    m_staircase    = false;
  } else if (m_calibType=="MMARTPhase") {
    m_phases = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    m_connectivity = true;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = false;
    m_staircase    = false;
  } else if (m_calibType=="MML1ALatency") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = true;
    m_staircase    = false;
  } else if (m_calibType=="MMStaircase") {
    m_phases = {-1};
    m_connectivity = false;
    m_tracks       = false;
    m_noise        = false;
    m_latency      = false;
    m_staircase    = true;
  } else {
    throw std::runtime_error("Unknown calibration request. Can't set up MMTriggerCalib: " + m_calibType);
  }
  ROOT::EnableThreadSafety();
}

void nsw::MMTriggerCalib::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  m_dry_run   = false;
  m_reset_vmm = false;
  m_threads = std::make_unique< std::vector< std::future<int> > >();
  m_threads->clear();


  m_patterns = patterns();
  setTotal(m_patterns.size());

  m_febs   = nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "MMFE8");
  m_addcs  = nsw::ConfigReader::makeObjects<nsw::ADDCConfig>(db, "ADDC");
  m_tps    = nsw::ConfigReader::makeObjects<nsw::TPConfig>  (db, "TP");

  ERS_INFO("Found " << m_febs.size()     << " MMFE8s");
  ERS_INFO("Found " << m_addcs.size()    << " ADDCs");
  ERS_INFO("Found " << m_tps.size()      << " TPs");
  ERS_INFO("Found " << m_phases.size()   << " ART input phases");
  ERS_INFO("Found " << m_patterns.size() << " patterns");

}

void nsw::MMTriggerCalib::configure() {

  if (counter() == 0) {
    m_watchdog = std::async(std::launch::async, &nsw::MMTriggerCalib::addc_tp_watchdog, this);
    m_writePattern = std::async(std::launch::async, &nsw::MMTriggerCalib::recordPattern, this);
  }

  for (auto toppattkv : m_patterns) {

    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    auto tr = toppattkv.second;

    ERS_INFO("Configure " << toppattkv.first
             << " with ART phase = " << tr.get<int>("art_input_phase")
             << " and TP L1A latency = " << tr.get<int>("tp_latency")
             );

    // enable test pulse
    configure_febs_from_ptree(tr, true);

    // set addc phase
    configure_addcs_from_ptree(tr);

    // send TP config ("ECR")
    configure_tps(tr);

    // record some data?
    if (m_latency)
      sleep(5);
  }

}

void nsw::MMTriggerCalib::acquire() {
  // TODO: remove this after Alti oneshot user command is used (and test) 
  if(m_calibType != "MMStaircase") std::this_thread::sleep_for(2000ms);
}

void nsw::MMTriggerCalib::unconfigure() {

  for (auto toppattkv : m_patterns) {

    int ipatt = pattern_number(toppattkv.first);
    if (ipatt != counter())
      continue;
    auto tr = toppattkv.second;

    ERS_INFO("Un-configure " << toppattkv.first);

    // disable test pulse
    configure_febs_from_ptree(tr, false);

    // read ARTs counters
    read_arts_counters();
  }

}

nsw::commands::Commands nsw::MMTriggerCalib::getAltiSequences() const {
  if (!(m_latency || m_staircase)) {
    return {{}, // before configure
            {nsw::commands::actionStartPG}, // during (before acquire)
            {} // after (before unconfigure)
    };
  }
  return {};
}

int nsw::MMTriggerCalib::wait_until_done() {
  for (auto& thread : *m_threads)
    thread.get();
  m_threads->clear();
  return 0;
}

int nsw::MMTriggerCalib::pattern_number(const std::string& name) const {
  return std::stoi( std::regex_replace(name, std::regex("pattern_"), "") );
}

int nsw::MMTriggerCalib::configure_febs_from_ptree(const ptree& tr, bool unmask) {
  //
  // if unmask and first art phase: send configuration
  // if   mask and  last art phase: send configuration
  //
  auto phase = tr.get<int>("art_input_phase");
  if (unmask) {
    if (m_phases.size() > 0 && phase != m_phases.front()) {
      return 0;
    }
  } else {
    if (m_phases.size() > 0 && phase != m_phases.back()) {
      return 0;
    }
  }

  for (auto pattkv : tr) {
    auto febpatt_n = pattkv.first;
    auto febtr     = pattkv.second;
    if (febpatt_n.find("febpattern_") == std::string::npos)
      continue;
    announce(febpatt_n, febtr, unmask);
    for (auto febkv : febtr) {
      for (auto & feb : m_febs) {
        // if new matching "geo_name" doesn't exist, look at old matching, continue if no match.
        // if new matching "geo_name" exist, look at either old matching or new matching, continue if both not match
        std::string addr = feb.getAddress();
        if (febkv.second.count("geo_name") == 0) {
          if (febkv.first != addr) continue;
        } else {
          std::string geo_name = febkv.second.get<std::string>("geo_name");
          if (febkv.first != addr && 
              addr.compare(addr.size() - geo_name.size(), geo_name.size(), geo_name) != 0 
              )
            continue;
        }
        m_threads->push_back(std::async(std::launch::async,
                                        &nsw::MMTriggerCalib::configure_vmms, this,
                                        feb, febkv.second, unmask));
        break;
      }
    }
    wait_until_done();
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_addcs_from_ptree(const ptree& tr) {
  std::string name_old = tr.count("addc_old") ? tr.get<std::string>("addc_old") : "";
  std::string name_geo = tr.count("addc_geo") ? tr.get<std::string>("addc_geo") : "";
  if (name_old != "")
    ERS_INFO("Configuring(Old name):" << name_old);
  if (name_geo != "")
    ERS_INFO("Configuring(New Name):" << name_geo);
  auto phase = tr.get<int>("art_input_phase");
  if (phase != -1) {
    if (m_phases.size() > 0 && phase == m_phases.front())
      std::cout << "ART phase: " << std::endl;
    std::cout << std::hex << phase << std::dec << std::flush;
    if (m_phases.size() > 0 && phase == m_phases.back())
      std::cout << std::endl;
    for (auto & addc : m_addcs)
      if (name_old == "" || name_geo == "" || addc.getAddress() == name_old || 
          addc.getAddress().find(name_geo) != std::string::npos)
        m_threads->push_back(std::async(std::launch::async,
                                        &nsw::MMTriggerCalib::configure_art_input_phase, this,
                                        addc, phase));
    wait_until_done();
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_tps(const ptree& tr) {
  auto latency = tr.get<int>("tp_latency");
  for (auto & tp : m_tps) {
    if (latency != -1)
      // TODO: need to account for boundary as well!!
      tp.setL1ARequestOffset(latency);
    auto cs = std::make_unique<nsw::ConfigSender>();
    while (m_tpscax_busy)
      usleep(1e5);
    m_tpscax_busy = true;
    if (!m_dry_run) {
      ERS_INFO("MMTP overflow word: " << cs->readSCAXRegisterWord(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW));
      cs->sendTPConfig(tp);
    }
    m_tpscax_busy = false;
  }
  return 0;
}

int nsw::MMTriggerCalib::announce(const std::string& name, const ptree& tr, bool unmask) const {
  std::cout << " Configure MMFE8s (" << (unmask ? "unmask" : "mask") << ") with " << name << std::endl << std::flush;
  for (auto febkv : tr) {
    std::cout << "  " << febkv.first;
    for (auto vmmkv : febkv.second) {
      if (vmmkv.first == "geo_name") continue;
      std::cout << " " << vmmkv.first;
      for (auto chkv : vmmkv.second)
        std::cout << "/" << chkv.second.get<unsigned>("");
    }
    std::cout << std::endl << std::flush;
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_vmms(nsw::FEBConfig feb, const ptree& febpatt, bool unmask) const {
  //
  // Example febpatt ptree:
  // {
  //   "0": ["0", "1", "2"],
  //   "1": ["0"],
  //   "2": ["0"]
  // }
  //
  auto cs = std::make_unique<nsw::ConfigSender>();
  int vmmid, chan;
  std::set<int> vmmids = {};
  for (auto vmmkv : febpatt) {
    if (vmmkv.first == "geo_name") continue;
    for (auto chkv : vmmkv.second) {
      vmmid = std::stoi(vmmkv.first);
      chan  = chkv.second.get<int>("");
      feb.getVmm(vmmid).setChannelRegisterOneChannel("channel_st", unmask ? 1 : 0, chan);
      feb.getVmm(vmmid).setChannelRegisterOneChannel("channel_sm", unmask ? 0 : 1, chan);
      vmmids.emplace(vmmid);
    }
  }
  if (m_reset_vmm) {
    for (auto vmmid : vmmids) {
      auto & vmm = feb.getVmm(vmmid);
      auto orig = vmm.getGlobalRegister("reset");
      vmm.setGlobalRegister("reset", 3);
      if (!m_dry_run)
        cs->sendVmmConfigSingle(feb, vmmid);
      vmm.setGlobalRegister("reset", orig);
    }
  }
  if (vmmids.size() == 8) {
    if (!m_dry_run)
      cs->sendVmmConfig(feb);
  } else {
    for (auto vmmid : vmmids)
      if (!m_dry_run)
        cs->sendVmmConfigSingle(feb, vmmid);
  }
  return 0;
}

int nsw::MMTriggerCalib::configure_art_input_phase(nsw::ADDCConfig addc, uint phase) const {
  auto cs = std::make_unique<nsw::ConfigSender>();
  if (m_staircase) {
    ERS_LOG("Writing ADDC config: " << addc.getAddress());
    for (size_t it = nsw::NUM_ART_PER_ADDC; it > 0; it--) {
      size_t iart = it - 1;
      try {
        if (!m_dry_run)
          cs->sendAddcConfig(addc, iart);
      } catch (std::exception & ex) {
        if (addc.getART(iart).MustConfigure()) {
          throw;
        } else {
          ERS_INFO("Allowed to fail: " << ex.what());
        }
      }
    }
    return 0;
  }

  if (phase > nsw::art::NUM_PHASE_INPUT)
    throw std::runtime_error("Gave bad phase to configure_art_input_phase: " + std::to_string(phase));

  constexpr size_t art_size = 2;
  uint8_t art_data[] = {0x0, 0x0};
  auto opc_ip   = addc.getOpcServerIp();
  auto sca_addr = addc.getAddress();
  uint8_t this_phase = phase + (phase << 4);
  for (auto art : addc.getARTs()) {
    auto name = sca_addr + "." + art.getName() + "Ps" + "." + art.getName() + "Ps";
    ERS_LOG("Writing ART phase " << name << ": 0x" << std::hex << phase);
    for (auto reg : nsw::art::REG_INPUT_PHASES) {
      art_data[0] = static_cast<uint8_t>(reg);
      art_data[1] = this_phase;
      if (!m_dry_run)
        cs->sendI2cRaw(opc_ip, name, art_data, art_size);
    }
  }
  return 0;
}

ptree nsw::MMTriggerCalib::patterns() const {
  ptree patts;
  int ipatts   = 0;
  int ifebpatt = 0;

  if (m_noise) {
    //
    // cable noise loop: no patterns
    //
    constexpr int npatts = 100;
    for (int i = 0; i < npatts; i++) {
      ptree feb_patt;
      ptree top_patt;
      top_patt.put("tp_latency", -1);
      top_patt.put("art_input_phase", -1);
      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
      ifebpatt++;
    }
  } else if (m_staircase) {
    //
    // staircase loop: reconfigure ADDCs in the order expected by TP.
    //                 checks for fiber- and bundle-swapping.
    //
    for (auto & addc: nsw::mmtp::ORDERED_ADDCS) {
      ptree feb_patt;
      ptree top_patt;
      top_patt.put("addc_old", std::string(addc.first));
      top_patt.put("addc_geo", std::string(addc.second));
      top_patt.put("tp_latency", -1);
      top_patt.put("art_input_phase", 0xf);
      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
      ifebpatt++;
    }
  } else if (m_latency) {
    //
    // latency loop: incrementing latency
    //               no FEB or ADDC patterns
    //
    constexpr int npatts = 100;
    for (int i = 0; i < npatts; i++) {
      ptree feb_patt;
      ptree top_patt;
      top_patt.put("tp_latency", i);
      top_patt.put("art_input_phase", -1);
      top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
      patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
      ipatts++;
      ifebpatt++;
    }
  } else if (m_connectivity) {
    //
    // connectivity max-parallel loop
    //
    for (size_t pos = 0; pos < nsw::mmfe8::MMFE8_PER_LAYER/2; pos += 2) {
      // pos(radius): 0, 2, 4, 6
      // PCB:         1, 2, 3, 4
      // do two radius at a time(pos, pos+1)
      constexpr bool even = true;
      constexpr bool odd = !even;
      const int pcb   = pos / 2 + 1;
      const auto pcbstr       = std::to_string(pcb);
      const auto pcbstr_plus4 = std::to_string(pcb+4);
      for (int chan = 0; chan < nsw::vmm::NUM_CH_PER_VMM; chan++) {
        if (m_calibType == "MMARTConnectivityTest" && chan % 10 != 0)
          continue;
        if (m_calibType == "MMARTPhase"            && chan % 10 != 0)
          continue;
        ptree feb_patt;
        for (auto && [name, geoName] : std::map<std::string, std::string>{
             { "MMFE8_L1P" + pcbstr       + "_HO" + (even ? "R" : "L"), 
               fmt::format("L7/R{}", pos)},
             { "MMFE8_L2P" + pcbstr       + "_HO" + (even ? "L" : "R"), 
               fmt::format("L6/R{}", pos)},
             { "MMFE8_L3P" + pcbstr       + "_HO" + (even ? "R" : "L"), 
               fmt::format("L5/R{}", pos)},
             { "MMFE8_L4P" + pcbstr       + "_HO" + (even ? "L" : "R"), 
               fmt::format("L4/R{}", pos)},
             { "MMFE8_L4P" + pcbstr       + "_IP" + (even ? "R" : "L"), 
               fmt::format("L3/R{}", pos)},
             { "MMFE8_L3P" + pcbstr       + "_IP" + (even ? "L" : "R"), 
               fmt::format("L2/R{}", pos)},
             { "MMFE8_L2P" + pcbstr       + "_IP" + (even ? "R" : "L"), 
               fmt::format("L1/R{}", pos)},
             { "MMFE8_L1P" + pcbstr       + "_IP" + (even ? "L" : "R"), 
               fmt::format("L0/R{}", pos)},
             { "MMFE8_L1P" + pcbstr_plus4 + "_HO" + (even ? "R" : "L"), 
               fmt::format("L7/R{}", pos+8)},
             { "MMFE8_L2P" + pcbstr_plus4 + "_HO" + (even ? "L" : "R"), 
               fmt::format("L6/R{}", pos+8)},
             { "MMFE8_L3P" + pcbstr_plus4 + "_HO" + (even ? "R" : "L"), 
               fmt::format("L5/R{}", pos+8)},
             { "MMFE8_L4P" + pcbstr_plus4 + "_HO" + (even ? "L" : "R"), 
               fmt::format("L4/R{}", pos+8)},
             { "MMFE8_L4P" + pcbstr_plus4 + "_IP" + (even ? "R" : "L"), 
               fmt::format("L3/R{}", pos+8)},
             { "MMFE8_L3P" + pcbstr_plus4 + "_IP" + (even ? "L" : "R"), 
               fmt::format("L2/R{}", pos+8)},
             { "MMFE8_L2P" + pcbstr_plus4 + "_IP" + (even ? "R" : "L"), 
               fmt::format("L1/R{}", pos+8)},
             { "MMFE8_L1P" + pcbstr_plus4 + "_IP" + (even ? "L" : "R"), 
               fmt::format("L0/R{}", pos+8)},

             { "MMFE8_L1P" + pcbstr       + "_HO" + (odd ? "R" : "L"), 
               fmt::format("L7/R{}", pos+1)},
             { "MMFE8_L2P" + pcbstr       + "_HO" + (odd ? "L" : "R"), 
               fmt::format("L6/R{}", pos+1)},
             { "MMFE8_L3P" + pcbstr       + "_HO" + (odd ? "R" : "L"), 
               fmt::format("L5/R{}", pos+1)},
             { "MMFE8_L4P" + pcbstr       + "_HO" + (odd ? "L" : "R"), 
               fmt::format("L4/R{}", pos+1)},
             { "MMFE8_L4P" + pcbstr       + "_IP" + (odd ? "R" : "L"), 
               fmt::format("L3/R{}", pos+1)},
             { "MMFE8_L3P" + pcbstr       + "_IP" + (odd ? "L" : "R"), 
               fmt::format("L2/R{}", pos+1)},
             { "MMFE8_L2P" + pcbstr       + "_IP" + (odd ? "R" : "L"), 
               fmt::format("L1/R{}", pos+1)},
             { "MMFE8_L1P" + pcbstr       + "_IP" + (odd ? "L" : "R"), 
               fmt::format("L0/R{}", pos+1)},
             { "MMFE8_L1P" + pcbstr_plus4 + "_HO" + (odd ? "R" : "L"), 
               fmt::format("L7/R{}", pos+1+8)},
             { "MMFE8_L2P" + pcbstr_plus4 + "_HO" + (odd ? "L" : "R"), 
               fmt::format("L6/R{}", pos+1+8)},
             { "MMFE8_L3P" + pcbstr_plus4 + "_HO" + (odd ? "R" : "L"), 
               fmt::format("L5/R{}", pos+1+8)},
             { "MMFE8_L4P" + pcbstr_plus4 + "_HO" + (odd ? "L" : "R"), 
               fmt::format("L4/R{}", pos+1+8)},
             { "MMFE8_L4P" + pcbstr_plus4 + "_IP" + (odd ? "R" : "L"), 
               fmt::format("L3/R{}", pos+1+8)},
             { "MMFE8_L3P" + pcbstr_plus4 + "_IP" + (odd ? "L" : "R"), 
               fmt::format("L2/R{}", pos+1+8)},
             { "MMFE8_L2P" + pcbstr_plus4 + "_IP" + (odd ? "R" : "L"), 
               fmt::format("L1/R{}", pos+1+8)},
             { "MMFE8_L1P" + pcbstr_plus4 + "_IP" + (odd ? "L" : "R"), 
               fmt::format("L0/R{}", pos+1+8)},
              }) {
          ptree febtree;
          for (size_t vmmid = 0; vmmid < nsw::MAX_NUMBER_OF_VMM; vmmid++) {

            //if (vmm_of_interest >= 0 && vmmid != vmm_of_interest)
            //  continue;

            // this might seem stupid, and it is, but
            //   it allows to write a vector of channels per VMM
            ptree vmmtree;
            ptree chantree;
            chantree.put("", chan);
            vmmtree.push_back(std::make_pair("", chantree));
            febtree.add_child(std::to_string(vmmid), vmmtree);
            febtree.put("geo_name", geoName);
          }
          feb_patt.add_child(name, febtree);
        }

        for (auto art_phase : m_phases) {
          ptree top_patt;
          top_patt.put("tp_latency", -1);
          top_patt.put("art_input_phase", art_phase);
          top_patt.add_child("febpattern_" + std::to_string(ifebpatt), feb_patt);
          patts.add_child("pattern_" + std::to_string(ipatts), top_patt);
          ipatts++;
        }

        ifebpatt++;
      }
    }
  } else if (m_tracks) {
    read_json(m_trackPatternFile, patts);
  }
  return patts;
}

int nsw::MMTriggerCalib::addc_tp_watchdog() {
  //
  // Be forewarned: this function reads TP SCAX registers.
  // Dont race elsewhere.
  //


  if (m_addcs.size() == 0)
    return 0;
  if (m_tps.size() == 0)
    return 0;

  nsw::ConfigSender cs;

  // sleep time
  constexpr size_t slp = 1;

  // collect all TPs from the ARTs
  std::set< std::pair<std::string, std::string> > tps;
  for (auto & addc : m_addcs)
    for (auto art : addc.getARTs())
      tps.emplace(std::make_pair(art.getOpcServerIp_TP(), art.getOpcNodeId_TP()));
  auto regAddrVec = nsw::intToByteVector(nsw::mmtp::REG_FIBER_ALIGNMENT, nsw::NUM_BYTES_IN_WORD32, true);
  std::vector<uint8_t> data_bcids = {
    nsw::mmtp::DUMMY_VAL,
    nsw::mmtp::DUMMY_VAL,
    nsw::mmtp::DUMMY_VAL,
    nsw::mmtp::DUMMY_VAL,
  };
  std::vector<uint8_t> data_bcids_total = {};

  // output file and announce
  auto now = nsw::calib::utils::strf_time();
  std::string rname = "addc_alignment."
    + std::to_string(runNumber()) + "." + applicationName() + "." + now + ".root";
  ERS_INFO("ADDC-TP watchdog. Output: " << rname << ". Sleep: " << slp << "s");
  auto rfile        = std::make_unique< TFile >(rname.c_str(), "recreate");
  auto rtree        = std::make_shared< TTree >("nsw", "nsw");
  auto addc_address = std::make_unique< std::vector<std::string> >();
  auto art_name     = std::make_unique< std::vector<std::string> >();
  auto art_fiber    = std::make_unique< std::vector<int> >();
  auto art_aligned  = std::make_unique< std::vector<int> >();
  auto art_bcid     = std::make_unique< std::vector<int> >();
  rtree->Branch("time",         &now);
  rtree->Branch("addc_address", addc_address.get());
  rtree->Branch("art_name",     art_name.get());
  rtree->Branch("art_fiber",    art_fiber.get());
  rtree->Branch("art_aligned",  art_aligned.get());
  rtree->Branch("art_bcid",     art_bcid.get());

  // monitor
  try {
    while (counter() < total()) {
      now = nsw::calib::utils::strf_time();
      addc_address->clear();
      art_name    ->clear();
      art_fiber   ->clear();
      art_aligned ->clear();
      for (auto tp : tps) {
        while (m_tpscax_busy)
          usleep(1e5);
        m_tpscax_busy = true;
        auto outdata = m_dry_run ? std::vector<uint8_t>(nsw::NUM_BYTES_IN_WORD32) :
          cs.readI2cAtAddress(tp.first, tp.second, regAddrVec.data(), regAddrVec.size(), nsw::NUM_BYTES_IN_WORD32);
        data_bcids_total.clear();
        for (auto reg : nsw::mmtp::REG_FIBER_BCIDS) {
          auto bxdata = nsw::intToByteVector(reg, nsw::NUM_BYTES_IN_WORD32, nsw::scax::SCAX_LITTLE_ENDIAN);
          if (!m_dry_run)
            data_bcids = cs.readI2cAtAddress(tp.first, tp.second, bxdata.data(), bxdata.size(), nsw::NUM_BYTES_IN_WORD32);
          for (auto byte : data_bcids)
            data_bcids_total.push_back(byte);
        }
        m_tpscax_busy = false;
        for (auto & addc : m_addcs) {
          for (auto art : addc.getARTs()) {
            if (art.IsMyTP(tp.first, tp.second)) {
              auto aligned = art.IsAlignedWithTP(outdata);
              auto tpbcid  = art.BcidFromTp(data_bcids_total);
              addc_address->push_back(addc.getAddress());
              art_name    ->push_back(art.getName());
              art_fiber   ->push_back(art.TP_GBTxAlignmentBit());
              art_aligned ->push_back(static_cast<int>(aligned));
              art_bcid    ->push_back(static_cast<int>(tpbcid));
            }
          }
        }
      }
      rtree->Fill();
      sleep(slp);
    }
  } catch (std::exception & e) {
    ERS_INFO("ADDC-TP watchdog. Caught exception: " << e.what());
  }

  // close
  usleep(1e6);
  ERS_INFO("Closing " << rfile->GetName());
  rfile->cd();
  rtree->Write();
  rfile->Close();
  return 0;
}

void nsw::MMTriggerCalib::recordPattern() {
  // do this before the start of run
  const auto now = nsw::calib::utils::strf_time();
  write_json(fmt::format("Pattern_run{}_{}.json", runNumber(), now), m_patterns);
}

int nsw::MMTriggerCalib::read_arts_counters() {

  // initialize output
  try {
    if (counter() == 0) {
      // file and tree
      std::string rname_hit = "art_counters."
        + std::to_string(runNumber()) + "." + applicationName() + "." + nsw::calib::utils::strf_time() + ".root";
      m_art_rfile = std::make_unique< TFile >(rname_hit.c_str(), "recreate");
      m_art_rtree = std::make_shared< TTree >("nsw", "nsw");
      ERS_INFO("ART hit counter. Output: "  << rname_hit);

      // branches
      m_art_now = nsw::calib::utils::strf_time();
      m_art_event    = -1;
      m_addc_address = "";
      m_art_name     = "";
      m_art_index    = -1;
      m_art_hits     = std::make_unique< std::vector<uint32_t> >();
      m_art_rtree->Branch("time",         &m_art_now);
      m_art_rtree->Branch("event",        &m_art_event);
      m_art_rtree->Branch("addc_address", &m_addc_address);
      m_art_rtree->Branch("art_name",     &m_art_name);
      m_art_rtree->Branch("art_index",    &m_art_index);
      m_art_rtree->Branch("art_hits",     m_art_hits.get());
    }
  } catch (std::exception & e) {
    ERS_INFO("read_arts_counters exception: " << e.what());
    return -1;
  }

  // read counters of 32 ARTs
  try {

    // init
    auto threads = std::make_unique<
      std::vector< std::future< std::vector<uint32_t> > >
      >();
    m_art_event = counter();
    m_art_now   = nsw::calib::utils::strf_time();

    // launch reader threads
    // https://its.cern.ch/jira/browse/OPCUA-2188
    for (auto & addc : m_addcs)
      for (auto art: addc.getARTs()) {
        if(addc.getART(art.index()).SkipConfigure()) {
          continue;
        }
        threads->push_back(std::async(std::launch::async,
                                      &nsw::MMTriggerCalib::read_art_counters,
                                      this, addc, art.index()));
      }

    // get results
    // 1 TTree entry per ART
    size_t it = 0;
    for (auto & addc : m_addcs) {
      for (auto art: addc.getARTs()) {
        if(addc.getART(art.index()).SkipConfigure()) {
          continue;
        }
        auto result = threads->at(it).get();
        m_addc_address = addc.getAddress();
        m_art_name     = art.getName();
        m_art_index    = it;
        m_art_hits->clear();
        for (auto val : result)
          m_art_hits->push_back(val);
        m_art_rtree->Fill();
        it++;
      }
    }

    threads->clear();

  } catch (std::exception & e) {
    ERS_INFO("read_arts_counters exception: " << e.what());
    return -1;
  }

  // close
  if (counter() == total() - 1) {
    if (m_art_rfile == nullptr || m_art_rtree == nullptr) {
      ERS_INFO("Cannot close art_rfile or art_rtree!"
               << " Something wasnt initialized. Skipping.");
      ERS_INFO("The pointers of interest:"
               << " m_art_rfile = " << m_art_rfile.get()
               << " m_art_rtree = " << m_art_rtree.get());
    } else {
      ERS_INFO("Closing " << m_art_rfile->GetName());
      m_art_rfile->cd();
      m_art_rtree->Write();
      m_art_rfile->Close();
    }
  }

  return 0;
}

// TODO: this function should move to NSWConfiguration
std::vector<uint32_t> nsw::MMTriggerCalib::read_art_counters(const nsw::ADDCConfig& addc, int art) const {

  // setup
  auto cs = std::make_unique<nsw::ConfigSender>();
  uint8_t art_data[] = {0x0, 0x0};
  auto opc_ip    = addc.getOpcServerIp();
  auto sca_addr  = addc.getAddress() + "." + addc.getART(art).getNameCore();
  size_t reg_local  = 0;
  size_t index      = 0;
  uint32_t word32   = 0;
  std::vector<uint8_t>  readback = {};
  std::vector<uint32_t> results  = {};

  // query registers
  for (size_t reg = nsw::art::REG_COUNTERS_START; reg < nsw::art::REG_COUNTERS_END; reg++) {

    // read N registers per transaction
    reg_local = reg - nsw::art::REG_COUNTERS_START;
    if ((reg_local % nsw::art::REG_COUNTERS_SIMULT) > 0)
      continue;

    // register address
    art_data[0] = static_cast<uint8_t>(reg);

    // read the register
    if (!simulation()) {
      try {
        readback = cs->readI2cAtAddress(opc_ip, sca_addr, art_data,
                                        nsw::art::ADDRESS_SIZE,
                                        nsw::art::REG_COUNTERS_SIMULT);
      } catch (std::exception & ex) {
        if (addc.getART(art).MustConfigure()) {
          throw;
        } else {
          ERS_INFO("Allowed to fail: " << ex.what());
          readback.clear();
          for (size_t it = 0; it < nsw::art::REG_COUNTERS_SIMULT; it++)
            readback.push_back(0);
        }
      }
    } else {
      readback.clear();
      for (size_t it = 0; it < nsw::art::REG_COUNTERS_SIMULT; it++)
        readback.push_back(static_cast<uint8_t>(it));
    }

    // check the size
    if (readback.size() != static_cast<size_t>(nsw::art::REG_COUNTERS_SIMULT)) {
      std::stringstream msg;
      msg << "Problem reading ART reg: " << sca_addr;
      nsw::NSWMMTriggerCalibIssue issue(ERS_HERE, msg.str());
      ers::warning(issue);
      throw std::runtime_error(msg.str());
    }

    // convert N 1-byte registers into N/4 32-bit word
    for (size_t it = 0; it < nsw::art::REG_COUNTERS_SIMULT; it++) {
      index = it % nsw::art::REG_COUNTERS_SIZE;
      if (index == 0)
        word32 = 0;
      word32 += (readback.at(it) << index*nsw::NUM_BITS_IN_BYTE);
      if (index == nsw::art::REG_COUNTERS_SIZE - 1)
        results.push_back(word32);
    }

  }

  return results;
}

void nsw::MMTriggerCalib::setCalibParamsFromIS(const ISInfoDictionary& is_dictionary,
                                               const std::string& is_db_name)
{
  if(!m_tracks) return;
  const auto calib_param_is_name = fmt::format("{}.Calib.trackPatternFile", is_db_name);
  if (is_dictionary.contains(calib_param_is_name)) {
    ISInfoDynAny calib_params_from_is;
    is_dictionary.getValue(calib_param_is_name, calib_params_from_is);
    const auto calibParams = calib_params_from_is.getAttributeValue<std::string>(0);
    ERS_INFO(fmt::format("trackPatternFile from IS: {}", calibParams));
    m_trackPatternFile = std::move(calibParams);
  } else {
    const auto is_cmd = fmt::format("is_write -p ${{TDAQ_PARTITION}} -n {} -t String -v <Track pattern file> -i 0", calib_param_is_name);
    nsw::calib::IsParameterNotFound issue(ERS_HERE, "trackPatternFile", is_cmd);
    ers::error(issue);
    throw issue;
  }
}
