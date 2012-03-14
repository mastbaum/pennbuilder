#ifdef __CINT__
#pragma link C++ class RAT::DS::RHDR+;
#pragma link C++ class RAT::DS::EPED+;
#pragma link C++ class RAT::DS::TRIG+;
#pragma link C++ class RAT::DS::PMTBundle+;
#pragma link C++ class RAT::DS::CaenTrace+;
#pragma link C++ class RAT::DS::CaenBundle+;
#pragma link C++ class RAT::DS::PackedEvent+;
#pragma link C++ class RAT::DS::GenericRec+;
#pragma link C++ class RAT::DS::PackedRec+;
#endif

#ifdef __MAKECINT__
#pragma link C++ nestedclass;
#pragma link C++ class vector<RAT::DS::PMTBundle>;
#pragma link C++ class vector<RAT::DS::CaenTrace>;
#pragma link C++ class vector<RAT::DS::CaenBundle>;
#pragma link C++ class vector<RAT::DS::PackedEvent>;
#endif

