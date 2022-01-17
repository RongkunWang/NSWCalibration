#include "NSWCalibration/sTGCPadsControlPhase.h"
#include "NSWCalibration/Utility.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/I2cMasterConfig.h"
#include <fmt/core.h>
#include <ers/ers.h>
#include <unistd.h>
#include <stdexcept>
#include <future>

void nsw::sTGCPadsControlPhase::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  //
  // make objects and outputs
  //
  setup_objects(db);
  setup_tree();

  //
  // set number of iterations
  // N1 pfebs * N2 phases per ROC
  //
  setTotal(nsw::padtrigger::NUM_PFEBS * nsw::roc::NUM_PHASES_CTRL_PHASE);

  //
  // other settings
  //

  nsw::snooze();
}

void nsw::sTGCPadsControlPhase::configure() {
  if (counter() == 0) {
    maskPFEBs();
  }
  m_pfeb_addr = counter() / nsw::roc::NUM_PHASES_CTRL_PHASE;
  m_delay     = counter() % nsw::roc::NUM_PHASES_CTRL_PHASE;
  m_pfeb_name = getCurrentPFEBName();
  if (setROCPhase()) {
    if (m_delay == 0) {
      std::tie(m_mask_to_0, m_mask_to_1)
        = getPadTriggerMask();
      setPadTriggerMask();
    }
    m_triggers = getPadTriggerRate();
    fill();
  }
}

void nsw::sTGCPadsControlPhase::unconfigure() {
  if (counter() == total() - 1) {
    close_tree();
  }
}

void nsw::sTGCPadsControlPhase::maskPFEBs() const {
  ERS_INFO("Masking all PFEB channels");
  auto threads = std::vector< std::future<void> >();
  for (auto& feb : m_pfebs) {
    threads.push_back( std::async(std::launch::async,
                                  &nsw::sTGCPadsControlPhase::maskPFEB,
                                  this,
                                  feb) );
  }
  for (auto& thread : threads) {
    thread.get();
  }
}

void nsw::sTGCPadsControlPhase::maskPFEB(nsw::FEBConfig feb) const {
  ERS_INFO(fmt::format("Masking {}/{}", feb.getOpcServerIp(), feb.getAddress()));
  for (auto& vmm: feb.getVmms()) {
    vmm.setChannelRegisterAllChannels("channel_st", false);
    vmm.setChannelRegisterAllChannels("channel_sm", true);
  }
  const auto cs = std::make_unique<nsw::ConfigSender>();
  if (!simulation()) {
    cs->sendVmmConfig(feb);
  }
}

std::string nsw::sTGCPadsControlPhase::getCurrentPFEBName() const {
  return std::string(nsw::padtrigger::ORDERED_PFEBS.at(m_pfeb_addr));
}

bool nsw::sTGCPadsControlPhase::setROCPhase() const {
  for (const auto& feb: m_pfebs) {
    //
    // find the PFEB for this loop
    //
    if (feb.getAddress().find(m_pfeb_name) == std::string::npos) {
      continue;
    }
    auto cs = std::make_unique<nsw::ConfigSender>();
    const auto opc_ip = feb.getOpcServerIp();
    const auto sca_address = feb.getAddress();
    ERS_INFO(fmt::format("Configuring {} ({}) of {} with {} = {}",
                         sca_address, m_pfeb_addr, opc_ip, the_subreg, m_delay));
    //
    // set reg107ePllTdc/ctrl_phase_0
    //
    auto roc_analog = I2cMasterConfig(feb.getRocAnalog());
    roc_analog.setRegisterValue(the_reg, the_subreg, m_delay);
    if (!simulation()) {
      cs->sendI2cMasterSingle(opc_ip, sca_address, roc_analog, the_reg);
    }
    return true;
  }

  //
  // continue if FEB not in DB
  //
  if (m_delay == 0) {
    ERS_INFO(fmt::format("Skipping {} because it doesnt exist in DB", m_pfeb_name));
    nsw::snooze();
  }
  return false;
}

std::pair<FebMask, FebMask> nsw::sTGCPadsControlPhase::getPadTriggerMask() const {

  FebMask mask_to_0;
  FebMask mask_to_1;

  const auto ip     = isIP(m_pfeb_name);
  const auto radius = getQuadRadius(m_pfeb_name);

  // choose mask for each PFEB
  for (size_t pfeb_addr = 0; pfeb_addr < nsw::padtrigger::NUM_PFEBS; pfeb_addr++) {
    if (pfeb_addr == m_pfeb_addr) {
      //
      // the PFEB of interesting: mask nothing
      //
      mask_to_0.set(pfeb_addr, false);
      mask_to_1.set(pfeb_addr, false);
    } else {
      const auto this_name   = std::string(nsw::padtrigger::ORDERED_PFEBS.at(pfeb_addr));
      const auto this_ip     = isIP(this_name);
      const auto this_radius = getQuadRadius(this_name);
      if (this_radius != radius) {
        //
        // PFEB of other radii: mask to 0
        //
        mask_to_0.set(pfeb_addr, true);
        mask_to_1.set(pfeb_addr, false);
      } else if (this_ip != ip) {
        //
        // PFEB of same radii but other wedge: mask to 1
        //
        mask_to_0.set(pfeb_addr, false);
        mask_to_1.set(pfeb_addr, true);
      } else if (((m_pfeb_addr + 1) % nsw::padtrigger::NUM_PFEBS_PER_QUAD) ==
                 (pfeb_addr         % nsw::padtrigger::NUM_PFEBS_PER_QUAD)) {
        //
        // PFEB of same radii, same wedge, next layer: mask to 0
        //
        mask_to_0.set(pfeb_addr, true);
        mask_to_1.set(pfeb_addr, false);
      } else {
        //
        // PFEB of same radii, same wedge, other 2 layers: mask to 1
        //
        mask_to_0.set(pfeb_addr, false);
        mask_to_1.set(pfeb_addr, true);
      }
    }
  }
  ERS_INFO(fmt::format("mask_to_0: {} ({:#010x})", mask_to_0.to_string(), mask_to_0.to_ulong()));
  ERS_INFO(fmt::format("mask_to_1: {} ({:#010x})", mask_to_1.to_string(), mask_to_1.to_ulong()));
  return std::make_pair(mask_to_0, mask_to_1);
}

void nsw::sTGCPadsControlPhase::setPadTriggerMask() const {
  for (const auto& pt: m_pts) {
    ERS_INFO(fmt::format("Configuring {}", pt.getName()));
    if (!simulation()) {
      pt.writeFPGARegister(nsw::padtrigger::REG_MASK_TO_0, m_mask_to_0.to_ulong());
      pt.writeFPGARegister(nsw::padtrigger::REG_MASK_TO_1, m_mask_to_1.to_ulong());
    }
  }
}

std::uint32_t nsw::sTGCPadsControlPhase::getPadTriggerRate() const {
  std::uint32_t word{0}, triggers_here{0}, triggers_total{0};
  ERS_INFO(fmt::format("Collecting triggers for {} seconds...", nsw::padtrigger::NUM_TRIGGER_RATE_READS));
  for (const auto& pt: m_pts) {
    for (size_t it = 0; it < nsw::padtrigger::NUM_TRIGGER_RATE_READS; it++) {
      if (!simulation()) {
        nsw::snooze();
        word = pt.readFPGARegister(nsw::padtrigger::REG_STATUS);
      } else {
        nsw::snooze(std::chrono::milliseconds(100));
      }
      triggers_here = word >> nsw::padtrigger::TRIGGER_RATE_BITSHIFT;
      triggers_total += triggers_here;
    }
    break;
  }
  ERS_INFO(fmt::format("Collected {} triggers", triggers_total));
  return triggers_total;
}

void nsw::sTGCPadsControlPhase::fill() {
  for (const auto& pt: m_pts) {
    m_pt_name        = pt.getName();
    m_pt_opcserverip = pt.getName();
    m_pt_address     = pt.getName();
    break;
  }
  m_now = nsw::calib::utils::strf_time();
  m_rtree->Fill();
}

void nsw::sTGCPadsControlPhase::setup_objects(const std::string& db) {
  //
  // pad trigger objects
  //
  ERS_INFO("Finding pad triggers...");
  m_pts = nsw::ConfigReader::makeObjects<nsw::hw::PadTrigger>(db, "PadTrigger");
  ERS_INFO(fmt::format("Found {} pad triggers", m_pts.size()));

  //
  // pfeb objects
  //
  ERS_INFO("Finding PFEBs...");
  m_pfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig>
    (db, "PFEB");
  ERS_INFO(fmt::format("Found {} PFEBs", m_pfebs.size()));

  //
  // the calibration is per-sector
  //
  if (m_pts.size() != std::size_t{1}) {
    const auto msg = std::string("Only works with 1 pad trigger. Crashing.");
    nsw::NSWsTGCPadsControlPhaseIssue issue(ERS_HERE, msg.c_str());
    ers::fatal(issue);
    throw issue;
  }
}

void nsw::sTGCPadsControlPhase::setup_tree() {
  m_runnumber = runNumber();
  m_app_name  = applicationName();
  m_now = nsw::calib::utils::strf_time();
  m_rname = fmt::format("{}.{}.{}.{}.root", m_calibType, m_runnumber, m_app_name, m_now);
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_rtree->Branch("runnumber",   &m_runnumber);
  m_rtree->Branch("appname",     &m_app_name);
  m_rtree->Branch("name",        &m_pt_name);
  m_rtree->Branch("opcserverip", &m_pt_opcserverip);
  m_rtree->Branch("address",     &m_pt_address);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("delay",       &m_delay);
  m_rtree->Branch("pfeb_name",   &m_pfeb_name);
  m_rtree->Branch("pfeb_addr",   &m_pfeb_addr);
  m_rtree->Branch("triggers",    &m_triggers);
}

void nsw::sTGCPadsControlPhase::close_tree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

bool nsw::sTGCPadsControlPhase::isIP(const std::string& pfeb_name) const {
  return pfeb_name.find("IP") != std::string::npos;
}

std::uint32_t nsw::sTGCPadsControlPhase::getQuadRadius(const std::string& pfeb_name) const {
  for (uint32_t radius = 1; radius < nsw::NUM_RADII_STGC+1; radius++) {
    const auto qx = fmt::format("Q{}", radius);
    if (pfeb_name.find(qx) != std::string::npos) {
      return radius;
    }
  }
  const auto msg = fmt::format("Cant get quad radius from {}", pfeb_name);
  nsw::NSWsTGCPadsControlPhaseIssue issue(ERS_HERE, msg.c_str());
  ers::fatal(issue);
  throw issue;
}

