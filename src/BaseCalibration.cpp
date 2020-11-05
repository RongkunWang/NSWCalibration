#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <exception>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"

#include "NSWCalibration/BaseCalibration.h"
#include "NSWCalibration/Phase160MHzCalibration.h"

template<typename Specialized>
BaseCalibration<Specialized>::BaseCalibration(nsw::FEBConfig t_config) : m_config(t_config),
                                                            m_specialized(t_config)
{
}

template<typename Specialized>
[[nodiscard]] nsw::FEBConfig BaseCalibration<Specialized>::adaptConfig(const nsw::FEBConfig &t_config, const ValueMap t_vals, int i)
{
    // creates a copy...
    auto config = t_config.getConfig();

    // loop over registers
    for (const auto &[registerName, settings] : t_vals)
    {
        // loop over names in registers
        for (const auto &[key, values] : settings)
        {
            // Set value TODO: digital?
            const std::string analogName{"rocPllCoreAnalog"};
            const std::string settingName{analogName + '.' + registerName + '.' + key};
            // Check if value exists
            try
            {
                config.get<std::string>(settingName);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Did not find key " << settingName << "in config\n";
                throw;
            }

            config.put(settingName, values[i]);
        }
    }

    return nsw::FEBConfig{config};
}

template<typename Specialized>
void BaseCalibration<Specialized>::basicConfigure(nsw::FEBConfig t_config) const
{
    nsw::ConfigSender configSender;
    configSender.sendConfig(t_config);
}

template<typename Specialized>
[[nodiscard]] std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> BaseCalibration<Specialized>::checkVmmCaptureRegisters(const nsw::FEBConfig &t_config) const
{
    std::array<uint8_t, 8> result;
    std::array<uint8_t, 8> resultParity;
    const auto opcIp = t_config.getOpcServerIp();
    // TODO: magic numbers
    const int vmmCaptureAddressInitial = 32;
    const int vmmParityCounterAddressInitial = 45;
    nsw::ConfigSender configSender;
    for (int dummy = 0; dummy < 2; dummy++)
    {
        for (int vmmId = 0; vmmId <= 7; vmmId++)
        {
            // see NSWConfiguration/app/vmm_capture_status.cpp
            const auto vmmCaptureStatus = configSender.readBackRoc(opcIp,
                                                                   t_config.getAddress() + ".gpio.bitBanger",
                                                                   17,                                                     // scl line
                                                                   18,                                                     // sda line
                                                                   static_cast<uint8_t>(vmmCaptureAddressInitial + vmmId), // reg number
                                                                   2);                                                     // delay
            result[vmmId] = vmmCaptureStatus;

            const auto parityCounter = configSender.readBackRoc(opcIp,
                                                                t_config.getAddress() + ".gpio.bitBanger",
                                                                17,                                                           // scl line
                                                                18,                                                           // sda line
                                                                static_cast<uint8_t>(vmmParityCounterAddressInitial + vmmId), // reg number
                                                                2);                                                           // delay
            resultParity[vmmId] = parityCounter;
        }
    }

    return {result, resultParity};
}

template<typename Specialized>
void BaseCalibration<Specialized>::saveResult(const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> &t_result, std::ofstream &t_filestream, int i) const
{
    const auto status = t_result.first;
    const auto parity = t_result.second;
    for (std::size_t vmmId = 0; vmmId < status.size(); vmmId++)
    {
        // Check registers (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
        const auto fifo_bit{0b0001'0000};
        const auto coherency_bit{0b0000'1000};
        const auto decoder_bit{0b0000'0100};
        const auto misalignment_bit{0b000'0010};
        const auto alignment_bit{0b0000'0001};
        const auto failed_fifo = status[vmmId] & fifo_bit;
        const auto failed_coherency = status[vmmId] & coherency_bit;
        const auto failed_decoder = status[vmmId] & decoder_bit;
        const auto failed_misalignment = status[vmmId] & misalignment_bit;
        const auto failed_alignment = status[vmmId] & alignment_bit;
        const auto failed_parity = parity[vmmId] > 0;
        t_filestream << i << ' ' << vmmId << ' ' << failed_fifo << ' ' << failed_coherency << ' '
                     << failed_decoder << ' ' << failed_misalignment << ' ' << failed_alignment << ' ' << failed_parity << '\n';
    }
}

template<typename Specialized>
[[nodiscard]] int BaseCalibration<Specialized>::analyzeResults(const std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>> &t_results) const
{
    std::vector<bool> testResults;
    testResults.reserve(t_results.size());
    std::transform(std::begin(t_results), std::end(t_results), std::back_inserter(testResults),
                   [](const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> &t_result) {
                       const auto status{t_result.first};
                       const auto parity{t_result.second};
                       const auto statusOk = std::all_of(std::begin(status), std::end(status),
                                                         [noError = 1](const auto t_val) { return t_val == noError; });  // no error: 0000 0001 = 1
                       const auto parityOk = std::all_of(std::begin(parity), std::end(parity),
                                                         [](const auto t_val) { return t_val == 0; });
                       return statusOk and parityOk;
                   });

    const auto index = std::distance(std::begin(testResults), std::find_if(std::begin(testResults), std::end(testResults),
                                                                           [firstElement = testResults.at(0)](const bool val) {
                                                                               return val != firstElement;
                                                                           }));

    for (const auto &el : testResults)
    {
        std::cout << el << " ";
    }
    std::cout << std::endl;

    // wrap around....
    std::rotate(std::begin(testResults), std::begin(testResults) + index, std::end(testResults));

    int counterGood = 0;
    int maxCounterGood = 0;
    int endGoodRegion = 0;
    for (std::size_t i = 0; i < testResults.size(); i++)
    {
        const auto &testResult = testResults[i];
        if (testResult)
        {
            counterGood++;
        }
        if (not testResult or i == testResults.size() - 1)
        {
            if (counterGood > maxCounterGood)
            {
                maxCounterGood = counterGood;
                endGoodRegion = i - 1;
            }
            counterGood = 0;
        }
    }

    // This definetely does not need any explanantion
    return (endGoodRegion - maxCounterGood / 2 + index) % testResults.size();
}

template<typename Specialized>
void BaseCalibration<Specialized>::run(const bool t_dryRun, const std::string& t_outputFilename) const
{
    std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>> allResults;

    // configure the whole board at the start
    if (t_dryRun)
    {
        return;
    }

    basicConfigure(m_config);

    std::ofstream outfile;
    // add _full before .*
    outfile.open(t_outputFilename.substr(0, t_outputFilename.find('.')) + "_full" + t_outputFilename.substr(t_outputFilename.find('.')));

    // iterate through settings (vector in map of map)
    for (std::size_t counter = 0; counter < m_specialized.getNumberOfConfigurations(); counter++)
    {
        m_specialized.setRegisters(counter);

        // check the result
        const auto result = checkVmmCaptureRegisters(m_config);
        allResults.push_back(result);

        // save
        saveResult(result. outfile, counter);
    }
    outfile.close();

    const auto bestIteration = analyzeResults(allResults);
    m_specialized.saveBestSettings(bestIteration, t_outputFilename);
}

// instantiate templates
template class BaseCalibration<Phase160MHzCalibration>;