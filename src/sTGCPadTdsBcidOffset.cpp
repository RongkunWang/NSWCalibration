#include "NSWCalibration/sTGCPadTdsBcidOffset.h"

#include <fmt/core.h>

#include <ers/ers.h>

#include "NSWCalibration/Utility.h"

nsw::sTGCPadTdsBcidOffset::sTGCPadTdsBcidOffset(std::string calibType,
                                            const hw::DeviceManager& deviceManager):
  CalibAlg(std::move(calibType), deviceManager)
{
  setTotal(m_numBcPerOrbit);
}

void nsw::sTGCPadTdsBcidOffset::setup(const std::string& /* db */) {
  checkObjects();
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    m_pad_trigger_bcid_offset = dev.readSubRegister("000_control_reg", "conf_bcid_offset");
  }
}

void nsw::sTGCPadTdsBcidOffset::configure() {
  if (counter() == 0) {
    openTree();
  }
  setTdsBcidOffsets();
}

void nsw::sTGCPadTdsBcidOffset::acquire() {
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    dev.togglePFEBBcidResetTrigger();
    const std::string rname{"017_pfeb_bcid_error_READONLY"};
    const auto val = dev.readSubRegister(rname, "pfeb_bcid_error");
    ERS_INFO(fmt::format("Found {} reg{:#04x} = {:#010x} when pad offset = {:#05x} and TDS offset = {:#05x}",
                         dev.getName(), dev.addressFromRegisterName(rname), val, m_pad_trigger_bcid_offset, counter()));
    fillTree(val);
  }
}

void nsw::sTGCPadTdsBcidOffset::unconfigure() {
  if (counter() == total() - 1) {
    closeTree();
  }
}

void nsw::sTGCPadTdsBcidOffset::checkObjects() const {
  const std::size_t npads = getDeviceManager().getPadTriggers().size();
  const std::size_t nfebs = getDeviceManager().getFebs().size();
  ERS_INFO(fmt::format("Found {} pad triggers", npads));
  ERS_INFO(fmt::format("Found {} FEBs", nfebs));
  if (npads != std::size_t{1}) {
    const auto msg = std::string("Requires 1 pad trigger");
    ers::error(nsw::NSWsTGCPadTdsBcidOffsetIssue(ERS_HERE, msg));
  }
}

void nsw::sTGCPadTdsBcidOffset::setTdsBcidOffsets() const {
  ERS_INFO(fmt::format("Write {:#010x} to tds register {}", counter(), m_tdsReg));
  auto threads = std::vector< std::future<void> >();
  threads.reserve(std::size(getDeviceManager().getFebs()));
  for (const auto& dev: getDeviceManager().getFebs()) {
    threads.push_back(std::async(std::launch::async, &nsw::sTGCPadTdsBcidOffset::setTdsBcidOffset, this, dev));
  }
  for (auto& thread: threads) {
    thread.get();
  }
}

void nsw::sTGCPadTdsBcidOffset::setTdsBcidOffset(const nsw::hw::FEB& dev) const {
  if (nsw::getElementType(dev.getScaAddress()) != "PFEB") {
    return;
  }
  for (const auto& tds : dev.getTdss()) {
    tds.writeValue(std::string{m_tdsReg}, static_cast<unsigned int>(counter()));
  }
}

void nsw::sTGCPadTdsBcidOffset::openTree() {
  m_runnumber = runNumber();
  const auto app_name = applicationName();
  const auto now = nsw::calib::utils::strf_time();
  m_rname = fmt::format("{}.{}.{}.{}.root", m_calibType, m_runnumber, app_name, now);
  ERS_INFO(fmt::format("Opening TFile/TTree {}", m_rname));
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_rtree->Branch("runnumber",               &m_runnumber);
  m_rtree->Branch("pad_trigger",             &m_pad_trigger);
  m_rtree->Branch("pad_trigger_bcid_offset", &m_pad_trigger_bcid_offset);
  m_rtree->Branch("tds_bcid_offset",         &m_tds_bcid_offset);
  m_rtree->Branch("pfeb_bcid_error",         &m_pfeb_bcid_error);
  for (const auto& dev: getDeviceManager().getPadTriggers()) {
    m_pad_trigger = dev.getName();
  }
}

void nsw::sTGCPadTdsBcidOffset::closeTree() {
  ERS_INFO("Closing TFile/TTree");
  m_rtree->Write();
  m_rfile->Close();
}

void nsw::sTGCPadTdsBcidOffset::fillTree(const std::uint64_t bcid_error) {
  m_pfeb_bcid_error = bcid_error;
  m_tds_bcid_offset = counter();
  m_rtree->Fill();
}
