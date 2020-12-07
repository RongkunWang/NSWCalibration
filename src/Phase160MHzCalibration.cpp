#include <fstream>

#include "NSWCalibration/Phase160MHzCalibration.h"
#include "NSWCalibration/BaseCalibration.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/ConfigTranslation.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/I2cMasterConfig.h"

Phase160MHzCalibration::Phase160MHzCalibration(nsw::FEBConfig t_config) : m_config(t_config),
                                                                          m_inputValues(getInputVals(t_config))
{
}

[[nodiscard]] ValueMap Phase160MHzCalibration::getInputVals(nsw::FEBConfig t_config) const
{
    auto config = t_config.getConfig();
    const auto analog = config.get_child("rocPllCoreAnalog");

    // 160MHz clock has 32 possible different values
    const int nEntries{32};

    const auto valsePllPhase40MHz = [nEntries, &analog]() {
        // 40MHz has values 112 - 127 (two times)
        const auto clock40MHzDefault = analog.get<int>("reg115.ePllPhase40MHz_0");
        const int clock40MHzStart{clock40MHzDefault & 0b1111'0000}; // xxxx 0000 (set last 4 bits to 0)

        auto vec = std::vector<std::string>(nEntries);

        // two times the same values -> generate half first
        const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )
        std::generate(vec.begin(), middle, [i = clock40MHzStart]() mutable { return std::to_string(i++); });

        // copy first half into second half
        std::copy(vec.begin(), middle, middle);

        //return std::vector<std::string>{"124"};
        return vec;
    }();

    const auto valsePllPhase160MHz = [nEntries]() {
        auto vec = std::vector<std::string>(nEntries);

        // range: 0-15 (two times)
        const int clock160MHzStart{0};

        // first half fourth bit is 0, then 1
        std::generate(vec.begin(), vec.end(), [i = clock160MHzStart]() mutable { return std::to_string(i++); });

        //return std::vector<std::string>{"12"};
        return vec;
    }();

    return {{"FIXME.ePllPhase160MHz_0", valsePllPhase160MHz},
            {"FIXME.ePllPhase160MHz_1", valsePllPhase160MHz},
            {"FIXME.ePllPhase160MHz_2", valsePllPhase160MHz},
            {"FIXME.ePllPhase40MHz_0", valsePllPhase40MHz},
            {"FIXME.ePllPhase40MHz_1", valsePllPhase40MHz},
            {"FIXME.ePllPhase40MHz_2", valsePllPhase40MHz}};
}

void Phase160MHzCalibration::setRegisters(const int t_iteration) const
{
    nsw::ConfigSender configSender;
    const auto ptree = BaseCalibration<Phase160MHzCalibration>::createPtree(m_inputValues, t_iteration);
    const auto configConverter = ConfigConverter(ptree, ConfigConverter::ConfigType::VALUE_BASED);
    const auto translatedPtree = configConverter.getRegisterBasedConfigWithoutSubregisters(m_config.getRocAnalog());
    const auto partialConfig = nsw::I2cMasterConfig(translatedPtree, ROC_ANALOG_NAME, ROC_ANALOG_REGISTERS, true);
    const auto opcIp = m_config.getOpcServerIp();
    const auto scaAddress = m_config.getAddress();

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

void Phase160MHzCalibration::saveBestSettings(const int t_bestIteration, const std::string &t_filename) const
{
    const auto bestPhase40MHz = m_inputValues.at("FIXME.ePllPhase40MHz_0").at(t_bestIteration);
    const auto bestPhase160MHz = m_inputValues.at("FIXME.ePllPhase160MHz_0").at(t_bestIteration);
    ERS_INFO("Best values (iteration) " << t_bestIteration << '\n'
              << "\t40MHz:  " << bestPhase40MHz << '\n'
              << "\t160MHz: " << bestPhase160MHz << '\n');
    std::ofstream outfile;
    outfile.open(t_filename);
    for (const auto& [registerName, values] : m_inputValues)
    {
        outfile << "rocCoreAnalog." << registerName << ':' << values[t_bestIteration] << '\n';
    }
    outfile.close();
}

std::size_t Phase160MHzCalibration::getNumberOfConfigurations() const
{
    return m_inputValues.begin()->second.size();
}
