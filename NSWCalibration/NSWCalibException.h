#ifndef NSWCALIBEXCEPTION_H
#define NSWCALIBEXCEPTION_H

#include "ers/ers.h"
/**
 *Exception base class for NSWCalibration
 * */
namespace daq {
       /**Definition of the exceptions*/
       ERS_DECLARE_ISSUE(NSWCALIB,                    // namespace
                         NSWCALIBException,           // issue name
                         ,                            // empty message
                         //((const char *) condition)
                         //((const char *) reason)
                         )

       ERS_DECLARE_ISSUE_BASE(NSWCALIB,                     // namespace name
                              Issue,                        // issue name
                              NSWCALIB::NSWCALIBException,  // base issue name
                              condition                     // 
                              <<"\n"<< reason,              // message 
                              ,              // no base class attributes
                              ((std::string)condition )    // 1st attribute for this class (const string fails)
                              ((std::string)reason )       // 2nd attribute for this class
                             )

//       ERS_DECLARE_ISSUE_BASE(NSWCALIB,                     // namespace name
//                              Issue,                        // issue name
//                              NSWCALIB::NSWCALIBException,  // base issue name
//                              condition                     // 
//                              <<"\n"<< reason,              // message
//                              ((const char *)condition )    // 1st attribute for this class
//                              ((const char *)reason )       // 2nd attribute for this class
//                             )
//
       ERS_DECLARE_ISSUE_BASE(NSWCALIB,                     // namespace name
                              LostConnection,                        // issue name
                              NSWCALIB::NSWCALIBException,  // base issue name
                              "Lost connection to " << fe << " because " << err << "exception.",// 
                              ,              // message
                              ((std::string)fe )    // 1st attribute for this class
                              ((std::string)err )    // 1st attribute for this class
                             )

}
#endif
