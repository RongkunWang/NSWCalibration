#ifndef PHASE_160MHZ_CALIBRATION_HPP
#define PHASE_160MHZ_CALIBRATION_HPP

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/FEBConfig.h"


using ValueMap = std::map<std::string, std::map<std::string, std::vector<std::string>>>;


// struct holding best settings
struct Settings
{
    std::string ePllPhase40MHz{""};
    std::string ePllPhase160MHz_3_0{""};
    std::string ePllPhase160MHz_4{""};
};


class Phase160MHzCalibration
{
public:
    Phase160MHzCalibration(nsw::FEBConfig t_config);
    [[nodiscard]] ValueMap getInputVals(nsw::FEBConfig t_config) const;
    void setRegisters(const int i) const;
    Settings getBestSettings(const ValueMap& t_inputValues, const int t_bestIteration) const;
    int getNumberOfConfigurations() const;

private:
    nsw::FEBConfig m_config;
    ValueMap m_inputValues{};
};

#endif