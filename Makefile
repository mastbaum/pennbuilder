EVB_OBJS = ds.o listener.o shipper.o event_builder.o
EVB_SRCS = $(EVB_OBJS:.o=.cpp)
CLI_OBJS = client.o ds.o
CLI_SRCS = $(CLI_OBJS:.o=.cpp)

INCDIR = include
INCLUDE = -I$(INCDIR) -I$(ROOTSYS)/include -I$(AVALANCHE)
CFLAGS =
LFLAGS = -L/usr/local/lib -L$(ROOTSYS)/lib -L$(AVALANCHE)
LIBS = -ljemalloc -pthread -lrt -lavalanche -lCore -lCint

CC = g++ -g

CXXFLAGS = $(CFLAGS) $(LFLAGS) $(INCLUDE) $(LIBS) -g

all: event_builder client

event_builder: $(EVB_OBJS)
	-root -b -l -q $(INCDIR)/PackedEvent.hh+g && rm
	-$(RM) $(INCDIR)/PackedEvent_hh.d
	-mv $(INCDIR)/PackedEvent_hh.so .
	$(CC) -o $@ PackedEvent_hh.so $(EVB_OBJS) $(CXXFLAGS)

client: $(CLI_OBJS)
	$(CC) -o $@ $(CLI_OBJS) $(CXXFLAGS)

clean: 
	-$(RM) core *.o

