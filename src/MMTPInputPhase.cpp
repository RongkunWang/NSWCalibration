#include "NSWCalibration/MMTPInputPhase.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"

#include <unistd.h>
#include <stdexcept>

#include "ers/ers.h"

using namespace std::chrono_literals;

void nsw::MMTPInputPhase::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  // parse calib type
  ERS_INFO("Calib type: " << m_calibType);

  // make NSWConfig objects from input db
  ERS_INFO("Finding MMTPs");
  ERS_INFO("Found " << getDeviceManager().getMMTps().size() << " MMTPs");

  // make output
  m_phase  = 0;
  m_offset = 0;

  if (  m_calibType == "MMTPInputPhase_PhaseOnly" || 
        m_calibType == "MMTPInputPhase_Validation" 
        ) {
    m_noffsets = 1;
    ERS_INFO("Calib type is " << m_calibType << ", setting m_noffsets = 1");
  }
  // set number of iterations
  setTotal(m_nreads * m_nphases * m_noffsets);

  nsw::snooze();
}

void nsw::MMTPInputPhase::setupRootFile() {
  m_now = nsw::calib::utils::strf_time();
  std::string rname = "tpscax." + std::to_string(runNumber()) + "."
    + applicationName() + "." + m_now + ".root";
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
  const std::uint32_t phase_offset = counter() / m_nreads;
  const std::uint32_t nth_read     = counter() % m_nreads;
  const std::uint32_t phase        = phase_offset / m_noffsets;
  const std::uint32_t AddcOffset       = phase_offset % m_noffsets;
  for (auto & tp : getDeviceManager().getMMTps()) {
    if (nth_read == 0) {
      configure_tp(tp, phase, AddcOffset);
    }
    read_tp       (tp, phase, AddcOffset);
  }
}

void nsw::MMTPInputPhase::unconfigure() {
  ERS_INFO("MMTPInputPhase::unconfigure " << counter());
}

nsw::commands::Commands nsw::MMTPInputPhase::getAltiSequences() const {
  return {{}, // before configure
          {}, // during (before acquire)
          {}   // after (before unconfigure)
  };
}

int nsw::MMTPInputPhase::configure_tp(const nsw::hw::MMTP & tp, const std::uint32_t phase, const std::uint32_t AddcOffset) const {
    ERS_INFO("Configuring " << tp.getScaAddress()
           << " with phase=" << phase
           << " and ADDC offset=" << AddcOffset);
    if (!simulation()) {
      // always set the phase
      tp.writeRegister(nsw::mmtp::REG_INPUT_PHASE, phase);

      // set offset during non-validation
      if(m_calibType != "MMTPInputPhase_Validation") {
        // in PhaseOnly, ADDC phase will be 0, otherwise, it'll be 0..7
        tp.writeRegister(nsw::mmtp::REG_INPUT_PHASEADDCOFFSET, AddcOffset);
        // in Phase Only, make sure to set the L1DDC offset to 0 (this calibration is not designed for scanning over L1DDC offset)
        if(m_calibType == "MMTPInputPhase_PhaseOnly") {
          tp.writeRegister(nsw::mmtp::REG_INPUT_PHASEL1DDCOFFSET, 0);
        }
      }
      // 64 million BC clock with error it will become 1.
      // 64e6 * 25e-9 = 1.6 seconds
      std::this_thread::sleep_for(5000ms);
    }
    return 0;
}

int nsw::MMTPInputPhase::read_tp(const nsw::hw::MMTP & tp, const std::uint32_t phase, const std::uint32_t AddcOffset) {
  ERS_INFO("Reading " << tp.getScaAddress()
           << " with phase=" << phase
           << " and ADDC offset=" << AddcOffset);
  // open output file
  // on first iteration
  if (counter() == 0) {
    setupRootFile();
    m_myfile.open("tpscax." + std::to_string(runNumber()) + "." + applicationName() + "." + m_now + ".txt");
  }

  // clear
  m_align->clear();
  m_bcid ->clear();
  m_fiber->clear();

  // for acquiring data
  std::uint32_t data_align{};
  std::vector<uint8_t> data_bcids_total = {};

  // read the 32-bit word of fiber alignment
  if (!simulation())
    data_align = tp.readRegister(nsw::mmtp::REG_FIBER_ALIGNMENT);

  // read the 4 32-bit words of fiber BCIDs (4 LSB per fiber)
  for (auto reg : nsw::mmtp::REG_FIBER_BCIDS) {
    if (!simulation()) {
      auto data_bcids = tp.readRegister(reg);
      for (auto byte : nsw::intToByteVector(data_bcids, nsw::NUM_BYTES_IN_WORD32, nsw::scax::SCAX_LITTLE_ENDIAN))
        data_bcids_total.push_back(byte);
    }
  }

  // write the header
  m_myfile << nsw::calib::utils::strf_time() << " " << phase << " " << AddcOffset << " ";
  m_now    = nsw::calib::utils::strf_time();
  m_phase  = phase;
  m_offset = AddcOffset;

  // write the alignment
  // NB: bytes are returned in reverse order
  m_myfile << std::bitset<32>(data_align) << " ";

  // write the bcids
  for (auto byte : data_bcids_total)
    m_myfile << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte) << std::dec;
  m_myfile << std::endl;

  // write the alignment and bcids
  for (size_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
    m_fiber->push_back(static_cast<int>(fiber));

    // alignment
    const auto align  = (data_align >> fiber) & 0x1;
    m_align->push_back(static_cast<int>(align));

    // bcid
    // TODO(AT): char ordering
    const auto byte = data_bcids_total.at(fiber / 2);
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
