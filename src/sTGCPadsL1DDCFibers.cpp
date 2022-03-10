#include "NSWCalibration/sTGCPadsL1DDCFibers.h"

#include <unistd.h>
#include <stdexcept>
#include <future>

#include <fmt/core.h>

#include <ers/ers.h>

#include "NSWCalibration/Utility.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/I2cMasterConfig.h"
#include "NSWConfiguration/hw/PadTrigger.h"

nsw::sTGCPadsL1DDCFibers::sTGCPadsL1DDCFibers(std::string calibType,
                                              const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager),
  m_pt{[&deviceManager]() -> const hw::PadTrigger& {
    ERS_INFO("Finding pad triggers...");
    const auto& pts = deviceManager.getPadTriggers();
    ERS_INFO(fmt::format("Found {} pad triggers", pts.size()));
    if (pts.size() != std::size_t{1}) {
      const auto msg = std::string("Only works with 1 pad trigger. Crashing.");
      nsw::NSWsTGCPadsL1DDCFibersIssue issue(ERS_HERE, msg);
      ers::fatal(issue);
      throw issue;
    }
    return pts.at(0);
  }()} {};

void nsw::sTGCPadsL1DDCFibers::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  //
  // make objects and outputs
  //
  setupObjects(db);
  setupTree();

  //
  // set number of iterations
  // N(phases per L-side) * N(phases per R-side)
  //
  setTotal(nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / phase_step *
           nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / phase_step);

  //
  // other settings
  //

  nsw::snooze();
}

void nsw::sTGCPadsL1DDCFibers::configure() {
  m_phase_L = (counter() / (nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / phase_step)) * phase_step;
  m_phase_R = (counter() % (nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / phase_step)) * phase_step;
  setROCPhase();
  nsw::snooze(std::chrono::milliseconds(100));
  for (auto i = num_reads; i > 0; i--) {
    m_bcid->clear();
    for (const auto bcid: getPadTriggerBCIDs()) {
      m_bcid->push_back(bcid);
    }
    fill();
  }
}

void nsw::sTGCPadsL1DDCFibers::unconfigure() {
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadsL1DDCFibers::setROCPhase() const {

  auto cs = std::make_unique<nsw::ConfigSender>();
  ERS_INFO(fmt::format("Configuring L-(R-)side PFEBs {}/{} with {} ({})",
                       the_reg, the_subreg, m_phase_L, m_phase_R));

  for (const auto& feb: m_pfebs) {

    // feb address
    const auto opc_ip      = feb.getOpcServerIp();
    const auto sca_address = feb.getAddress();
    const auto is_left     = isLeft(feb.getAddress());
    const auto phase       = is_left ? m_phase_L : m_phase_R;
    const auto side        = is_left ? "L" : "R";
    ERS_INFO(fmt::format("Configuring {} ({}-side)",
                         sca_address, side));

    // set phase
    auto roc_analog = I2cMasterConfig(feb.getRocAnalog());
    roc_analog.setRegisterValue(the_reg, the_subreg, phase);
    if (!simulation()) {
      cs->sendI2cMasterSingle(opc_ip, sca_address, roc_analog, the_reg);
    }
  }
}

std::vector<std::uint32_t> nsw::sTGCPadsL1DDCFibers::getPadTriggerBCIDs() const {
  if (simulation()) {
    return std::vector<std::uint32_t>(nsw::padtrigger::NUM_PFEBS);
  }
  return m_pt.get().readPFEBBCIDs();
}

void nsw::sTGCPadsL1DDCFibers::fill() {
  m_pt_name = m_pt.get().getName();
  m_now     = nsw::calib::utils::strf_time();
  m_rtree->Fill();
}

void nsw::sTGCPadsL1DDCFibers::setupObjects(const std::string& db) {
  //
  // pad trigger objects
  //


  //
  // pfeb objects
  //
  ERS_INFO("Finding PFEBs...");
  m_pfebs = nsw::ConfigReader::makeObjects<nsw::FEBConfig>(db, "PFEB");
  ERS_INFO(fmt::format("Found {} PFEBs", m_pfebs.size()));
}

bool nsw::sTGCPadsL1DDCFibers::isLeft(const std::string& name) const {
  return (name.find("HOL") != std::string::npos) or
         (name.find("IPL") != std::string::npos);
}

bool nsw::sTGCPadsL1DDCFibers::isLeftStripped(const std::string& name) const {
  for (const auto& feb: m_pfebs) {
    if (feb.getAddress().find(name) != std::string::npos) {
      return isLeft(feb.getAddress());
    }
  }
  return false;
}

void nsw::sTGCPadsL1DDCFibers::setupTree() {
  m_runnumber = runNumber();
  m_app_name  = applicationName();
  m_now = nsw::calib::utils::strf_time();
  m_reads = num_reads;
  m_step  = phase_step;
  m_rname = fmt::format("{}.{}.{}.{}.root",
            m_calibType, m_runnumber, m_app_name, m_now);
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_pfeb  = std::make_unique< std::vector<std::uint32_t> >();
  m_bcid  = std::make_unique< std::vector<std::uint32_t> >();
  m_mask  = std::make_unique< std::vector<bool> >();
  m_left  = std::make_unique< std::vector<bool> >();
  m_rtree->Branch("runnumber",   &m_runnumber);
  m_rtree->Branch("appname",     &m_app_name);
  m_rtree->Branch("name",        &m_pt_name);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("nreads",      &m_reads);
  m_rtree->Branch("phase_step",  &m_step);
  m_rtree->Branch("phase_L",     &m_phase_L);
  m_rtree->Branch("phase_R",     &m_phase_R);
  m_rtree->Branch("pfeb_bcid",    m_bcid.get());
  m_rtree->Branch("pfeb_index",   m_pfeb.get());
  m_rtree->Branch("pfeb_mask",    m_mask.get());
  m_rtree->Branch("pfeb_left",    m_left.get());
  for (const auto name: nsw::padtrigger::ORDERED_PFEBS) {
    m_pfeb->push_back(m_pfeb->size());
    m_mask->push_back(existsInDB(std::string(name)));
    m_left->push_back(isLeftStripped(std::string(name)));
  }
}

bool nsw::sTGCPadsL1DDCFibers::existsInDB(const std::string& name) const {
  for (const auto& feb: m_pfebs) {
    if (feb.getAddress().find(name) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void nsw::sTGCPadsL1DDCFibers::closeTree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

