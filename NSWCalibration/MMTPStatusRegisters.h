#ifndef MMTPSTATUSREGISTERS_H_
#define MMTPSTATUSREGISTERS_H_

#include <string>
#include <vector>

#include "NSWConfiguration/TPConfig.h"

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

namespace nsw {

  class MMTPStatusRegisters {

  public:
    MMTPStatusRegisters();
    ~MMTPStatusRegisters() = default;

    void Initialize();
    void Execute();
    void Finalize();

    void SetDebug     (const bool val) {m_debug     = val;}
    void SetSimulation(const bool val) {m_sim       = val;}
    void SetEnable    (const bool val) {m_enable    = val;}
    void SetResetL1A  (const bool val) {m_reset_l1a = val;}
    void SetSleepTime (const uint32_t val)     {m_sleep_time = val;}
    void SetConfig    (const std::string& val) {m_config     = val;}
    void SetLab       (const std::string& val) {m_lab        = val;}
    void SetSector    (const std::string& val) {m_sector     = val;}
    void SetRunNumber (const std::string& val) {m_run_number = val;}
    void SetMetadata  (const std::string& val) {m_metadata   = val;}


    bool Enable()           const {return m_enable;}
    bool Debug()            const {return m_debug;}
    bool Simulation()       const {return m_sim;}
    bool Sim()              const {return m_sim;}
    bool ResetL1A()         const {return m_reset_l1a;}
    uint32_t SleepTime()    const {return m_sleep_time;}
    std::string Config()    const {return m_config;}
    std::string Lab()       const {return m_lab;}
    std::string Sector()    const {return m_sector;}
    std::string RunNumber() const {return m_run_number;}
    std::string Metadata()  const;

  public:
    void InitializeMMTPConfig();
    void InitializeTTree();
    void FinalizeTTree();
    void ExecuteStartOfLoop();
    void ExecuteFiberIndex();
    void ExecuteBasicReads();
    void ExecuteBufferOverflow();
    void ExecuteFiberAlignment();
    void ExecuteHotVMMs();
    void ExecuteResetL1A();
    void ExecuteEndOfLoop();

  private:
    std::string m_config;
    std::vector<nsw::TPConfig> m_tps;

  private:
    std::string m_opc_ip;
    std::string m_tp_address;
    std::string m_metadata;
    std::string m_run_number;
    std::string m_lab;
    std::string m_sector;

  private:
    bool m_sim;
    bool m_debug;
    bool m_enable;

  private:
    std::unique_ptr<TFile> m_rfile;
    std::shared_ptr<TTree> m_rtree;
    std::unique_ptr<std::vector<uint32_t> > m_fiber_index;
    std::unique_ptr<std::vector<uint32_t> > m_fiber_align;
    std::unique_ptr<std::vector<uint32_t> > m_fiber_masks;
    std::unique_ptr<std::vector<uint32_t> > m_fiber_hots;
    std::string m_now;
    std::string m_rname;
    uint32_t m_event;
    uint32_t m_overflow_word;
    uint32_t m_date_code;
    uint32_t m_git_hash;
    uint32_t m_fiber_align_word;
    uint32_t m_sleep_time;
    bool m_reset_l1a;

  };

}

#endif
