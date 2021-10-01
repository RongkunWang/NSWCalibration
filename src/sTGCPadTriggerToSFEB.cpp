#include "NSWCalibration/sTGCPadTriggerToSFEB.h"
#include "NSWCalibration/Utility.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/Utility.h"

#include <fstream>
#include <unistd.h>
#include <stdexcept>

#include <ers/ers.h>

nsw::sTGCPadTriggerToSFEB::sTGCPadTriggerToSFEB(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCPadTriggerToSFEB::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  m_dry_run = false;

  // parse calib type
  if (m_calibType=="sTGCPadTriggerToSFEB") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCPadTriggerToSFEB: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  // can be SFEB, SFEB8, or SFEB6 :(
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB8"))
    m_sfebs.push_back(feb);
  for (auto feb: nsw::ConfigReader::makeObjects<nsw::FEBConfig> (db, "SFEB6"))
    m_sfebs.push_back(feb);
  ERS_INFO("Found " << m_sfebs.size() << " SFEBs");

  // protect against no sFEBs
  if (m_sfebs.size() == 0) {
    const std::string msg = "Error in sTGCPadTriggerToSFEB: No sFEBs in this config!";
    ERS_INFO(msg);
    throw std::runtime_error(msg);
  }

  // start dog
  m_watchdog = std::async(std::launch::async, &nsw::sTGCPadTriggerToSFEB::sfeb_watchdog, this);

  // keep it simple
  setTotal(1);
  setToggle(false);
  setWait4swROD(false);
  usleep(1e6);
}

void nsw::sTGCPadTriggerToSFEB::configure() {
  ERS_INFO("sTGCPadTriggerToSFEB::configure " << counter());
  constexpr int seconds = 600;
  for (int second = 0; second < seconds; second++) {
    nsw::snooze(std::chrono::milliseconds(100));
    ERS_INFO("sTGCPadTriggerToSFEB::sleeping " << second+1 << " / " << seconds);

    // check on the watchdog
    auto status = m_watchdog.wait_for(std::chrono::milliseconds(1));
    if (status == std::future_status::ready) {
      try {
        m_watchdog.get();
      } catch (std::exception & e) {
        std::stringstream msg;
        msg << "Error in sTGCPadTriggerToSFEB: " << e.what();
        nsw::NSWsTGCPadTriggerToSFEBIssue issue(ERS_HERE, msg.str());
        ers::warning(issue);
        break;
      }
    }
  }
}

void nsw::sTGCPadTriggerToSFEB::unconfigure() {
  ERS_INFO("sTGCPadTriggerToSFEB::unconfigure " << counter());
  usleep(1e6);
}

int nsw::sTGCPadTriggerToSFEB::sfeb_watchdog() const {
  //
  // read sTDS monitoring registers
  //
  if (m_sfebs.size() == 0) {
    ERS_INFO("Error in sTGCPadTriggerToSFEB: No sFEBs in this config!");
    return 0;
  }

  // sleep time
  constexpr auto slp = std::chrono::milliseconds(200);

  // output file and announce
  std::string fname = "sfeb_register15_" + nsw::calib::utils::strf_time() + ".txt";
  std::ofstream myfile;
  myfile.open(fname);
  ERS_INFO("SFEB watchdog. Output: " << fname);

  // monitor
  // pointer < sfebs < threads < tds < register15 > > > >
  auto threads = std::make_unique<std::vector< std::future< std::vector<uint32_t> > > >();
  while (counter() < total()) {
    myfile << "Time " << nsw::calib::utils::strf_time() << std::endl;
    for (auto & feb : m_sfebs)
      threads->push_back( std::async(std::launch::async,
                                     &nsw::sTGCPadTriggerToSFEB::sfeb_register15,
                                     this,
                                     feb) );
    for (size_t is = 0; is < m_sfebs.size(); is++) {
      auto result = threads->at(is).get();
      for (size_t it = 0; it < m_sfebs.at(is).getTdss().size(); it++) {
        auto name_sfeb = m_sfebs.at(is).getAddress();
        auto name_tds  = m_sfebs.at(is).getTdss().at(it).getName();
        auto val       = result.at(it);
        std::stringstream valstream;
        valstream << std::setw(8) << std::setfill('0') << std::hex << val;
        myfile << name_sfeb << " " << name_tds << " 0x" << valstream.str() << std::endl;
      }
    }
    threads->clear();
    nsw::snooze(slp);
  }

  // close
  ERS_INFO("Closing " << fname);
  myfile.close();
  return 0;
}

std::vector<uint32_t> nsw::sTGCPadTriggerToSFEB::sfeb_register15(const nsw::FEBConfig & feb) const {
  auto cs = std::make_unique<nsw::ConfigSender>();
  auto opc_ip   = feb.getOpcServerIp();
  auto sca_addr = feb.getAddress();
  auto regs     = std::vector<uint32_t>();
  for (auto tds : feb.getTdss()) {
    const std::string address_to_read("register15");
    const std::string tds_i2c_address("register15_READONLY");
    const std::string full_node_name = sca_addr + "." + tds.getName()  + "." + address_to_read;
    auto size_in_bytes = tds.getTotalSize(tds_i2c_address) / 8;
    std::vector<uint8_t> dataread(size_in_bytes);
    constexpr size_t attempts = 5;
    for (size_t attempt = 0; attempt < attempts; attempt++) {
      try {
        if (!m_dry_run)
          dataread = cs->readI2c(opc_ip, full_node_name, size_in_bytes);
        auto reg_str = nsw::vectorToBitString(dataread);
        auto reg_32  = static_cast<uint32_t>(std::stoul(reg_str, nullptr, 2));
        regs.push_back(reg_32);
        break;
      } catch (std::exception & e) {
        ERS_LOG("Catching " << sca_addr << "." << tds.getName() << " on attempt " << attempt);
        if (attempt == attempts-1) {
          const std::string msg = "Error: failed to read reg15 for " + sca_addr + "." + tds.getName();
          ERS_INFO(msg);
          throw std::runtime_error(msg);
        }
      }
    }
  }
  return regs;
}
