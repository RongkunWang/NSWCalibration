

#ifndef CALIBALG_H_ 
#define CALIBALG_H_ 
 
//
// Base class for NSW calib algs //
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

  class CalibAlg {
  
    public:
      CalibAlg();
      ~CalibAlg() {};
      virtual void setup(std::string db);
      virtual void configure();
      virtual void configure(int tpdac); // my addition...
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
