#ifndef PHASE_40MHZ_VMM_CALIBRATION_HPP
#define PHASE_40MHZ_VMM_CALIBRATION_HPP

#include <map>
#include <string>
#include <vector>

#include "NSWConfiguration/FEBConfig.h"


using ValueMap = std::map<std::string, std::vector<int>>;


class Phase40MHzVmmCalibration
{
public:
    Phase40MHzVmmCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_input);
    [[nodiscard]] ValueMap getInputVals(const std::vector<int>& t_input) const;
    void setRegisters(const int t_iteration) const;
    void saveBestSettings(const int t_bestIteration, const std::string& t_filename) const;
    std::size_t getNumberOfConfigurations() const;
    int getValueOfIteration(const int t_iteration) const;

private:
    nsw::FEBConfig m_config;
    ValueMap m_inputValues{};
};

#endif
