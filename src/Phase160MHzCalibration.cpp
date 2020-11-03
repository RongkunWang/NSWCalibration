#include "NSWCalibration/Phase160MHzCalibration.h"
#include "NSWCalibration/BaseCalibration.h"

#include "NSWConfiguration/ConfigSender.h"
#include "NSWConfiguration/FEBConfig.h"


Phase160MHzCalibration::Phase160MHzCalibration(nsw::FEBConfig t_config) : 
    m_config(t_config),
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

    const auto valsePllPhase160MHz_4 = [nEntries]() {
        auto vec = std::vector<std::string>(nEntries);

        const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

        // first half fourth bit is 0, then 1
        std::generate(vec.begin(), middle, []() { return "0"; });
        std::generate(middle, vec.end(), []() { return "1"; });

        //return std::vector<std::string>{"1"};
        return vec;
    }();

    const auto valsePllPhase160MHz_3_0 = [nEntries]() {
        auto vec = std::vector<std::string>(nEntries);

        // range: 0-15 (two times)
        const int clock160MHzStart{0};

        const auto middle = vec.begin() + vec.size() / 2; // iterator behind middle one ( algorithm works on [begin, end) )

        // first half fourth bit is 0, then 1
        std::generate(vec.begin(), middle, [i = clock160MHzStart]() mutable { return std::to_string(i++); });

        // copy first half into second half
        std::copy(vec.begin(), middle, middle);

        //return std::vector<std::string>{"12"};
        return vec;
    }();

    return {{"reg115",
             {{"ePllPhase160MHz_0[4]", valsePllPhase160MHz_4},
              {"ePllPhase40MHz_0", valsePllPhase40MHz}}},
            {"reg116",
             {{"ePllPhase160MHz_1[4]", valsePllPhase160MHz_4},
              {"ePllPhase40MHz_1", valsePllPhase40MHz}}},
            {"reg117",
             {{"ePllPhase160MHz_2[4]", valsePllPhase160MHz_4},
              {"ePllPhase40MHz_2", valsePllPhase40MHz}}},
            {"reg118",
             {{"ePllPhase160MHz_0[3:0]", valsePllPhase160MHz_3_0},
              {"ePllPhase160MHz_1[3:0]", valsePllPhase160MHz_3_0}}},
            {"reg119",
             {{"ePllPhase160MHz_2[3:0]", valsePllPhase160MHz_3_0}}}};
}


void Phase160MHzCalibration::setRegisters(const int i) const
{
    nsw::ConfigSender configSender;
    const auto opcIp = m_config.getOpcServerIp();
    auto scaAddress = m_config.getAddress();
    const auto adaptedConfig = BaseCalibration::adaptConfig(m_config, m_inputValues, i);
    const auto analog = m_config.get_child("rocPllCoreAnalog");
    for (const auto& entry : m_inputValues)
    {
        configSender.sendI2cMasterSingle(opcIp, scaAddress, analog, entry.first);
    }
}


Settings Phase160MHzCalibration::getBestSettings(const int t_bestIteration) const
{
    return Settings{m_inputValues.at("reg115").at("ePllPhase40MHz_0").at(t_bestIteration),
                    m_inputValues.at("reg118").at("ePllPhase160MHz_0[3:0]").at(t_bestIteration),
                    m_inputValues.at("reg115").at("ePllPhase160MHz_0[4]").at(t_bestIteration)};
}


int Phase160MHzCalibration::getNumberOfConfigurations() const
{
    return m_inputValues.begin()->second.begin()->second.size();
}