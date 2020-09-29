#ifndef SCAIDCALIB_H_
#define SCAIDCALIB_H_

#include <unordered_map>
#include <vector>

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
    ScaIdCalib(const std::string& calib_type);
    ~ScaIdCalib() {};
    void setup(std::string db) override;
    void configure() override;
    void unconfigure() override;
  private:
    std::string m_calib_type;
    // <name, SCAConfig>
    std::unordered_map<std::string, nsw::SCAConfig> m_boards;
    // <name, SCAID>
    std::unordered_map<std::string, unsigned int> m_ids;
  };

} // namespace nsw
#endif // SCAIDCALIB_H_