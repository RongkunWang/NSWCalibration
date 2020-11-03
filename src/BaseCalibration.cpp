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

#include "BaseCalibration.h"

BaseCalibration::BaseCalibration(nsw::FEBConfig t_config) : m_config(t_config),
                                                            m_specialized(t_config)
{
}

[[nodiscard]] nsw::FEBConfig adaptConfig(const nsw::FEBConfig &t_config, const ValueMap t_vals, int i)
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

void BaseCalibration::basicConfigure(nsw::FEBConfig t_config) const
{
    nsw::ConfigSender configSender;
    configSender.sendConfig(t_config);
}

[[nodiscard]] std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> BaseCalibration::checkVmmCaptureRegisters(const nsw::FEBConfig &t_config) const
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

void BaseCalibration::printResult(const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> &t_result, int i) const
{
    std::ofstream outfile;
    outfile.open("log_myreadout.txt", std::ios_base::app);
    outfile << "Iteration: " << i << '\n';
    const auto status = t_result.first;
    const auto parity = t_result.second;
    for (int vmmId = 0; vmmId < status.size(); vmmId++)
    {
        outfile << "VMM Status/Parity" << vmmId << " : " << unsigned(status[vmmId]) << '/' << unsigned(parity[vmmId]) << " (";

        // Check registers (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
        const auto fifo_bit{0b0001'0000};
        const auto coherency_bit{0b0000'1000};
        const auto decoder_bit{0b0000'0100};
        const auto misalignment_bit{0b000'0010};
        const auto alignment_bit{0b0000'0001};
        if (status[vmmId] & fifo_bit)
        {
            outfile << "FIFO full error, ";
        }
        if (status[vmmId] & coherency_bit)
        {
            outfile << "Coherency error, ";
        }
        if (status[vmmId] & decoder_bit)
        {
            outfile << "Decoder error, ";
        }
        if (status[vmmId] & misalignment_bit)
        {
            outfile << "Misalignment error, ";
        }
        if (not(status[vmmId] & alignment_bit))
        {
            outfile << "VMM not aligned, ";
        }
        if (parity[vmmId] > 0)
        {
            outfile << "Parity counter error = " << unsigned(parity[vmmId]);
        }
        outfile << ")\n";
    }
    outfile << '\n';
    outfile.close();
}

int BaseCalibration::analyzeResults(const std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>> &t_results) const
{
    std::vector<bool> testResults;
    testResults.reserve(t_results.size());
    std::transform(std::begin(t_results), std::end(t_results), std::back_inserter(testResults),
                   [](const std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>> &t_result) {
                       const auto status{t_result.first};
                       const auto parity{t_result.second};
                       const auto statusOk = std::all_of(std::begin(status), std::end(status),
                                                         [noError = 0b0000'0001](const auto t_val) { return t_val & noError; });
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
                endGoodRegion = i;
            }
            counterGood = 0;
        }
    }

    // This definetely does not need any explanantion
    return (endGoodRegion - maxCounterGood / 2 + index) % testResults.size();
}

void BaseCalibration::run(const bool dryRun) const
{
    std::vector<std::pair<std::array<uint8_t, 8>, std::array<uint8_t, 8>>> allResults;

    // configure the whole board at the start
    if (not dryRun)
    {
        basicConfigure(m_config);
    }

    // iterate through settings (vector in map of map)
    for (std::size_t counter = 0; counter < t_specialized.getNumberOfConfigurations; counter++)
    {
        t_specialized.setRegisters(counter);

        // check the result
        const auto result = checkVmmCaptureRegisters(m_config);
        allResults.push_back(result);

        // print
        printResult(result, counter);
    }
    const auto bestIteration = analyzeResults(allResults);
    const auto bestSettings = t_specialized.getBestSettings(inputVals, bestIteration);
    std::cout << "Best values (iteration) " << bestIteration << '\n'
              << "\t40MHz: " << bestSettings.ePllPhase40MHz << '\n'
              << "\t160MHz[3:0]: " << bestSettings.ePllPhase160MHz_3_0 << '\n'
              << "\t160MHz[4]: " << bestSettings.ePllPhase160MHz_4 << '\n';
}