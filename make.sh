gcc event_builder.c listener.c shipper.c ds.c -o event_builder -ljemalloc -lpthread -I./include ${1}
gcc client.c ds.c -o client -ljemalloc -I./include ${1}

