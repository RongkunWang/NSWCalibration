#ifndef NSWCALIBALG_H_
#define NSWCALIBALG_H_

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

namespace nsw {

  class NSWCalibAlg {

  public:
    NSWCalibAlg();
    ~NSWCalibAlg() {};
    virtual void setup(std::string db);
    virtual void configure();
    virtual void unconfigure();
    bool next();
    int counter() {return m_counter;};
    int total() {return m_total;};
    void setCounter(int ctr) {m_counter = ctr;};
    void setTotal(int tot) {m_total = tot;};

  private:
    int m_counter;
    int m_total;

  };

}

#endif
