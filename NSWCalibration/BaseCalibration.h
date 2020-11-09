#ifndef BASE_CALIBRATION_HPP
#define BESE_CALIBRATION_HPP

#include <string>
#include <vector>
#include <array>

#include "NSWConfiguration/FEBConfig.h"


using ValueMap = std::map<std::string, std::map<std::string, std::vector<std::string>>>;


template<typename Specialized>
class BaseCalibration
{
public:
    BaseCalibration(nsw::FEBConfig t_config);
    [[nodiscard]] static nsw::FEBConfig adaptConfig(const nsw::FEBConfig& t_config, const ValueMap t_vals, int i);
    void basicConfigure(nsw::FEBConfig t_config) const;
    [[nodiscard]] std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> checkVmmCaptureRegisters(const nsw::FEBConfig& t_config) const;
    void saveResult(const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>& t_result, std::ofstream &t_filestream, int i) const;
    [[nodiscard]] int analyzeResults(const std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>>& t_results) const;
    void run(const bool t_dryRun, const std::string& t_outputFilename) const;

private:
    nsw::FEBConfig m_config;
    Specialized m_specialized;
};


#endif