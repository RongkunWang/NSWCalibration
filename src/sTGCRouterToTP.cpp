#include "NSWCalibration/sTGCRouterToTP.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

#include <unistd.h>
#include <istream>
#include <stdexcept>

#include "ers/ers.h"

nsw::sTGCRouterToTP::sTGCRouterToTP(const std::string& calibType) {
  setCounter(-1);
  setTotal(0);
  m_calibType = calibType;
}

void nsw::sTGCRouterToTP::setup(const std::string& db) {
  ERS_INFO("setup " << db);

  m_dry_run = false;

  // parse calib type
  if (m_calibType=="sTGCRouterToTP") {
    ERS_INFO("Calib type: " << m_calibType);
  } else {
    std::stringstream msg;
    msg << "Unknown calibration request for sTGCRouterToTP: " << m_calibType << ". Crashing.";
    ERS_INFO(msg.str());
    throw std::runtime_error(msg.str());
  }

  // make NSWConfig objects from input db
  m_routers = nsw::ConfigReader::makeObjects<nsw::RouterConfig> (db, "Router");
  ERS_INFO("Found " << m_routers.size() << " Routers");

  // set number of iterations
  setTotal(static_cast<int>(m_routers.size()));
  setToggle(false);
  setWait4swROD(false);
  usleep(1e6);
}

void nsw::sTGCRouterToTP::configure() {
  ERS_INFO("sTGCRouterToTP::configure " << counter());
  constexpr int wait = 10;
  bool success = false;
  for (auto & router : m_routers) {
      auto name  = router.getAddress();
      auto layer = "L" + std::to_string(counter());
      if (name.find(layer) != std::string::npos) {
          configure_router(router, wait);
          success = true;
          break;
      }
  }
  if (!success) {
      ERS_INFO("Warning: no Router for Layer " << counter());
      usleep(wait * 1e6);
  }
  usleep(wait * 1e6);
}

void nsw::sTGCRouterToTP::unconfigure() {
  ERS_INFO("sTGCRouterToTP::unconfigure " << counter());
}

int nsw::sTGCRouterToTP::configure_router(const nsw::RouterConfig & router, int hold_reset) const {
    ERS_INFO("Configuring " << router.getAddress());
    auto cs = std::make_unique<nsw::ConfigSender>();
    if (!m_dry_run) {
        cs->sendRouterSoftReset(router, hold_reset);
    }
    return 0;
}

std::string nsw::sTGCRouterToTP::strf_time() const {
    std::stringstream ss;
    std::string out;
    std::time_t result = std::time(nullptr);
    std::tm tm = *std::localtime(&result);
    ss << std::put_time(&tm, "%Y_%m_%d_%Hh%Mm%Ss");
    ss >> out;
    return out;
}
