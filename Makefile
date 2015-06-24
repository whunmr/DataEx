GTEST_DIR=/home/j/lab/gtest-1.7.0/
MOCKCPP_HEADER_DIR=/home/j/lab/mockcpp/mockcpp/include
MOCKCPP_LIB_DIR=/usr/include/mockcpp/lib

WARNFLAGS = -Wall -Wno-invalid-offsetof -Wno-c++0x-compat
CPPFLAGS= $(WARNFLAGS) -g -I/usr/include -I$(GTEST_DIR)include -I$(MOCKCPP_HEADER_DIR)
LDFLAGS=
LDLIBS=-L$(GTEST_DIR)lib -L$(MOCKCPP_LIB_DIR) -lgtest -lpthread -lmockcpp -lpcrecpp

#TEST_SRCS=$(shell find test -iname *.cpp)
TEST_SRCS=test/test.cpp test/main.cpp
TEST_OBJS=$(subst .cpp,.o,$(TEST_SRCS))

all: $(TEST_OBJS)
#g++ -I/usr/include -o msgflow src/msgflow.cpp -lpthread -lpcrecpp -lboost_regex
	@g++ $(CPPFLAGS) $(LDFLAGS) -g -o main $^ $(LDLIBS) && ./main --gtest_filter="*" && echo "" && echo "" && echo ""

clean:
	rm -rdf $(OBJS) main main.dSYM test/*.o msgflow
