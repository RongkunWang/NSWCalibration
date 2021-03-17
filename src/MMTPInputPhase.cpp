#include "NSWCalibration/MMTPInputPhase.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

nsw::MMTPInputPhase::MMTPInputPhase(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

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
  setToggle(true);
  setWait4swROD(false);
  usleep(1e6);
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
  const int phase_offset = counter() / m_nreads;
  const int nth_read     = counter() % m_nreads;
  const int phase        = phase_offset / m_noffsets;
  const int offset       = phase_offset % m_noffsets;
  for (auto & tp : m_tps) {
    if (nth_read == 0)
      configure_tp(tp, phase, offset);
    read_tp       (tp, phase, offset);
  }
}

void nsw::MMTPInputPhase::unconfigure() {
  ERS_INFO("MMTPInputPhase::unconfigure " << counter());
}

int nsw::MMTPInputPhase::configure_tp(const nsw::TPConfig & tp, int phase, int offset) const {
    ERS_INFO("Configuring " << tp.getAddress()
           << " with phase=" << phase
           << " and offset=" << offset);
    constexpr uint8_t phreg     = 0x0B;
    constexpr uint8_t offsetreg = 0x0C;
    auto ip   = tp.getOpcServerIp();
    auto addr = tp.getAddress();
    auto cs   = std::make_unique<nsw::ConfigSender>();
    if (!simulation()) {
      cs->sendI2cAtAddress(ip, addr, {0x00, 0x00, 0x00, phreg},
                           nsw::intToByteVector(phase, 4, true));
      cs->sendI2cAtAddress(ip, addr, {0x00, 0x00, 0x00, offsetreg},
                           nsw::intToByteVector(offset, 4, true));
    }
    return 0;
}

int nsw::MMTPInputPhase::read_tp(const nsw::TPConfig & tp, int phase, int offset) {
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

  auto fiber_alignment                  = nsw::hexStringToByteVector("0x02", 4, true);
  const std::vector<std::string> bxlsb  = {"0x04", "0x05", "0x06", "0x07"};
  std::vector<uint8_t> data_align       = {0x11, 0x11, 0x11, 0x11};
  std::vector<uint8_t> data_bcids       = {0x55, 0x55, 0x55, 0x55};
  std::vector<uint8_t> data_bcids_total = {};

  // read the 32-bit word of fiber alignment
  if (!simulation())
    data_align = cs->readI2cAtAddress(ip, addr, fiber_alignment.data(), fiber_alignment.size(), 4);

  // read the 4 32-bit words of fiber BCIDs (4 LSB per fiber)
  for (auto reg : bxlsb) {
    auto bxdata = nsw::hexStringToByteVector(reg, 4, true);
    if (!simulation())
      data_bcids = cs->readI2cAtAddress(ip, addr, bxdata.data(), bxdata.size(), 4);
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
    m_myfile << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte) << std::dec;
  m_myfile << std::endl;

  // write the alignment and bcids
  for (size_t fiber = 0; fiber < 8*data_align.size(); fiber++) {
    m_fiber->push_back((int)(fiber));

    // alignment
    auto byte   = data_align.at(fiber / 8);
    auto bitpos = fiber % 8;
    auto align  = (byte >> bitpos) & 1;
    m_align->push_back((int)(align));

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
