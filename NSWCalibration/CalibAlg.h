#ifndef CALIBALG_H_
#define CALIBALG_H_

//
// Base class for NSW calib algs
//

#include <iostream>
#include <thread>
#include <future>
#include <regex>

#include "ers/ers.h"

#include "NSWConfiguration/ConfigReader.h"
#include "NSWConfiguration/ConfigSender.h"

using boost::property_tree::ptree;
namespace chr = std::chrono;

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
    void setCounter(int ctr) {m_counter = ctr;}
    void setTotal(int tot) {m_total = tot;}
    void setToggle(bool tog) {m_toggle = tog;}
    void setWait4swROD(bool wait) {m_wait4swrod = wait;}

    // "progress bar"
    void setStartTime() {m_time_start = chr::system_clock::now();}
    void setElapsedSeconds() {m_elapsed_seconds = chr::system_clock::now() - m_time_start;}
    double elapsedSeconds() {return m_elapsed_seconds.count();}
    double rate() {return elapsedSeconds() > 0 ? counter() / elapsedSeconds() : -1;}
    double remainingSeconds() {return (double)(total()-counter())/rate();}

  private:
    int m_counter;
    int m_total;
    bool m_toggle = 1;
    bool m_wait4swrod = 0;
    chr::time_point<chr::system_clock> m_time_start;
    chr::duration<double> m_elapsed_seconds;

  };

}

#endif
