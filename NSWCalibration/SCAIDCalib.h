#ifndef SCAIDCALIB_H_
#define SCAIDCALIB_H_

/*
 * A CalibAlg for gathering SCA IDs and storing them in an output JSON with the corresponding board names.
 * Optionally, also for comparing them with a known boardname <-> SCA ID table and checking for mismatches (WIP).
 * Author: Yuval Zach <yuval.zach@cern.ch> at WIS
*/

#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "boost/property_tree/ptree.hpp"

#include "ers/ers.h"

#include "NSWConfiguration/SCAConfig.h"
#include "NSWConfiguration/OpcClient.h"
#include "NSWConfiguration/ConfigReader.h"

#include "NSWCalibration/CalibAlg.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWSCAIDCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class ScaIdCalib: public CalibAlg {

  public:
    ScaIdCalib(const std::string& sca_table_path);
    ~ScaIdCalib() {};
    
    // Reads all boards which contain an SCA chip from the configuration database `db`
    void setup(std::string db) override;

    // Reads SCA IDs from given boards and stores to the JSON file `sca_ids.json`
    // TODO: Additionally compares SCA IDs and their supposed location with same data given
    // in the <config_dir>/sca_ids.json file.
    void configure() override;

    void unconfigure() override { }
  private:
    std::string m_calib_type;
    std::string m_sca_table_path;

    // K: Logical ID, V: SCA ID
    std::unordered_map<std::string, unsigned int> m_static_ids;
    // <name, SCAID>
    std::unordered_map<std::string, unsigned int> m_queried_ids;

    std::vector<nsw::SCAConfig> m_boards;

    // Fetches SCA IDs from all boards defined in m_boards
    void fetch_sca_ids();

    // Fetches SCA IDs from all boards defined in m_boards, and compares them against the SCA IDs
    // found in m_static_ids.
    void check_sca_ids() const;
    
    // Stores all boards and their SCA IDs in a json file given by `filepath`
    void write_sca_ids(const std::string& filepath) const;
  };

} // namespace nsw
#endif // SCAIDCALIB_H_