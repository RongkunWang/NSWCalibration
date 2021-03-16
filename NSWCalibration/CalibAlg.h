#ifndef CALIBALG_H_
#define CALIBALG_H_

//
// Base class for NSW calib algs
//

#include <stdint.h>                     // for uint32_t
#include <string>                       // for string
#include <chrono>                       // for system_clock

namespace nsw {

  class CalibAlg {

  public:
    CalibAlg();
    virtual ~CalibAlg() = default;
    virtual void setup(const std::string& db);
    virtual void configure();
    virtual void unconfigure();
    bool next();
    void progressbar();
    int counter() const {return m_counter;};
    int total() const {return m_total;};
    bool toggle() const {return m_toggle;}
    bool wait4swrod() const {return m_wait4swrod;}
    bool simulation() const {return m_simulation;}
    std::string applicationName() const {return m_name;}
    uint32_t runNumber() const {return m_run_number;}
    void setCounter(int ctr) {m_counter = ctr;}
    void setTotal(int tot) {m_total = tot;}
    void setToggle(bool tog) {m_toggle = tog;}
    void setWait4swROD(bool wait) {m_wait4swrod = wait;}
    void setSimulation(bool sim) {m_simulation = sim;}
    void setApplicationName(const std::string& name) {m_name = name;}
    void setRunNumber(uint32_t val) {m_run_number = val;}

    // "progress bar"
    void setStartTime() {m_time_start = std::chrono::system_clock::now();}
    void setElapsedSeconds() {m_elapsed_seconds = std::chrono::system_clock::now() - m_time_start;}
    double elapsedSeconds() const {return m_elapsed_seconds.count();}
    double rate() const {return elapsedSeconds() > 0 ? counter() / elapsedSeconds() : -1;}
    double remainingSeconds() const {return static_cast<double>(total()-counter())/rate();}

  private:
    int m_counter = -1;
    int m_total = 0;
    bool m_toggle = true;
    bool m_wait4swrod = false;
    bool m_simulation = false;
    uint32_t m_run_number = 0;
    std::string m_name = "";
    std::chrono::time_point<std::chrono::system_clock> m_time_start;
    std::chrono::duration<double> m_elapsed_seconds{0};

  };

}

#endif
