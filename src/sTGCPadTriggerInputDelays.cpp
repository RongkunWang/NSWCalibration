#include "NSWCalibration/sTGCPadTriggerInputDelays.h"
#include <unistd.h>
#include <stdexcept>
#include "ers/ers.h"
#include "NSWCalibration/Utility.h"

nsw::sTGCPadTriggerInputDelays::sTGCPadTriggerInputDelays(std::string calibType,
                                                          const hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager)
{
  checkObjects();
  setTotal(nsw::padtrigger::NUM_INPUT_DELAYS);
}

void nsw::sTGCPadTriggerInputDelays::configure() {
  ERS_INFO("sTGCPadTriggerInputDelays::configure " << counter());
  if (counter() == 0) {
    setupTree();
  }
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    setDelays(pt);
    for (std::uint32_t i = 0; i < nsw::padtrigger::NUM_PFEB_BCID_READS; i++) {
      m_bcid = pt.readPFEBBCIDs();
      fillTree();
    }
  }
}

void nsw::sTGCPadTriggerInputDelays::unconfigure() {
  ERS_INFO("sTGCPadTriggerInputDelays::unconfigure " << counter());
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadTriggerInputDelays::setDelays(const nsw::hw::PadTrigger& pt) const {
  ERS_INFO(fmt::format("Configuring {} with delay {:#x}", pt.getName(), counter()));
  if (!simulation()) {
    pt.writePFEBCommonDelay(counter());
  }
}

void nsw::sTGCPadTriggerInputDelays::checkObjects() {
  const auto npts = getDeviceManager().getPadTriggers().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npts));
  if (npts > 1) {
    const auto msg = fmt::format("sTGCPadTriggerInputDelays only works with 1 PT");
    ers::error(nsw::NSWsTGCPadTriggerInputDelaysIssue(ERS_HERE, msg));
  }
}

void nsw::sTGCPadTriggerInputDelays::fillTree() {
  for (const auto& pt: getDeviceManager().getPadTriggers()) {
    m_opcserverip = pt.getName();
    m_address     = pt.getName();
    break;
  }
  m_now = nsw::calib::utils::strf_time();
  m_delay = counter();
  m_rtree->Fill();
}

void nsw::sTGCPadTriggerInputDelays::setupTree() {
  m_now = nsw::calib::utils::strf_time();
  m_rname = fmt::format("{}.{}.{}.{}.root", m_calibType, runNumber(), applicationName(), m_now);
  ERS_INFO(fmt::format("Opening ROOT file/tree: {}", m_rname));
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_rtree->Branch("opcserverip", &m_opcserverip);
  m_rtree->Branch("address",     &m_address);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("delay",       &m_delay);
  m_rtree->Branch("pfeb_bcid",   &m_bcid);
  m_rtree->Branch("pfeb_index",  &m_pfeb);
  m_pfeb.resize(nsw::padtrigger::NUM_PFEBS);
  std::iota(std::begin(m_pfeb), std::end(m_pfeb), 0);
}

void nsw::sTGCPadTriggerInputDelays::closeTree() {
  ERS_INFO("Closing ");
  m_rtree->Write();
  m_rfile->Close();
}
