#ifndef MMTRIGGERCALIB_H_
#define MMTRIGGERCALIB_H_

//
// Derived class for NSW ART input phase calib
//

#include <future>
#include <string>
#include <vector>

#include "NSWCalibration/CalibAlg.h"

#include "NSWConfiguration/ADDCConfig.h"
#include "NSWConfiguration/FEBConfig.h"
#include "NSWConfiguration/TPConfig.h"

#include "TFile.h"
#include "TTree.h"

#include "ers/Issue.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWMMTriggerCalibIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class MMTriggerCalib: public CalibAlg {

  public:
    MMTriggerCalib(std::string calibType);
    ~MMTriggerCalib() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    boost::property_tree::ptree patterns();
    template <class T>
      std::vector<T> make_objects(std::string cfg, std::string element_type, std::string name = "");
    int pattern_number(std::string name);
    int configure_febs_from_ptree(boost::property_tree::ptree tr, bool unmask);
    int configure_addcs_from_ptree(boost::property_tree::ptree tr);
    int configure_vmms(nsw::FEBConfig feb, boost::property_tree::ptree febpatt, bool unmask);
    int configure_art_input_phase(nsw::ADDCConfig addc, uint phase);
    int configure_tps(boost::property_tree::ptree tr);
    int addc_tp_watchdog();

    //
    // Read the hit counters of all ART ASICs.
    // The output is saved to a ROOT file, where
    //  each entry of the TTree is 32 counters read from 1 ASIC.
    // On first iteration, create the ROOT file and tree.
    // On last iteration, close the file.
    //
    int read_arts_counters();

    //
    // https://espace.cern.ch/ATLAS-NSW-ELX/Shared%20Documents/ART/art2_registers_v.xlsx
    //
    std::vector<int> read_art_counters(const nsw::ADDCConfig& addc, int art);
    int wait_until_done();
    int announce(std::string name, boost::property_tree::ptree tr, bool unmask);
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
    bool m_staircase = 0;
    bool m_dry_run = 0;
    bool m_reset_vmm = 0;
    boost::property_tree::ptree m_patterns;
    std::unique_ptr< std::vector< std::future<int> > > m_threads = 0;
    std::future<int> m_watchdog;
    std::atomic<bool> m_tpscax_busy = 0;

    std::unique_ptr<TFile> m_art_rfile;
    std::shared_ptr<TTree> m_art_rtree;
    std::string m_addc_address;
    std::string m_art_name;
    std::string m_art_now;
    int m_art_event;
    int m_art_index;
    std::unique_ptr< std::vector<int> > m_art_hits;

  };

}

#endif
