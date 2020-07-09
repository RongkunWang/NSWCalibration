#ifndef MMTRIGGERCALIB_H_
#define MMTRIGGERCALIB_H_

//
// Derived class for NSW ART input phase calib
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/ADDCConfig.h"
using boost::property_tree::ptree;

namespace nsw {

  class MMTriggerCalib: public CalibAlg {

  public:
    MMTriggerCalib(std::string calibType);
    ~MMTriggerCalib() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    ptree patterns();
    template <class T>
      std::vector<T> make_objects(std::string cfg, std::string element_type, std::string name = "");
    int pattern_number(std::string name);
    int configure_febs_from_ptree(ptree tr, bool unmask);
    int configure_addcs_from_ptree(ptree tr);
    int configure_vmms(nsw::FEBConfig feb, ptree febpatt, bool unmask);
    int configure_art_input_phase(nsw::ADDCConfig addc, uint phase);
    int configure_tps(ptree tr);
    int addc_tp_watchdog();
    int wait_until_done();
    int announce(std::string name, ptree tr, bool unmask);
    std::string strf_time();

  private:
    std::string                    m_calibType = "";

    std::vector<nsw::FEBConfig>    m_febs   = {};
    std::vector<nsw::ADDCConfig>   m_addcs  = {};
    std::vector<nsw::TPConfig>     m_tps    = {};
    std::vector<int>               m_phases = {};

    bool m_connectivity = 0;
    bool m_tracks = 0;
    bool m_noise = 0;
    bool m_latency = 0;
    bool m_dry_run = 0;
    bool m_reset_vmm = 0;
    ptree m_patterns;
    std::unique_ptr< std::vector< std::future<int> > > m_threads = 0;
    std::map<std::string, std::unique_ptr<nsw::ConfigSender> > m_senders = {};
    std::future<int> m_watchdog;
    std::atomic<bool> m_tpscax_busy = 0;

  };

}

#endif
