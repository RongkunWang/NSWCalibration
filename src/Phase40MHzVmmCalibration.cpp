#include <fstream>

#include "NSWCalibration/Phase40MHzVmmCalibration.h"
#include "NSWCalibration/BaseCalibration.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ConfigConverter.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/I2cMasterConfig.h"

Phase40MHzVmmCalibration::Phase40MHzVmmCalibration(nsw::FEBConfig t_config, const std::vector<int>& t_input={}) : m_config(t_config),
                                                                                                                  m_inputValues(getInputVals(t_input))
{
}

[[nodiscard]] ValueMap Phase40MHzVmmCalibration::getInputVals(const std::vector<int>& t_input) const
{

    const auto valsePllPhase40MHz = [&t_input]() {
        if (t_input.empty())
        {
            // 40MHz clock has 128 possible different values
            const int nEntries{128};

            // 40MHz has values 0 - 127
            const int clock40MHzStart{0};

            auto vec = std::vector<int>(nEntries);

            std::generate(vec.begin(), vec.end(), [i = clock40MHzStart]() mutable { return i++; });

            return vec;
        }
        else
        {
            return t_input;
        }
    }();

    for (const auto val : valsePllPhase40MHz)
    {
        std::cout << val << ' ';
    }
    std::cout << '\n';

    return {{"ePllVmm0.ePllPhase40MHz_0", valsePllPhase40MHz},
            {"ePllVmm0.ePllPhase40MHz_1", valsePllPhase40MHz},
            {"ePllVmm0.ePllPhase40MHz_2", valsePllPhase40MHz},
            {"ePllVmm0.ePllPhase40MHz_3", valsePllPhase40MHz},
            {"ePllVmm1.ePllPhase40MHz_0", valsePllPhase40MHz},
            {"ePllVmm1.ePllPhase40MHz_1", valsePllPhase40MHz},
            {"ePllVmm1.ePllPhase40MHz_2", valsePllPhase40MHz},
            {"ePllVmm1.ePllPhase40MHz_3", valsePllPhase40MHz}};
}

void Phase40MHzVmmCalibration::setRegisters(const int t_iteration) const
{
    nsw::ConfigSender configSender;
    const auto ptree = BaseCalibration<Phase40MHzVmmCalibration>::createPtree(m_inputValues, t_iteration);
    const auto configConverter = ConfigConverter(ptree, ConfigConverter::RegisterAddressSpace::ROC_ANALOG, ConfigConverter::ConfigType::VALUE_BASED);
    auto translatedPtree = configConverter.getRegisterBasedConfigWithoutSubregisters(m_config.getRocAnalog());
    const auto partialConfig = nsw::I2cMasterConfig(translatedPtree, ROC_ANALOG_NAME, ROC_ANALOG_REGISTERS, true);
    const auto opcIp = m_config.getOpcServerIp();
    const auto scaAddress = m_config.getAddress();

    BaseCalibration<Phase40MHzVmmCalibration>::basicConfigure(m_config);
    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 0);
    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocPllResetN", 0);
    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocSResetN", 0);

    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocSResetN", 1);

    configSender.sendI2cMasterConfig(opcIp, scaAddress, partialConfig);
    //for (const auto& entry : m_inputValues)
    //{
    //    std::cout << "SET REG " << entry.first << std::endl;
    //    configSender.sendI2cMasterSingle(opcIp, scaAddress, analog, entry.first);
    //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //}

    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocPllResetN", 1);

    bool roc_locked = 0;
    while (!roc_locked) {
        bool rPll1 = configSender.readGPIO(opcIp, scaAddress + ".gpio.rocPllLocked");
        bool rPll2 = configSender.readGPIO(opcIp, scaAddress + ".gpio.rocPllRocLocked");
        roc_locked = rPll1 & rPll2;
    }

    configSender.sendGPIO(opcIp, scaAddress + ".gpio.rocCoreResetN", 1);
    //configSender.sendConfig(adaptedConfig);
}

void Phase40MHzVmmCalibration::saveBestSettings(const int t_bestIteration, const std::string &t_filename) const
{
    const auto bestPhase40MHz = m_inputValues.at("ePllVmm0.ePllPhase40MHz_0").at(t_bestIteration);
    std::cout << "Best values (iteration) " << t_bestIteration << '\n'
              << "\t40MHz: " << bestPhase40MHz << '\n';
    std::ofstream outfile;
    outfile.open(t_filename);
    for (const auto& [registerName, values] : m_inputValues)
    {
        outfile << "rocCoreAnalog." << registerName << ':' << values[t_bestIteration] << '\n';
    }
    outfile.close();
}

std::size_t Phase40MHzVmmCalibration::getNumberOfConfigurations() const
{
    return m_inputValues.begin()->second.size();
}

int Phase40MHzVmmCalibration::getValueOfIteration(const int t_iteration) const
{
    return m_inputValues.at("ePllVmm0.ePllPhase40MHz_0").at(t_iteration);
}
