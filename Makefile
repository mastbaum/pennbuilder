EVB_OBJS = ds.o listener.o shipper.o event_builder.o
EVB_SRCS = $(EVB_OBJS:.o=.c)
CLI_OBJS = client.o ds.o
CLI_SRCS = $(CLI_OBJS:.o=.c)

INCDIR = include
CFLAGS = -I$(INCDIR)
CXXFLAGS = 

CC = gcc

LIBS = -ljemalloc -lpthread -lrt

all: event_builder client

event_builder: $(EVB_OBJS)
	$(CC) -o $@ $(EVB_OBJS) $(LIBS) $(INCLUDE) $(CXXFLAGS)

client: $(CLI_OBJS)
	$(CC) -o $@ $(CLI_OBJS) $(LIBS) $(INCLUDE) $(CXXFLAGS)

clean: 
	-$(RM) core *.o

