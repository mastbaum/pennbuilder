ORINCLUDES = -I$(ORDIR)/Decoders -I$(ORDIR)/IO -I$(ORDIR)/Management -I$(ORDIR)/Processors -I$(ORDIR)/Toolkit -I$(ORDIR)/Util
ORLIBS = -L$(ORDIR)/lib -lORDecoders -lORIO -lORManagement -lORProcessors -lORUtil
ORSRC = orca/ORCaen1720Decoder.cc orca/ORMTCDecoder.cc orca/ORPMTDecoder.cc

INCLUDE = -I./include -I$(ROOTSYS)/include -I$(AVALANCHE) -I$(ORDIR)/Management $(ORINCLUDES)
LFLAGS = -L/usr/local/lib -L$(ROOTSYS)/lib -L$(AVALANCHE)
LIBS = -ljemalloc -pthread -lrt -lavalanche -lCore -lCint -lRIO $(ORLIBS)
CC = g++ -g

all: directories root_dict event_builder

directories:
	test -d build || mkdir build
	test -d include || mkdir include
	cp -av ds/*.h ds/*.hh include/
	cp -av io/*.h include/
	cp -av orca/*.h orca/*.hh include/

root_dict:
	$(ROOTSYS)/bin/rootcint -f ./build/PackedEvent_dict.cc -c -p -I. -I$(ROOTSYS)/include -D_REENTRANT $(PWD)/include/PackedEvent.hh $(PWD)/include/LinkDef.hh

event_builder: $(EVB_OBJS)
	$(CC) -o $@ build/PackedEvent_dict.cc ds/ds.cpp io/listener.cpp io/shipper.cpp orca/orca.cpp event_builder.cpp $(ORSRC) $(INCLUDE) $(LFLAGS) $(LIBS) $(CXXFLAGS) $(CFLAGS)

clean: 
	-$(RM) -rf build event_builder

