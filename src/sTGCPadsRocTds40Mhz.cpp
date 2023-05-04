#include "NSWCalibration/sTGCPadsRocTds40Mhz.h"
#include "NSWCalibration/Utility.h"

#include <fmt/core.h>

#include <ers/ers.h>

nsw::sTGCPadsRocTds40Mhz::sTGCPadsRocTds40Mhz(std::string calibType,
                                    const hw::DeviceManager& deviceManager) :
  CalibAlg(std::move(calibType), deviceManager)
{
  checkObjects();
  setTotal(nsw::roc::NUM_PHASES_EPLL_TDS_40MHZ / m_phaseStep * nsw::padtrigger::NUM_INPUT_DELAYS);
}

void nsw::sTGCPadsRocTds40Mhz::configure() {
  if (counter() == 0) {
    openTree();
  }
  m_phase    = (counter() / nsw::padtrigger::NUM_INPUT_DELAYS) * m_phaseStep;
  m_pt_delay = (counter() % nsw::padtrigger::NUM_INPUT_DELAYS);
  setROCPhases();
  setPadTriggerDelays();
  nsw::snooze(std::chrono::milliseconds{100});
}

void nsw::sTGCPadsRocTds40Mhz::acquire() {
  m_error = getPadTriggerBcidErrors();
  m_bcid = getPadTriggerBcids();
  fillTree();
}

void nsw::sTGCPadsRocTds40Mhz::unconfigure() {
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadsRocTds40Mhz::setPadTriggerDelays() const {
  ERS_INFO(fmt::format("Setting pad trigger input delays to {}", m_pt_delay));
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    dev.writePFEBCommonDelay(m_pt_delay);
  }
}

void nsw::sTGCPadsRocTds40Mhz::setROCPhases() const {
  ERS_INFO(fmt::format("Config PFEB ROCs {} with {}", m_reg, m_phase));
  auto threads = std::vector< std::future<void> >();
  for (const auto& feb: getDeviceManager().getFebs()) {
    threads.push_back(
      std::async(std::launch::async, &nsw::sTGCPadsRocTds40Mhz::setROCPhase, this, feb)
    );
  }
  for (auto& thr: threads) {
    thr.get();
  }
}

void nsw::sTGCPadsRocTds40Mhz::setROCPhase(const nsw::hw::FEB& feb) const {
  if (feb.getGeoInfo().resourceType() != "PFEB") {
    return;
  }
  ERS_INFO(fmt::format("Config {}", feb.getScaAddress()));
  if (feb.getRoc().ping() == nsw::hw::ScaStatus::REACHABLE) {
    feb.getRoc().writeValue(std::string{m_reg}, m_phase);
  } else {
    ERS_INFO(fmt::format("Skipping unreachable {}", feb.getScaAddress()));
  }
}

std::vector<std::uint32_t> nsw::sTGCPadsRocTds40Mhz::getPadTriggerBcids() const {
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    return dev.readMedianPFEBBCIDs(m_numReads);
  }
  return std::vector<std::uint32_t>();
}

std::vector<std::uint32_t> nsw::sTGCPadsRocTds40Mhz::getPadTriggerBcidErrors() const {
  std::vector<std::uint32_t> errors(nsw::padtrigger::NUM_PFEBS);
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    // need to look at data to know if reading status is necessary (big slowdown if yes)
    // dev.togglePFEBBcidResetReadout();
    // nsw::snooze();
    // const auto word = dev.readSubRegister("00F_pfeb_status_READONLY", "status");
    const uint32_t word{0};
    for (std::size_t it = 0; it < nsw::padtrigger::NUM_PFEBS; it++) {
      errors.at(it) = (word >> it) & std::size_t{0b1};
    }
    return errors;
  }
  return std::vector<std::uint32_t>();
}

void nsw::sTGCPadsRocTds40Mhz::fillTree() {
  m_now = nsw::calib::utils::strf_time();
  m_rtree->Fill();
}

void nsw::sTGCPadsRocTds40Mhz::checkObjects() const {
  const auto npts  = getDeviceManager().getPadTriggers().size();
  const auto nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npts));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npts > 1) {
    const auto msg = fmt::format("sTGCPadTriggerInputDelays only works with 1 PT");
    ers::error(nsw::NSWsTGCPadsRocTds40MhzIssue(ERS_HERE, msg));
  }
}

void nsw::sTGCPadsRocTds40Mhz::openTree() {
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
  m_error = std::vector<std::uint32_t>();
  m_rtree->Branch("runnumber",   &m_runnumber);
  m_rtree->Branch("appname",     &m_app_name);
  m_rtree->Branch("name",        &m_pt_name);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("nreads",      &m_reads);
  m_rtree->Branch("phase_step",  &m_step);
  m_rtree->Branch("phase",       &m_phase);
  m_rtree->Branch("pad_delay",   &m_pt_delay);
  m_rtree->Branch("pfeb_bcid",   &m_bcid);
  m_rtree->Branch("pfeb_error",  &m_error);
  m_rtree->Branch("pfeb_index",  &m_pfeb);
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    m_pt_name = dev.getName();
    break;
  }
  for (std::size_t it = 0; it < nsw::padtrigger::NUM_PFEBS; it++) {
    m_pfeb.push_back(m_pfeb.size());
  }
}

void nsw::sTGCPadsRocTds40Mhz::closeTree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

