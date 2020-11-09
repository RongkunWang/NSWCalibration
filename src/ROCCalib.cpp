#include <filesystem>

#include "NSWCalibration/ROCCalib.h"
#include "NSWCalibration/BaseCalibration.h"
#include "NSWCalibration/Phase160MHzCalibration.h"
using boost::property_tree::ptree;

nsw::ROCCalib::ROCCalib(std::string calibType) {
  setCounter(-1);
  setTotal(1);
  setToggle(0);
  m_calibType = calibType;
}

void nsw::ROCCalib::setup(std::string db) {
  ERS_INFO("setup " << db);

  m_dry_run = 0;
  m_db = db;

  // parse calib type
  if (m_calibType=="ROCPhase") {
    ERS_INFO(m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for ROCCalib: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

}


[[nodiscard]] std::map<std::string, nsw::FEBConfig> nsw::ROCCalib::splitConfigs() const
{
    // takes either vector or set...
    nsw::ConfigReader reader(m_db);
    const auto func = [&reader] (const auto& t_names) {
        std::map<std::string, nsw::FEBConfig> feb_configs;
        // TODO: Adapt this???
        for (const auto& name : t_names)
        {
            std::cout << name << std::endl;
            if (name == "MMFE8_0000")
            {
                std::cout << "GOOD" << std::endl;
            }
            try
            {
                if (nsw::getElementType(name) == "MMFE8")
                {
  		    feb_configs.emplace(name, reader.readConfig(name));
                    std::cout << "Adding: " << name << '\n';
                }
                else
                {
                    std::cout << "Skipping: " << name
                              << " because its a " << nsw::getElementType(name)
                              << '\n';
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "WARNING! Skipping FE " << name
                          << " - Problem constructing configuration due to : " << e.what() << '\n';
                throw;
            }
        }
        return feb_configs;
    };
    return func(reader.getAllElementNames());
  
}

void nsw::ROCCalib::configure() {
  ERS_INFO("ROCCalib::configure " << counter());
  if (m_calibType=="ROCPhase") {
    ERS_INFO("Hey, partition! Do you have a minute?");
    ERS_INFO("...");
    ERS_INFO("Can it wait a bit? I'm in the middle of some calibrations.");
    for (const auto& [name, config] : splitConfigs() )
      {
         BaseCalibration<Phase160MHzCalibration> calibrator(config);
	 ERS_INFO("Calibrating FEB " << name);
         const std::string filename = "ROCPhase_optimal_values_boardid_" + name + ".txt";
         calibrator.run(m_dry_run, filename);
         break; // FIXME: All boards
      }
    ERS_INFO("Current path is " << std::filesystem::current_path());
    ERS_INFO("Everything is fine!");
  }
}

void nsw::ROCCalib::unconfigure() {
  ERS_INFO("ROCCalib::unconfigure " << counter());
}

