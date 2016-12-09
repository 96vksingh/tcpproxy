COMPILER         = $(CXX)
OPTIMIZATION_OPT = -O0
OPTIONS          = -pedantic -ansi -Wall -Werror $(OPTIMIZATION_OPT) -g -std=c++11
PTHREAD          = -lpthread
LINKER_OPT       = -lstdc++ $(PTHREAD) -lboost_thread -lboost_system

BUILD_LIST+=tcpproxy

all: $(BUILD_LIST)

tcpproxy: tcpproxy.cpp
	$(COMPILER) $(OPTIONS) $(EXTA_CFLAGS) -o tcpproxy tcpproxy.cpp $(LINKER_OPT)

strip_bin :
	strip -s tcpproxy

clean:
	rm -f tcpproxy core *.o *.bak *~ *stackdump *#
