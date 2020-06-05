cd AltiController
patch -i ../NSWCalibration/AltiController_start_pattern_transition.diff schema/alti.schema.xml
patch -i ../NSWCalibration/AltiController_user.diff                     src/AltiController.cpp
cd ..
