#include "NSWCalibration/sTGCPadsHitRateSca.h"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <ers/ers.h>
#include "NSWCalibration/Utility.h"

nsw::sTGCPadsHitRateSca::sTGCPadsHitRateSca(std::string calibType,
                                            const hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager)
{
  m_acquire_time = getAcquireTime();
  ERS_INFO(fmt::format("Acquire time: {} seconds", m_acquire_time));
  setTotal(m_thresholdAdjustments.size() * nsw::tds::NUM_CH_PER_PAD_TDS);
}

void nsw::sTGCPadsHitRateSca::configure() {
  if (updateVmmThresholds()) {
    setVmmThresholds();
  }
  setCurrentTdsChannels();
}

void nsw::sTGCPadsHitRateSca::acquire() {
  if (counter() == 0) {
    setupTree();
  }
  for (std::size_t it = 0; it < m_acquire_time; it++) {
    nsw::snooze();
    for (const auto& dev: getDeviceManager().getPadTriggers()) {
      const auto rates = dev.readPFEBRates();
      for (std::size_t pfeb = 0; pfeb < rates.size(); pfeb++) {
        fillTree(pfeb, rates.at(pfeb));
      }
    }
  }
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadsHitRateSca::checkObjects() const {
  const std::size_t npads = getDeviceManager().getPadTriggers().size();
  const std::size_t nfebs = getDeviceManager().getFebs().size();

  // pad trigger objects
  ERS_INFO(fmt::format("Found {} pad triggers", npads));
  if (npads != std::size_t{1}) {
    const auto msg = std::string("Requires 1 pad trigger");
    ers::error(nsw::NSWsTGCPadsHitRateScaIssue(ERS_HERE, msg));
  }

  // pfeb objects
  ERS_INFO(fmt::format("Found {} PFEBs", nfebs));
}

void nsw::sTGCPadsHitRateSca::setupTree() {
  m_runnumber = runNumber();
  m_app_name = applicationName();
  m_now = nsw::calib::utils::strf_time();
  m_rname = fmt::format("{}.{}.{}.{}.root", m_calibType, m_runnumber, m_app_name, m_now);
  ERS_INFO(fmt::format("Opening TFile/TTree {}", m_rname));
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_rtree->Branch("runnumber",   &m_runnumber);
  m_rtree->Branch("appname",     &m_app_name);
  m_rtree->Branch("name",        &m_pt_name);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("rate",        &m_rate);
  m_rtree->Branch("threshold",   &m_threshold);
  m_rtree->Branch("pfeb_addr",   &m_pfeb);
  m_rtree->Branch("tds_chan",    &m_tds_chan);
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    m_pt_name = dev.getName();
  }
}

void nsw::sTGCPadsHitRateSca::fillTree(const std::uint32_t pfeb,
                                       const std::uint32_t rate) {
  m_threshold = getCurrentVmmThreshold();
  m_tds_chan = getCurrentTdsChannel();
  m_pfeb = pfeb;
  m_rate = rate;
  m_rtree->Fill();
}

void nsw::sTGCPadsHitRateSca::closeTree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

void nsw::sTGCPadsHitRateSca::setCurrentTdsChannels() const {
  const auto mask = getCurrentTdsChannelMask();
  ERS_INFO(fmt::format("Write {:016x}_{:016x} to tds register {}",
                       static_cast<std::uint64_t>(mask >> nsw::NUM_BITS_IN_WORD64),
                       static_cast<std::uint64_t>(mask),
                       m_regAddressChannelMask));
  auto threads = std::vector< std::future<void> >();
  for (const auto& dev: getDeviceManager().getFebs()) {
    threads.push_back(std::async(std::launch::async, &nsw::sTGCPadsHitRateSca::setCurrentTdsChannel, this, dev));
  }
  for (auto& thread: threads) {
    thread.get();
  }
}

void nsw::sTGCPadsHitRateSca::setCurrentTdsChannel(const nsw::hw::FEB& dev) const {
  if (nsw::getElementType(dev.getOpcNodeId()) != "PFEB") {
    return;
  }
  for (std::size_t it = 0; it < nsw::NUM_TDS_PER_PFEB; it++) {
    dev.getTds(it).writeRegister(m_regAddressChannelMask, getCurrentTdsChannelMask());
  }
}

__uint128_t nsw::sTGCPadsHitRateSca::getCurrentTdsChannelMask() const {
  const auto currentChan = getCurrentTdsChannel();
  const bool inMsbs = currentChan >= nsw::NUM_BITS_IN_WORD64;
  const std::uint64_t one{1};
  const std::uint64_t lsbsNot = inMsbs ? 0 : (one << currentChan);
  const std::uint64_t msbsNot = inMsbs ? (one << (currentChan - nsw::NUM_BITS_IN_WORD64)) : 0;
  const std::uint64_t lsbs = ~lsbsNot;
  const std::uint64_t msbs = ~msbsNot;
  return nsw::constructUint128t(msbs, lsbs);
}

std::size_t nsw::sTGCPadsHitRateSca::getCurrentVmmThreshold() const {
  return counter() / nsw::tds::NUM_CH_PER_PAD_TDS;
}

std::size_t nsw::sTGCPadsHitRateSca::getCurrentTdsChannel() const {
  return counter() % nsw::tds::NUM_CH_PER_PAD_TDS;
}

bool nsw::sTGCPadsHitRateSca::updateVmmThresholds() const {
  return counter() % nsw::tds::NUM_CH_PER_PAD_TDS == 0;
}

void nsw::sTGCPadsHitRateSca::setVmmThresholds() const {
  auto threads = std::vector< std::future<void> >();
  for (const auto& dev: getDeviceManager().getFebs()) {
    threads.push_back(std::async(std::launch::async, &nsw::sTGCPadsHitRateSca::setVmmThreshold, this, dev));
  }
  for (auto& thread: threads) {
    thread.get();
  }
}

void nsw::sTGCPadsHitRateSca::setVmmThreshold(const nsw::hw::FEB& dev) const {
  if (nsw::getElementType(dev.getOpcNodeId()) != "PFEB") {
    return;
  }
  const auto thrAdj = m_thresholdAdjustments.at(getCurrentVmmThreshold());
  for (std::size_t it = nsw::PFEB_FIRST_PAD_VMM; it < nsw::NUM_VMM_PER_PFEB; it++) {
    const auto& vmmDev = dev.getVmm(it);
    auto vmmConf = nsw::VMMConfig{vmmDev.getConfig()};
    const auto currentThreshold = vmmConf.getGlobalThreshold();
    ERS_INFO(fmt::format("Setting threshold {} in {}/{}",
                         currentThreshold + thrAdj, dev.getOpcNodeId(), vmmConf.getName()));
    checkThresholdAdjustment(currentThreshold, thrAdj);
    vmmConf.setGlobalThreshold(currentThreshold + thrAdj);
    vmmDev.writeConfiguration(vmmConf);
  }
}

void nsw::sTGCPadsHitRateSca::checkThresholdAdjustment(std::uint32_t thr, int adj) const {
  if (adj < 0 and static_cast<std::uint32_t>(std::abs(adj)) > thr) {
    const auto msg = fmt::format("Cannot adjust {} by {}", thr, adj);
    throw std::runtime_error(msg);
  }
}

std::size_t nsw::sTGCPadsHitRateSca::getAcquireTime() const {
  const auto tokens = nsw::tokenizeString(m_calibType, "_");
  if (std::size(tokens) == 1) {
    return m_acquire_time;
  }
  return std::stoul(tokens.at(1));
}
