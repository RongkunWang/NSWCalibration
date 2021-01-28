#ifndef BASE_CALIBRATION_HPP
#define BESE_CALIBRATION_HPP

#include <string>
#include <vector>
#include <array>

#include "boost/property_tree/ptree.hpp"

#include "NSWConfiguration/FEBConfig.h"


using ValueMap = std::map<std::string, std::vector<int>>;


struct StatusRegisters
{
    std::array<uint8_t, 8> m_vmmStatus;
    std::array<uint8_t, 8> m_vmmParity;
    std::array<uint8_t, 4> m_srocStatus;
};


template<typename Specialized>
class BaseCalibration
{
public:
    BaseCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_values);
    static void basicConfigure(const nsw::FEBConfig& t_config);
    [[nodiscard]] static StatusRegisters checkVmmCaptureRegisters(const nsw::FEBConfig& t_config);
    void saveResult(const StatusRegisters& t_result, std::ofstream &t_filestream, const int t_iteration) const;
    [[nodiscard]] int analyzeResults(const std::vector<StatusRegisters>& t_results) const;
    void run(const bool t_dryRun, const std::string& t_outputFilename) const;
    [[nodiscard]] static ptree createPtree(const ValueMap& t_inputValues, const int t_iteration);
    static void commandAlti(const std::string& t_command);
    static void startAlti();
    static void stopAlti();

private:
    nsw::FEBConfig m_config;
    Specialized m_specialized;
};


#endif
