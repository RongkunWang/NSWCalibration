#include "NSWCalibration/sTGCPadsControlPhase.h"
#include "NSWCalibration/Utility.h"
#include <fmt/core.h>
#include <ers/ers.h>

nsw::sTGCPadsControlPhase::sTGCPadsControlPhase(std::string calibType,
                                                const hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager)
{
  checkObjects();
  setTotal(nsw::roc::NUM_PHASES_CTRL_PHASE);
}

void nsw::sTGCPadsControlPhase::configure() {
  if (counter() == 0) {
    setupTree();
    maskPFEBs();
  }
  m_delay = counter();
  setROCPhases();
  nsw::snooze(2 * nsw::padtrigger::PFEB_HIT_RATE_TIME);
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    ERS_INFO("Reading PFEB hit rates...");
    const auto rates = pt.readPFEBRates();
    for (std::size_t it = 0; it < std::size(rates); it++) {
      m_pfeb_addr = it;
      m_pfeb_rate = rates.at(it);
      m_pfeb_name = nsw::padtrigger::ORDERED_PFEBS.at(it);
      fillTree();
    }
  }
}

void nsw::sTGCPadsControlPhase::unconfigure() {
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadsControlPhase::maskPFEBs() const {
  ERS_INFO("Masking all PFEB channels");
  auto threads = std::vector< std::future<void> >();
  for (const auto& feb: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadsControlPhase::maskPFEB, this, feb)
    );
  }
  for (auto& thr : threads) {
    thr.get();
  }
}

void nsw::sTGCPadsControlPhase::maskPFEB(const nsw::hw::FEB& feb) const {
  const auto name = feb.getOpcNodeId();
  if (name.find("PFEB") == std::string::npos) {
    return;
  }
  ERS_INFO(fmt::format("Masking {}", name));
  for (const auto& vmmDev: feb.getVmms()) {
    auto vmmConf = nsw::VMMConfig{vmmDev.getConfig()};
    vmmConf.setChannelRegisterAllChannels("channel_st", false);
    vmmConf.setChannelRegisterAllChannels("channel_sm", true);
    if (not simulation()) {
      vmmDev.writeConfiguration(vmmConf);
    }
  }
}

void nsw::sTGCPadsControlPhase::setROCPhases() const {
  auto threads = std::vector< std::future<void> >();
  for (const auto& feb: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadsControlPhase::setROCPhase, this, feb)
    );
  }
  for (auto& thr : threads) {
    thr.get();
  }
}

void nsw::sTGCPadsControlPhase::setROCPhase(const nsw::hw::FEB& feb) const {
  const auto name = feb.getOpcNodeId();
  if (name.find("PFEB") == std::string::npos) {
    return;
  }
  ERS_INFO(fmt::format("Configure {}: {} = {}", name, m_reg, m_delay));
  if (not simulation()) {
    feb.getRoc().writeValue(m_reg, m_delay);
  }
}

void nsw::sTGCPadsControlPhase::checkObjects() const {
  const auto npts  = getDeviceManager().getPadTriggers().size();
  const auto nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npts));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npts > 1) {
    const auto msg = fmt::format("sTGCPadTriggerInputDelays only works with 1 PT");
    ers::error(nsw::NSWsTGCPadsControlPhaseIssue(ERS_HERE, msg));
  }
}

void nsw::sTGCPadsControlPhase::setupTree() {
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
  m_rtree->Branch("pfeb_rate",   &m_pfeb_rate);
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    m_pt_name        = pt.getName();
    m_pt_opcserverip = pt.getName();
    m_pt_address     = pt.getName();
    break;
  }
}

void nsw::sTGCPadsControlPhase::fillTree() {
  m_now = nsw::calib::utils::strf_time();
  m_rtree->Fill();
}

void nsw::sTGCPadsControlPhase::closeTree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

