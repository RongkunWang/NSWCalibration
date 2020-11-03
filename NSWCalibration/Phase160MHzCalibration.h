#ifndef PHASE_160MHZ_CALIBRATION_HPP
#define PHASE_160MHZ_CALIBRATION_HPP

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/FEBConfig.h"


using ValueMap = std::map<std::string, std::map<std::string, std::vector<std::string>>>;


class Phase160MHzCalibration
{
public:
    Phase160MHzCalibration(nsw::FEBConfig t_config);
    [[nodiscard]] ValueMap getInputVals(nsw::FEBConfig t_config) const;
    void setRegisters(const int i) const;
    void saveBestSettings(const int t_bestIteration, const std::string& t_filename) const;
    std::size_t getNumberOfConfigurations() const;

private:
    nsw::FEBConfig m_config;
    ValueMap m_inputValues{};
};

#endif