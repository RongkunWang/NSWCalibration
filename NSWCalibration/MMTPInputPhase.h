#ifndef MMTPINPUTPHASE_H_
#define MMTPINPUTPHASE_H_

//
// Derived class for testing TP input phases
//

#include "NSWCalibration/CalibAlg.h"
#include "NSWConfiguration/TPConfig.h"
#include "TFile.h"
#include "TTree.h"

ERS_DECLARE_ISSUE(nsw,
                  NSWMMTPInputPhaseIssue,
                  message,
                  ((std::string)message)
                  )

namespace nsw {

  class MMTPInputPhase: public CalibAlg {

  public:
    MMTPInputPhase(std::string calibType);
    ~MMTPInputPhase() {};
    void setup(std::string db);
    void configure();
    void unconfigure();

  public:
    int configure_tp(const nsw::TPConfig & tp, int phase, int offset);
    int read_tp     (const nsw::TPConfig & tp, int phase, int offset);
    std::string strf_time();

  private:
    /// output text file of TP SCAX reads
    std::ofstream m_myfile;
    std::unique_ptr<TFile> m_rfile = 0;
    std::shared_ptr<TTree> m_rtree = 0;

    /// output root file of TP SCAX reads
    std::string m_now = "";
    int m_phase = 0;
    int m_offset = 0;
    std::unique_ptr< std::vector<int> > m_align = 0;
    std::unique_ptr< std::vector<int> > m_bcid  = 0;
    std::unique_ptr< std::vector<int> > m_fiber = 0;

    /// number of times to read the TP SCAX registers
    int m_nreads = 20;

    /// number of input phases available (register 0x0B)
    int m_nphases = 8;

    /// number of input phase-offsets available (register 0x0C)
    int m_noffsets = 8;

    std::string m_calibType = "";
    std::vector<nsw::TPConfig> m_tps = {};
  };

}

#endif
