#include "NSWCalibration/SCAIDCalib.h"

namespace nsw {

ScaIdCalib::ScaIdCalib(const std::string& sca_table_path) :
  m_sca_table_path(sca_table_path) {
  setTotal(1);
  setCounter(-1); // required for running once: next() is called at the beginning of the handler loop
  setToggle(false);
}

void ScaIdCalib::setup(std::string db) {
  // Get the SCAID <-> Physical location table, parse into in-memory data structure...
  boost::property_tree::ptree sca_json;
  try {
    boost::property_tree::read_json(m_sca_table_path, sca_json);
  } catch (const std::exception& ex) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "Could not read SCA ID table: " + std::string(ex.what()));
    ers::fatal(issue);
  }
  for (const auto& pair : sca_json) {
    m_static_ids.emplace(pair.first, pair.second.get_value<unsigned int>());
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
      m_boards.emplace_back(reader.readConfig(name));
  }
}

void ScaIdCalib::configure() {
  fetch_sca_ids();
  // Not in production!
  write_sca_ids("sca_ids.json");
  check_sca_ids();
}

void ScaIdCalib::fetch_sca_ids() {
  // Read SCA IDs from each board
  std::unordered_map<std::string, OpcClient> clients;
  
  std::mutex clients_mutex;
  std::vector<std::future<std::pair<std::string, unsigned int>>> runners;

  auto get_id = [&](const SCAConfig& board) -> std::pair<std::string, unsigned int> {
    const auto& opc_ip = board.getOpcServerIp();
    // If we don't have an OpcClient already connected to this server, create one
    if (clients.find(opc_ip) == clients.end()) {
      // unordered_map writes NOT thread safe by default
      std::lock_guard<std::mutex> guard(clients_mutex);
      try {
        clients.emplace(opc_ip, opc_ip);
      } catch (const std::exception& ex) {
        throw std::runtime_error("Could not connect to OPC server " + board.getOpcServerIp() + ": " + ex.what());
      }
    }
    // Map reads are thread-safe
    try {
      // Must be const, as to avoid data races in OpcClient
      const auto& opc_client = clients.at(opc_ip);
      return std::make_pair(board.getAddress(), opc_client.readScaID(board.getAddress()));
    } catch (const std::exception& ex) {
        throw std::runtime_error("Could not read SCA ID of board at address " + board.getAddress() + ": " + ex.what());
    }
  };
  // Launch runners
  for (const auto& board : m_boards) {
    runners.push_back(std::async(std::launch::async, get_id, board));
  }
  for (auto& runner : runners) {
    try {
      m_queried_ids.insert(runner.get());
    } catch (const std::exception& ex) {
      NSWSCAIDCalibIssue issue(ERS_HERE, ex.what());
      ers::error(issue);
    }
  }
}

void ScaIdCalib::write_sca_ids(const std::string& filepath) const {
  if (m_queried_ids.empty()) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "No SCA IDs found, writing empty list to disk anyway");
    ers::warning(issue);
  }

  boost::property_tree::ptree pt;
  for (const auto& pair : m_queried_ids) {
    // SCA IDs are 24 bits (6 hex digits) long
    std::stringstream hex_stream;
    hex_stream << "0x" << std::setfill('0') << std::setw(8) << std::hex << pair.second;
    pt.put(pair.first, hex_stream.str());
  }

  try {
    boost::property_tree::write_json(filepath, pt);
  } catch (const std::exception& ex) {
    NSWSCAIDCalibIssue issue(ERS_HERE, "Could not write JSON to disk: " + std::string(ex.what()));
    ers::error(issue);
  }
}

void ScaIdCalib::check_sca_ids() const {
  if (m_static_ids.size() != m_queried_ids.size()) {
    std::string warning = "Table size doesn't match queried IDs count, table size: " + std::to_string(m_static_ids.size())
      + ", queried IDs count: " + std::to_string(m_queried_ids.size());
    NSWSCAIDCalibIssue issue(ERS_HERE, warning);
    ers::warning(issue);
  }
  for (const auto& static_pair : m_static_ids) {
    try {
      const auto& queried_id = m_queried_ids.at(static_pair.first);
      if (queried_id != static_pair.second) {
        std::string warning = "SCA ID mismatch at " + static_pair.first + ". Static: "
          + std::to_string(static_pair.second) + ", queried: " + std::to_string(queried_id);
        NSWSCAIDCalibIssue issue(ERS_HERE, warning);
        ers::warning(issue);
      }
    } catch (const std::exception& ex) {
      // SCA ID in table doesn't exist in queried IDs
      NSWSCAIDCalibIssue issue(ERS_HERE, "Could not find matching logical ID in queried ID table: "
        + static_pair.first + ", skipping");
      ers::warning(issue);
    }
  }
}
} // namespace nsw