#include "NSWCalibration/MMTPStatusRegisters.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Constants.h"
#include "NSWConfiguration/TPConfig.h"
#include "NSWConfiguration/Utility.h"

#include <unistd.h>
#include <stdexcept>
#include <boost/property_tree/json_parser.hpp>

#include "ers/ers.h"
#include "TROOT.h"

nsw::MMTPStatusRegisters::MMTPStatusRegisters() {
  ROOT::EnableThreadSafety();
  m_now = nsw::calib::utils::strf_time();
  m_config = "";
  m_opc_ip = "";
  m_tp_address = "";
  m_metadata = "";
  m_run_number = "0000000000";
  m_lab = "XXX";
  m_sector = "YYY";
  m_reset_l1a = true;
  m_sleep_time = 1;
}

void nsw::MMTPStatusRegisters::Initialize() {
  InitializeMMTPConfig();
  InitializeTTree();
}

void nsw::MMTPStatusRegisters::Execute() {
  ExecuteStartOfLoop();
  ExecuteFiberIndex();
  ExecuteBufferOverflow();
  ExecuteFiberAlignment();
  ExecuteHotVMMs();
  ExecuteResetL1A();
  ExecuteEndOfLoop();
}

void nsw::MMTPStatusRegisters::Finalize() {
  FinalizeTTree();
}

void nsw::MMTPStatusRegisters::ExecuteStartOfLoop() {

  //
  // update loop
  //
  m_event = m_event + 1;
  m_now = nsw::calib::utils::strf_time();
  m_fiber_index->clear();
  m_fiber_align->clear();
  m_fiber_masks->clear();
  m_fiber_hots ->clear();

}

void nsw::MMTPStatusRegisters::ExecuteFiberIndex() {

  //
  // write the fiber index for easier navigating
  //
  for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
    m_fiber_index->push_back(fiber);
  }

}

void nsw::MMTPStatusRegisters::ExecuteBufferOverflow() {

  auto cs = std::make_unique<nsw::ConfigSender>();
  const auto& tp = m_tps.at(0);

  //
  // read buffer overflow
  //
  if (!Sim()) {
    bool quiet = true;
    cs->sendTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW, 0x00, quiet);
    cs->sendTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW, 0x01, quiet);
    cs->sendTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW, 0x00, quiet);
    m_overflow_word = nsw::byteVectorToWord32(
      cs->readTpConfigRegister(tp, nsw::mmtp::REG_PIPELINE_OVERFLOW)
    );
  } else {
    m_overflow_word = 0;
  }

}

void nsw::MMTPStatusRegisters::ExecuteFiberAlignment() {

  auto cs = std::make_unique<nsw::ConfigSender>();
  const auto& tp = m_tps.at(0);
  uint32_t fiber_align_word;

  //
  // read fiber alignment
  //
  if (!Sim()) {
    fiber_align_word = nsw::byteVectorToWord32(
      cs->readTpConfigRegister(tp, nsw::mmtp::REG_FIBER_ALIGNMENT)
    );
  } else {
    fiber_align_word = 0;
  }

  //
  // get alignment of each fiber (bit)
  //
  for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {
    m_fiber_align->push_back((fiber_align_word >> fiber) & 0b1);
  }

}

void nsw::MMTPStatusRegisters::ExecuteHotVMMs() {

  auto cs = std::make_unique<nsw::ConfigSender>();
  const auto& tp = m_tps.at(0);
  bool quiet = true;

  //
  // loop over fibers
  //
  for (uint32_t fiber = 0; fiber < nsw::mmtp::NUM_FIBERS; fiber++) {

    //
    // set the fiber of interest
    //
    if (!Sim()) {
      cs->sendTpConfigRegister(tp, nsw::mmtp::REG_FIBER_HOT_MUX, fiber, quiet);
    }

    //
    // read hot fibers
    //
    uint32_t fiber_hot = 0xffff;
    if (!Sim()) {
      fiber_hot = nsw::byteVectorToWord32(
        cs->readTpConfigRegister(tp, nsw::mmtp::REG_FIBER_HOT_READ)
      );
    } else {
      fiber_hot = std::pow(2, fiber);
    }
    m_fiber_hots->push_back(fiber_hot);
    m_fiber_masks->push_back(0);

    //
    // debug
    //
    if (Debug()) {
      for (auto val: *(m_fiber_hots.get())) {
        std::cout << "Hot VMM : " << std::bitset<nsw::mmtp::NUM_VMMS_PER_FIBER>(val) << std::endl;
      }
    }

  }

}

void nsw::MMTPStatusRegisters::ExecuteResetL1A() {

  auto cs = std::make_unique<nsw::ConfigSender>();
  const auto& tp = m_tps.at(0);

  //
  // reset L1A packet builder
  //
  if (ResetL1A()) {
    if (Debug()) {
      ERS_LOG("Sending TP config to reset L1A builder for "
              << m_opc_ip << " " << m_tp_address);
    }
    bool quiet = true;
    if (!Sim()) {
      cs->sendTpConfig(tp, quiet);
    }
  }

}

void nsw::MMTPStatusRegisters::ExecuteEndOfLoop() {

  //
  // end loop
  //
  m_rtree->Fill();
  ERS_INFO("Finished iteration " << m_event);
  sleep(m_sleep_time);

}

void nsw::MMTPStatusRegisters::FinalizeTTree() {

  //
  // close files
  //
  ERS_INFO("Closing " << m_rname);
  m_rfile->cd();
  m_rtree->Write();
  m_rfile->Close();
  ERS_INFO("Closed " << m_rname);

}

void nsw::MMTPStatusRegisters::InitializeMMTPConfig() {

  //
  // TP objects
  //
  auto cfg = "json://" + m_config;
  m_tps = nsw::ConfigReader::makeObjects<nsw::TPConfig>(cfg, "TP");
  if (m_tps.size() > 1) {
    std::string msg = "Can only analyze 1 TP for now.";
    ERS_INFO(msg);
    throw std::runtime_error(msg);
  }

  const auto& tp = m_tps.at(0);
  m_opc_ip     = tp.getOpcServerIp();
  m_tp_address = tp.getAddress();

}

void nsw::MMTPStatusRegisters::InitializeTTree() {
  m_rname = "mmtp_diagnostics." + Metadata() + "." + m_now + ".root";
  m_rfile = std::make_unique< TFile >(m_rname.c_str(), "recreate");
  m_rtree = std::make_shared< TTree >("nsw", "nsw");
  m_event            = -1;
  m_overflow_word    = -1;
  m_fiber_align_word = -1;
  m_fiber_index = std::make_unique< std::vector<uint32_t> >();
  m_fiber_align = std::make_unique< std::vector<uint32_t> >();
  m_fiber_masks = std::make_unique< std::vector<uint32_t> >();
  m_fiber_hots  = std::make_unique< std::vector<uint32_t> >();
  m_rtree->Branch("time",          &m_now);
  m_rtree->Branch("event",         &m_event);
  m_rtree->Branch("opc_ip",        &m_opc_ip);
  m_rtree->Branch("tp_address",    &m_tp_address);
  m_rtree->Branch("overflow_word", &m_overflow_word);
  m_rtree->Branch("sleep_time",    &m_sleep_time);
  m_rtree->Branch("reset_l1a",     &m_reset_l1a);
  m_rtree->Branch("fiber_index",   m_fiber_index.get());
  m_rtree->Branch("fiber_align",   m_fiber_align.get());
  m_rtree->Branch("fiber_masks",   m_fiber_masks.get());
  m_rtree->Branch("fiber_hots",    m_fiber_hots.get());
}

std::string nsw::MMTPStatusRegisters::Metadata() const {
  if (m_metadata != "")
    return m_metadata;
  return m_run_number + "." + m_lab + "." + m_sector;
}
