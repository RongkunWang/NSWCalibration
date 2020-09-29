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
  // Read SCAIDs from each board
  std::unordered_map<std::string, OpcClient> clients;
  for (const auto& pair : m_boards) {
    const auto& opc_ip = pair.second.getOpcServerIp();
    if (clients.find(opc_ip) == clients.end()) {
      // Create new pairing: OPC server IP <-> OPC client (note the `emplace`)
      clients.emplace(opc_ip, opc_ip);
    }
    auto id = clients.at(opc_ip).readScaID(pair.second.getAddress());
    m_ids.emplace(pair.first, id);
  }
}

void ScaIdCalib::unconfigure() {
  // Possibly put logic for storing IDs to disk here
}

} // namespace nsw