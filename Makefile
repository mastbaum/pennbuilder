ORINCLUDES = -I$(ORDIR)/Decoders -I$(ORDIR)/IO -I$(ORDIR)/Management -I$(ORDIR)/Processors -I$(ORDIR)/Toolkit -I$(ORDIR)/Util
ORLIBS = -L$(ORDIR)/lib -lORDecoders -lORIO -lORManagement -lORProcessors -lORUtil
ORSRC = ORCaen1720Decoder.cc ORMTCDecoder.cc ORPMTDecoder.cc
INCLUDE = -I./include -I$(ROOTSYS)/include -I$(AVALANCHE) -I$(ORDIR)/Management $(ORINCLUDES)
LFLAGS = -L/usr/local/lib -L$(ROOTSYS)/lib -L$(AVALANCHE)
LIBS = -ljemalloc -pthread -lrt -lavalanche -lCore -lCint -lRIO $(ORLIBS)
CC = g++ -g

#include $(ORDIR)/buildTools/BasicAppMakefile

all: root_dict event_builder

root_dict:
	$(ROOTSYS)/bin/rootcint -f PackedEvent_dict.cc -c -p -I. -I$(ROOTSYS)/include -D_REENTRANT ./include/PackedEvent.hh ./include/LinkDef.hh

event_builder: $(EVB_OBJS)
	$(CC) -o $@ PackedEvent_dict.cc ds.cpp listener.cpp orca.cpp shipper.cpp event_builder.cpp $(ORSRC) $(INCLUDE) $(LFLAGS) $(LIBS) $(CXXFLAGS) $(CFLAGS)

clean: 
	-$(RM) core PackedEvent_dict.* *.o event_builder

