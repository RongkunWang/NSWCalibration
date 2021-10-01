#include "NSWCalibration/sTGCPadTriggerInputDelays.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

#include <unistd.h>
#include <stdexcept>

#include <ers/ers.h>
nsw::sTGCPadTriggerInputDelays::sTGCPadTriggerInputDelays(const std::string& calibType) : 
  m_calibType(calibType) {
  setCounter(-1);
  setTotal(0);
}

void nsw::sTGCPadTriggerInputDelays::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  //
  // parse calib type
  //
  setup_type();

  //
  // make objects and outputs
  //
  setup_objects(db);
  setup_tree();

  //
  // set number of iterations
  //
  setTotal(nsw::padtrigger::NUM_INPUT_DELAYS);

  //
  // other settings
  //
  setToggle(false);
  setWait4swROD(false);
  nsw::snooze();
}

void nsw::sTGCPadTriggerInputDelays::configure() {
  ERS_INFO("sTGCPadTriggerInputDelays::configure " << counter());
  for (const auto& pt: m_pts) {
    set_delays(pt);
    for (uint32_t i = 0; i < nsw::padtrigger::NUM_PFEB_BCID_READS; i++) {
      read_bcids(pt);
      fill();
    }
  }
}

void nsw::sTGCPadTriggerInputDelays::unconfigure() {
  ERS_INFO("sTGCPadTriggerInputDelays::unconfigure " << counter());
  if (counter() == total() - 1) {
    close_tree();
  }
}

void nsw::sTGCPadTriggerInputDelays::set_delays(const nsw::PadTriggerSCAConfig & pt) {
  m_delay = 0;
  for (size_t it = 0; it < nsw::NUM_BITS_IN_WORD32 / nsw::padtrigger::NUM_BITS_PER_PFEB_BCID; it++) {
    m_delay += (static_cast<uint32_t>(counter()) << it*4);
  }
  ERS_INFO("Configuring " << pt.getAddress() << " with delay = " << std::hex << m_delay);
  if (!simulation()) {
    bool quiet = false;
    auto cs = std::make_unique<nsw::ConfigSender>();
    cs->sendPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_DELAY_23_16, m_delay, quiet);
    cs->sendPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_DELAY_15_08, m_delay, quiet);
    cs->sendPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_DELAY_07_00, m_delay, quiet);
  }
  ERS_LOG("Done configuring " << pt.getAddress());
}

void nsw::sTGCPadTriggerInputDelays::read_bcids(const nsw::PadTriggerSCAConfig & pt) {

  //
  // Read BCID words
  //
  ERS_LOG("Reading PFEB BCIDs of " << pt.getAddress());
  uint32_t bcids_23_16 = 0;
  uint32_t bcids_15_08 = 0;
  uint32_t bcids_07_00 = 0;
  if (!simulation()) {
    auto cs = std::make_unique<nsw::ConfigSender>();
    bcids_23_16 = cs->readPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_BCID_23_16);
    bcids_15_08 = cs->readPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_BCID_15_08);
    bcids_07_00 = cs->readPadTriggerConfigRegister(pt, nsw::padtrigger::REG_PFEB_BCID_07_00);
  }

  //
  // Decode BCIDs
  //
  ERS_LOG("Decoding PFEB BCIDs");
  m_pfeb->clear();
  m_bcid->clear();
  for (const auto& bcid: pt.PFEBBCIDs(bcids_07_00,
                                      bcids_15_08,
                                      bcids_23_16)) {
    m_pfeb->push_back(static_cast<int>(m_bcid->size()));
    m_bcid->push_back(static_cast<int>(bcid));
  }
}

void nsw::sTGCPadTriggerInputDelays::fill() {
  for (const auto& pt: m_pts) {
    m_opcserverip = pt.getOpcServerIp();
    m_address     = pt.getAddress();
    break;
  }
  m_now = nsw::calib::utils::strf_time();
  m_rtree->Fill();
  m_pfeb->clear();
  m_bcid->clear();
}

void nsw::sTGCPadTriggerInputDelays::setup_type() {
  if (m_calibType == "sTGCPadTriggerInputDelays") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCPadTriggerInputDelays: "
        << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }
}

void nsw::sTGCPadTriggerInputDelays::setup_objects(const std::string& db) {
  ERS_INFO("Finding PTs");
  m_pts = nsw::ConfigReader::makeObjects<nsw::PadTriggerSCAConfig>
    (db, "PadTriggerSCA");
  ERS_INFO("Found " << m_pts.size() << " pad triggers");
  if (m_pts.size() != 1) {
    std::stringstream msg;
    msg << "sTGCPadTriggerInputDelays only works with 1 PT now. Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }
}

void nsw::sTGCPadTriggerInputDelays::setup_tree() {
  m_now = nsw::calib::utils::strf_time();
  m_rname = std::string("padtriggerinputdelays")
    + "." + std::to_string(runNumber())
    + "." + applicationName()
    + "." + m_now
    + ".root";
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_rtree->Branch("opcserverip", &m_opcserverip);
  m_rtree->Branch("address",     &m_address);
  m_rtree->Branch("time",        &m_now);
  m_rtree->Branch("delay",       &m_delay);
  m_rtree->Branch("pfeb_bcid",    m_bcid.get());
  m_rtree->Branch("pfeb_index",   m_pfeb.get());
}

void nsw::sTGCPadTriggerInputDelays::close_tree() {
  ERS_INFO("Closing ");
  m_rtree->Write();
  m_rfile->Close();
}
