INCLUDE = -I./include -I$(ROOTSYS)/include -I$(AVALANCHE)
LFLAGS = -L/usr/local/lib -L$(ROOTSYS)/lib -L$(AVALANCHE)
LIBS = -ljemalloc -pthread -lrt -lavalanche -lCore -lCint
CC = g++ -g

all: root_dict event_builder

root_dict:
	$(ROOTSYS)/bin/rootcint -f PackedEvent_dict.cc -c -p -I. -I$(ROOTSYS)/include -D_REENTRANT ./include/PackedEvent.hh ./include/LinkDef.hh

event_builder: $(EVB_OBJS)
	$(CC) -o $@ PackedEvent_dict.cc ds.cpp listener.cpp shipper.cpp event_builder.cpp $(INCLUDE) $(LFLAGS) $(LIBS) $(CXXFLAGS) $(CFLAGS)

clean: 
	-$(RM) core PackedEvent_dict.* *.o event_builder

