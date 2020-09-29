#include "NSWCalibration/SCAIDCalib.h"

namespace nsw {

ScaIdCalib::ScaIdCalib(const std::string& calib_type) : m_calib_type("SCAIDFetch") {
  if (calib_type == "SCAIDTest") {
    NSWSCAIDCalibIssue issue(ERS_HERE, "SCAIDTest calibration type not supported yet, defaulting to SCAIDFetch");
    ers::warning(issue);
  }
  setTotal(1);
  setCounter(-1); // required for running once: next() is called at the beginning of the handler loop
  setToggle(false);
}

void ScaIdCalib::setup(std::string db) {
  if (m_calib_type == "SCAIDTest") {
    // Get the SCAID <-> Physical location table, parse into some in-memory data structure...
  }

  // Get all boards with SCA chips from configuration database
  ConfigReader reader(db);
  try {
    reader.readConfig();
  } catch (const std::exception& ex) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "Could not read configuration database: " + std::string(ex.what()));
    ers::fatal(issue);
    throw issue;
  }

  const auto element_names = reader.getAllElementNames();
  for (const auto& name : element_names) {
    // TPs have no SCA chips
    if (name.find("MMTP") == std::string::npos && name.find("STGCTP") == std::string::npos)
      m_boards.emplace(name, reader.readConfig(name));
  }
}

void ScaIdCalib::configure() {
  fetch_sca_ids();
  write_sca_ids("sca_ids.json");
}

void ScaIdCalib::fetch_sca_ids() {
  // Read SCA IDs from each board
  std::unordered_map<std::string, OpcClient> clients;
  for (const auto& pair : m_boards) {
    const auto& opc_ip = pair.second.getOpcServerIp();
    if (clients.find(opc_ip) == clients.end()) {
      // Create new pairing: OPC server IP <-> OPC client (note the `emplace`)
      try {
        clients.emplace(opc_ip, opc_ip);
      } catch (const std::exception& ex) {
        NSWSCAIDCalibIssue issue(ERS_HERE, "Could not connect to OPC server " + pair.second.getOpcServerIp() + ": " + ex.what());
        ers::error(issue);
      }
    }
    try {
      auto id = clients.at(opc_ip).readScaID(pair.second.getAddress());
      m_ids.emplace(pair.first, id);
    } catch (const std::exception& ex) {
      NSWSCAIDCalibIssue issue(ERS_HERE, "Could not read SCA ID of board at address " + pair.second.getAddress() + ": " + ex.what());
      ers::error(issue);
    }
  }
}

void ScaIdCalib::write_sca_ids(const std::string& filepath) const {
  if (m_ids.empty()) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "No SCA IDs found, writing empty list to disk anyway");
    ers::warning(issue);
  }

  boost::property_tree::ptree pt;
  std::stringstream hex_stream;
  // SCA IDs are 24 bits (6 hex digits) long
  hex_stream << std::setfill('0') << std::setw(6) << std::hex;
  for (const auto& pair : m_ids) {
    hex_stream << "0x" << pair.second;
    pt.put(pair.first, hex_stream.str());
    hex_stream.str("");   // clear the stream
  }
  try {
    boost::property_tree::write_json(filepath, pt);
  } catch (const std::exception& ex) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "Could not write JSON to disk: " + std::string(ex.what()));
    ers::error(issue);
  }
}

} // namespace nsw