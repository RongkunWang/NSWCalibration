#include "NSWCalibration/sTGCPadsL1DDCFibers.h"
#include <stdexcept>
#include <fmt/core.h>
#include <ers/ers.h>
#include "NSWCalibration/Utility.h"
#include "NSWConfiguration/I2cMasterConfig.h"

nsw::sTGCPadsL1DDCFibers::sTGCPadsL1DDCFibers(std::string calibType,
                                              const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager)
{
  checkObjects();

  // set number of iterations
  // N(phases per L-side) * N(phases per R-side)
  setTotal(nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / m_phaseStep *
           nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / m_phaseStep);
}

void nsw::sTGCPadsL1DDCFibers::configure() {
  if (counter() == 0) {
    setupTree();
  }
  setPhases();
  setROCPhases();
  for (auto i = m_numReads; i > 0; i--) {
    m_bcid = getPadTriggerBCIDs();
    fillTree();
  }
}

void nsw::sTGCPadsL1DDCFibers::unconfigure() {
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadsL1DDCFibers::setPhases() {
  m_phase_L = (counter() / (nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / m_phaseStep)) * m_phaseStep;
  m_phase_R = (counter() % (nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / m_phaseStep)) * m_phaseStep;
}

void nsw::sTGCPadsL1DDCFibers::setROCPhases() const {
  ERS_INFO(fmt::format("Config L-(R-)side PFEBs {} with {} ({})", m_reg, m_phase_L, m_phase_R));
  auto threads = std::vector< std::future<void> >();
  for (const auto& feb: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadsL1DDCFibers::setROCPhase, this, feb)
    );
  }
  for (auto& thr: threads) {
    thr.get();
  }
}

void nsw::sTGCPadsL1DDCFibers::setROCPhase(const nsw::hw::FEB& feb) const {
  if (feb.getGeoInfo().resourceType() != "PFEB") {
    return;
  }

  // bookkeeping
  const auto name    = feb.getScaAddress();
  const auto is_left = isLeft(name);
  const auto phase   = is_left ? m_phase_L : m_phase_R;
  const auto side    = is_left ? "L" : "R";
  ERS_INFO(fmt::format("Configuring {} ({}-side)", name, side));

  // set phase
  if (not simulation()) {
    if (feb.getRoc().readScaOnline()) {
      feb.getRoc().writeValue(m_reg, phase);
    } else {
      ERS_INFO(fmt::format("Skipping unreachable {}", feb.getScaAddress()));
    }
  }
}

std::vector<std::uint32_t> nsw::sTGCPadsL1DDCFibers::getPadTriggerBCIDs() const {
  if (not simulation()) {
    for (const auto& pt: getDeviceManager().getPadTriggers()) {
      return pt.readPFEBBCIDs();
    }
  }
  return std::vector<std::uint32_t>(nsw::padtrigger::NUM_PFEBS);
}

void nsw::sTGCPadsL1DDCFibers::fillTree() {
  m_now = nsw::calib::utils::strf_time();
  m_rtree->Fill();
}

void nsw::sTGCPadsL1DDCFibers::checkObjects() const {
  const auto npts  = getDeviceManager().getPadTriggers().size();
  const auto nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npts));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npts > 1) {
    const auto msg = fmt::format("sTGCPadTriggerInputDelays only works with 1 PT");
    ers::error(nsw::NSWsTGCPadsL1DDCFibersIssue(ERS_HERE, msg));
  }
}

bool nsw::sTGCPadsL1DDCFibers::isLeft(const std::string& name) const {
  return nsw::contains(name, "HOL") or
         nsw::contains(name, "IPL") or
         nsw::contains(name, "/L1") or
         nsw::contains(name, "/L3") or
         nsw::contains(name, "/L5") or
         nsw::contains(name, "/L7");
}

bool nsw::sTGCPadsL1DDCFibers::isLeftStripped(const std::string& name) const {
  for (const auto& feb: getDeviceManager().getFebs()) {
    if (feb.getGeoInfo().resourceType() != "PFEB") {
      continue;
    }
    if (nsw::contains(feb.getScaAddress(), name)) {
      return isLeft(feb.getScaAddress());
    }
  }
  return false;
}

void nsw::sTGCPadsL1DDCFibers::setupTree() {
  m_runnumber = runNumber();
  m_app_name  = applicationName();
  m_now = nsw::calib::utils::strf_time();
  m_reads = m_numReads;
  m_step  = m_phaseStep;
  m_rname = fmt::format("{}.{}.{}.{}.root",
            m_calibType, m_runnumber, m_app_name, m_now);
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_pfeb  = std::vector<std::uint32_t>();
  m_bcid  = std::vector<std::uint32_t>();
  m_mask  = std::vector<bool>();
  m_left  = std::vector<bool>();
  m_rtree->Branch("runnumber",   &m_runnumber);
  m_rtree->Branch("appname",     &m_app_name);
  m_rtree->Branch("name",        &m_pt_name);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("nreads",      &m_reads);
  m_rtree->Branch("phase_step",  &m_step);
  m_rtree->Branch("phase_L",     &m_phase_L);
  m_rtree->Branch("phase_R",     &m_phase_R);
  m_rtree->Branch("pfeb_bcid",   &m_bcid);
  m_rtree->Branch("pfeb_index",  &m_pfeb);
  m_rtree->Branch("pfeb_mask",   &m_mask);
  m_rtree->Branch("pfeb_left",   &m_left);
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    m_pt_name = pt.getName();
    break;
  }
  for (std::size_t it = 0; it < nsw::padtrigger::NUM_PFEBS; it++) {
    const auto nameCore = nsw::padtrigger::ORDERED_PFEBS.at(it);
    const auto nameStar = nsw::padtrigger::ORDERED_PFEBS_GEOID.at(it);
    m_pfeb.push_back(m_pfeb.size());
    m_mask.push_back(existsInDB(std::string(nameCore)) or existsInDB(std::string(nameStar)));
    m_left.push_back(isLeftStripped(std::string(nameCore)) or isLeftStripped(std::string(nameStar)));
  }
}

bool nsw::sTGCPadsL1DDCFibers::existsInDB(const std::string& name) const {
  for (const auto& feb: getDeviceManager().getFebs()) {
    if (feb.getGeoInfo().resourceType() != "PFEB") {
      continue;
    }
    if (nsw::contains(feb.getScaAddress(), name)) {
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

