#include "NSWCalibration/MMTPInputPhase.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

void nsw::MMTPInputPhase::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  // parse calib type
  if (m_calibType=="MMTPInputPhase") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for MMTPInputPhase: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  ERS_INFO("Finding MMTPs");
  m_tps = nsw::ConfigReader::makeObjects<nsw::TPConfig> (db, "TP");
  ERS_INFO("Found " << m_tps.size() << " MMTPs");

  // make output
  m_now = nsw::calib::utils::strf_time();
  std::string rname = "tpscax." + std::to_string(runNumber()) + "."
    + applicationName() + "." + m_now + ".root";
  m_phase  = 0;
  m_offset = 0;
  m_rfile  = std::make_unique< TFile >(rname.c_str(), "recreate");
  m_rtree  = std::make_shared< TTree >("nsw", "nsw");
  m_align  = std::make_unique< std::vector<int> >();
  m_bcid   = std::make_unique< std::vector<int> >();
  m_fiber  = std::make_unique< std::vector<int> >();
  m_rtree->Branch("time",         &m_now);
  m_rtree->Branch("phase",        &m_phase);
  m_rtree->Branch("offset",       &m_offset);
  m_rtree->Branch("fiber_align",  m_align.get());
  m_rtree->Branch("fiber_bcid",   m_bcid.get());
  m_rtree->Branch("fiber_index",  m_fiber.get());

  // set number of iterations
  setTotal(m_nreads * m_nphases * m_noffsets);

  nsw::snooze();
}

void nsw::MMTPInputPhase::configure() {
  //
  // Suppose m_nreads = 2. Then:
  // counter  phase  offset  do_config
  //    0       0      0         1
  //    1       0      0         0
  //    2       0      1         1
  //    3       0      1         0
  //    4       0      2         1
  //    5       0      2         0
  //    6       0      3         1
  //    7       0      3         0
  //    8       0      4         1
  // etc
  //
  ERS_INFO("MMTPInputPhase::configure " << counter());
  const uint32_t phase_offset = counter() / m_nreads;
  const uint32_t nth_read     = counter() % m_nreads;
  const uint32_t phase        = phase_offset / m_noffsets;
  const uint32_t offset       = phase_offset % m_noffsets;
  for (auto & tp : m_tps) {
    if (nth_read == 0)
      configure_tp(tp, phase, offset);
    read_tp       (tp, phase, offset);
  }
}

void nsw::MMTPInputPhase::unconfigure() {
  ERS_INFO("MMTPInputPhase::unconfigure " << counter());
}

nsw::commands::Commands nsw::MMTPInputPhase::getAltiSequences() const {
  return {{}, // before configure
          {nsw::commands::actionStartPG}, // during (before acquire)
          {nsw::commands::actionStopPG}   // after (before unconfigure)
  };
}

int nsw::MMTPInputPhase::configure_tp(const nsw::TPConfig & tp, uint32_t phase, uint32_t offset) const {
    ERS_INFO("Configuring " << tp.getAddress()
           << " with phase=" << phase
           << " and offset=" << offset);
    auto cs = std::make_unique<nsw::ConfigSender>();
    if (!simulation()) {
      cs->sendSCAXRegister(tp, nsw::mmtp::REG_INPUT_PHASE,       phase);
      cs->sendSCAXRegister(tp, nsw::mmtp::REG_INPUT_PHASEOFFSET, offset);
    }
    return 0;
}

int nsw::MMTPInputPhase::read_tp(const nsw::TPConfig & tp, uint32_t phase, uint32_t offset) {
  ERS_INFO("Reading " << tp.getAddress()
           << " with phase=" << phase
           << " and offset=" << offset);
  // open output file
  // on first iteration
  auto cs   = std::make_unique<nsw::ConfigSender>();
  auto ip   = tp.getOpcServerIp();
  auto addr = tp.getAddress();
  if (counter() == 0)
    m_myfile.open("tpscax." + std::to_string(runNumber()) + "." + applicationName() + "." + m_now + ".txt");

  // clear
  m_align->clear();
  m_bcid ->clear();
  m_fiber->clear();

  // for acquiring data
  std::vector<uint8_t> data_align = {nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL};
  std::vector<uint8_t> data_bcids = {nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL, nsw::mmtp::DUMMY_VAL};
  std::vector<uint8_t> data_bcids_total = {};

  // read the 32-bit word of fiber alignment
  if (!simulation())
    data_align = cs->readSCAXRegister(tp, nsw::mmtp::REG_FIBER_ALIGNMENT);

  // read the 4 32-bit words of fiber BCIDs (4 LSB per fiber)
  for (auto reg : nsw::mmtp::REG_FIBER_BCIDS) {
    if (!simulation())
      data_bcids = cs->readSCAXRegister(tp, reg);
    for (auto byte : data_bcids)
      data_bcids_total.push_back(byte);
  }

  // write the header
  m_myfile << nsw::calib::utils::strf_time() << " " << phase << " " << offset << " ";
  m_now    = nsw::calib::utils::strf_time();
  m_phase  = phase;
  m_offset = offset;

  // write the alignment
  // NB: bytes are returned in reverse order
  for (auto byte = data_align.rbegin(); byte != data_align.rend(); ++byte)
    m_myfile << std::bitset<8>(*byte);
  m_myfile << " ";

  // write the bcids
  for (auto byte : data_bcids_total)
    m_myfile << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte) << std::dec;
  m_myfile << std::endl;

  // write the alignment and bcids
  for (size_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
    m_fiber->push_back(static_cast<int>(fiber));

    // alignment
    auto byte   = data_align.at(fiber / nsw::NUM_BITS_IN_BYTE);
    auto bitpos = fiber % nsw::NUM_BITS_IN_BYTE;
    auto align  = (byte >> bitpos) & 0x1;
    m_align->push_back(static_cast<int>(align));

    // bcid
    // TODO(AT): char ordering
    byte = data_bcids_total.at(fiber / 2);
    int bcid = 0;
    if (fiber % 2 == 0)
      bcid = (byte >> 0) & 0xf;
    else
      bcid = (byte >> 4) & 0xf;
    m_bcid->push_back(bcid);
  }
  m_rtree->Fill();

  // close output file
  // on last iteration
  if (counter() == total()-1) {
    m_myfile.close();
    m_rtree->Write();
    m_rfile->Close();
  }

  return 0;
}
