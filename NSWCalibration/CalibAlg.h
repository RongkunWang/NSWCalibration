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
    ~CalibAlg() {};
    virtual void setup(std::string db);
    virtual void configure();
    virtual void unconfigure();
    bool next();
    void progressbar();
    int counter() {return m_counter;};
    int total() {return m_total;};
    bool toggle() {return m_toggle;}
    bool wait4swrod() {return m_wait4swrod;}
    bool simulation() {return m_simulation;}
    std::string applicationName() {return m_name;}
    uint32_t runNumber() {return m_run_number;}
    void setCounter(int ctr) {m_counter = ctr;}
    void setTotal(int tot) {m_total = tot;}
    void setToggle(bool tog) {m_toggle = tog;}
    void setWait4swROD(bool wait) {m_wait4swrod = wait;}
    void setSimulation(bool sim) {m_simulation = sim;}
    void setApplicationName(std::string name) {m_name = name;}
    void setRunNumber(uint32_t val) {m_run_number = val;}

    // "progress bar"
    void setStartTime() {m_time_start = std::chrono::system_clock::now();}
    void setElapsedSeconds() {m_elapsed_seconds = std::chrono::system_clock::now() - m_time_start;}
    double elapsedSeconds() {return m_elapsed_seconds.count();}
    double rate() {return elapsedSeconds() > 0 ? counter() / elapsedSeconds() : -1;}
    double remainingSeconds() {return (double)(total()-counter())/rate();}

  private:
    int m_counter;
    int m_total;
    bool m_toggle = 1;
    bool m_wait4swrod = 0;
    bool m_simulation = 0;
    uint32_t m_run_number = 0;
    std::string m_name = "";
    std::chrono::time_point<std::chrono::system_clock> m_time_start;
    std::chrono::duration<double> m_elapsed_seconds;

  };

}

#endif
