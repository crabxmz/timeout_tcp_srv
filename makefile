CXX ?= g++

DEBUG ?= 0

CXXFLAGS += -std=c++11

ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

CXXFLAGS += -lpthread

threadpool_test:./threadpool/*
	$(CXX) -o threadpool_test.o  $^ $(CXXFLAGS)

tcp_srv:./timeout_tcp_srv/* \
		./threadpool/threadpool.h \
		./timer/*
	$(CXX) -o tcp_srv.o  $^ $(CXXFLAGS)

clean:
	rm -r *.o