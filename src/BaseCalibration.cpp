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
#include "NSWCalibration/Phase160MHzVmmCalibration.h"
#include "NSWCalibration/Phase40MHzVmmCalibration.h"

#include "RunControl/Common/OnlineServices.h"
#include "RunControl/RunControl.h"
#include "RunControl/Common/RunControlCommands.h"
#include "ipc/core.h"

template <typename Specialized>
BaseCalibration<Specialized>::BaseCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_values) : m_config(t_config),
                                                                                                           m_specialized(t_config, t_values)
{
}

template <typename Specialized>
void BaseCalibration<Specialized>::basicConfigure(const nsw::FEBConfig& t_config)
{
    nsw::ConfigSender configSender;
    configSender.sendConfig(t_config);
}

template <typename Specialized>
[[nodiscard]] StatusRegisters BaseCalibration<Specialized>::checkVmmCaptureRegisters(const nsw::FEBConfig &t_config)
{
    std::array<uint8_t, 8> vmmStatus;
    std::array<uint8_t, 8> vmmParity;
    std::array<uint8_t, 4> srocStatus;
    const auto opcIp = t_config.getOpcServerIp();
    // TODO: magic numbers
    const int vmmCaptureAddressInitial = 32;
    const int vmmParityCounterAddressInitial = 45;
    const int srocStatusAddressInitial = 40;
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
            vmmStatus[vmmId] = vmmCaptureStatus;

            const auto parityCounter = configSender.readBackRoc(opcIp,
                                                                t_config.getAddress() + ".gpio.bitBanger",
                                                                17,                                                           // scl line
                                                                18,                                                           // sda line
                                                                static_cast<uint8_t>(vmmParityCounterAddressInitial + vmmId), // reg number
                                                                2);                                                           // delay
            vmmParity[vmmId] = parityCounter;
            if (dummy == 1)
            {
                std::cout << " VMM ID " << vmmId << " 0x" << std::hex << static_cast<unsigned int>(vmmCaptureStatus) << " 0x" << static_cast<unsigned int>(parityCounter) << std::dec << '\n';
            }

        }
        for (int srocId = 0; srocId <= 3; srocId++)
        {
            const auto srocStatusRegister = configSender.readBackRoc(opcIp,
                                                                     t_config.getAddress() + ".gpio.bitBanger",
                                                                     17,                                                           // scl line
                                                                     18,                                                           // sda line
                                                                     static_cast<uint8_t>(srocStatusAddressInitial + srocId),      // reg number
                                                                     2);                                                           // delay
            srocStatus[srocId] = srocStatusRegister;
            if (dummy == 1)
            {
                std::cout << "sROC ID " << srocId << " 0x" << std::hex << static_cast<unsigned int>(srocStatusRegister) << std::dec << '\n';
            }
        }

    }

    return {vmmStatus, vmmParity, srocStatus};
}

template <typename Specialized>
void BaseCalibration<Specialized>::saveResult(const StatusRegisters &t_result, std::ofstream &t_filestream, const int t_iteration) const
{
    const auto vmmStatus = t_result.m_vmmStatus;
    const auto parity = t_result.m_vmmParity;
    const auto srocStatus = t_result.m_srocStatus;
    const auto setting = m_specialized.getValueOfIteration(t_iteration);
    // Check registers (https://espace.cern.ch/ATLAS-NSW-ELX/_layouts/15/WopiFrame.aspx?sourcedoc=/ATLAS-NSW-ELX/Shared%20Documents/ROC/ROC_Reg_digital_analog_combined_annotated.xlsx&action=default)
    for (std::size_t vmmId = 0; vmmId < vmmStatus.size(); vmmId++)
    {
        const auto fifoBit{0b0001'0000};
        const auto coherencyBit{0b0000'1000};
        const auto decoderBit{0b0000'0100};
        const auto misalignmentBit{0b000'0010};
        const auto alignmentBit{0b0000'0001};
        const auto failedFifo = static_cast<bool>(vmmStatus[vmmId] & fifoBit);
        const auto failedCoherency = static_cast<bool>(vmmStatus[vmmId] & coherencyBit);
        const auto failedDecoder = static_cast<bool>(vmmStatus[vmmId] & decoderBit);
        const auto failedMisalignment = static_cast<bool>(vmmStatus[vmmId] & misalignmentBit);
        const auto failedAlignment = static_cast<bool>(not(vmmStatus[vmmId] & alignmentBit));
        const auto failedParity = static_cast<bool>(parity[vmmId] > 0);
        const auto twofiftyfive = vmmStatus[vmmId] == 255;
        t_filestream << setting << ' ' << vmmId << ' ' << failedFifo << ' ' << failedCoherency << ' '
                     << failedDecoder << ' ' << failedMisalignment << ' ' << failedAlignment << ' ' << failedParity << ' ' << twofiftyfive << " 0x" << std::hex << static_cast<unsigned int>(vmmStatus[vmmId]) << std::dec << '\n';
    }
    for (std::size_t srocId = 0; srocId < srocStatus.size(); srocId++)
    {
        const auto ttcFifoBit{0b0000'0100};
        const auto encoderBit{0b000'0010};
        const auto eventFullBit{0b0000'0001};
        const auto failedFifo = static_cast<bool>(srocStatus[srocId] & ttcFifoBit);
        const auto failedEncoder = static_cast<bool>(srocStatus[srocId] & encoderBit);
        const auto failedEventFull = static_cast<bool>(srocStatus[srocId] & eventFullBit);
        const auto twofiftyfive = srocStatus[srocId] == 255;
        t_filestream << setting << ' ' << srocId << ' ' << failedFifo << ' ' << failedEncoder << ' ' << failedEventFull << ' ' << twofiftyfive << " 0x" << std::hex << static_cast<unsigned int>(srocStatus[srocId]) << std::dec << '\n';
    }
}

template <typename Specialized>
[[nodiscard]] int BaseCalibration<Specialized>::analyzeResults(const std::vector<StatusRegisters> &t_results) const
{
    std::vector<bool> testResults;
    testResults.reserve(t_results.size());
    std::transform(std::begin(t_results), std::end(t_results), std::back_inserter(testResults),
                   [](const StatusRegisters &t_result) {
                       const auto status{t_result.m_vmmStatus};
                       const auto parity{t_result.m_vmmParity};
                       const auto statusOk = std::all_of(std::begin(status), std::end(status),
                                                         [noError = 1](const auto t_val) { return t_val == noError; }); // no error: 0000 0001 = 1
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
                if (i != testResults.size() - 1)
                {
                    endGoodRegion -= 1;
                }
            }
            counterGood = 0;
        }
    }

    if (maxCounterGood == 0)
    {
        std::cout << "ERROR: No good setting found!\n";
        // TODO: ERS Logging
    }

    // This definetely does not need any explanantion
    return (endGoodRegion - maxCounterGood / 2 + index) % testResults.size();
}

template <typename Specialized>
void BaseCalibration<Specialized>::run(const bool t_dryRun, const std::string &t_outputFilename) const
{
    std::vector<StatusRegisters> allResults;

    // configure the whole board at the start
    if (t_dryRun)
    {
        return;
    }

    //basicConfigure(m_config);

    std::ofstream outfile;
    // add _full before .*
    outfile.open(t_outputFilename.substr(0, t_outputFilename.find('.')) + "_full" + t_outputFilename.substr(t_outputFilename.find('.')));

    //// init ipc core for alti control
    char* argv[7] = {"/afs/cern.ch/work/n/nswdaq/public/nswdaq/tdaq-09-02-01/nswdaq/installed/x86_64-centos7-gcc8-opt/bin/NSWConfigRc_main", "-n", "VS-Config", "-P", "VerticalSliceTests", "-s" ,"VerticalSliceTests"};
    int nargs = 7;
    IPCCore::init(nargs, argv);
    BaseCalibration<Phase160MHzCalibration>::stopAlti();

    // iterate through settings (vector in map of map)
    for (std::size_t counter = 0; counter < m_specialized.getNumberOfConfigurations(); counter++)
    {
        try
        {
            m_specialized.setRegisters(counter);

            BaseCalibration<Phase160MHzCalibration>::startAlti();
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // check the result
            const auto result = checkVmmCaptureRegisters(m_config);
            allResults.push_back(result);
            BaseCalibration<Phase160MHzCalibration>::stopAlti();
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // save
            saveResult(result, outfile, counter);
        }
        catch (const std::exception& ex)
        {
            std::cout << ex.what() << '\n';
            break;
        }
    }
    outfile.close();

    const auto bestIteration = analyzeResults(allResults);
    m_specialized.saveBestSettings(bestIteration, t_outputFilename);
}

template <typename Specialized>
ptree BaseCalibration<Specialized>::createPtree(const ValueMap& t_inputValues, const int t_iteration)
{
    ptree tree;
    std::for_each(std::begin(t_inputValues), std::end(t_inputValues), [&tree, t_iteration] (const auto& t_pair) {
        const auto& name = t_pair.first;
        const auto value = std::to_string(t_pair.second[t_iteration]);
        tree.put(name, value);
    });
    return tree;
}

template <typename Specialized>
void BaseCalibration<Specialized>::commandAlti(const std::string& t_command)
{
    const std::string app_name = "Alti_RCD";
    //const std::string partition_name = "part-BB5-Rocphase";
    //const std::string partition_name = "part-VS-stgc-rocphase";
    const std::string partition_name = "part-VS-1MHzTest";
    const daq::rc::UserCmd cmd(t_command, std::vector<std::string>());
    daq::rc::CommandSender sendr(partition_name, "NSWCalibRcSender");
    sendr.sendCommand(app_name, cmd);
}

template <typename Specialized>
void BaseCalibration<Specialized>::startAlti()
{
    commandAlti("StartPatternGenerator");
}

template <typename Specialized>
void BaseCalibration<Specialized>::stopAlti()
{
    commandAlti("StopPatternGenerator");
}

// instantiate templates
template class BaseCalibration<Phase160MHzCalibration>;
template class BaseCalibration<Phase160MHzVmmCalibration>;
template class BaseCalibration<Phase40MHzVmmCalibration>;
