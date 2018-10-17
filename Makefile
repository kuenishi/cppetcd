.PHONY: all clean pb test install

TEST_SOURCES=$(wildcard test/*.cc)

TEST_RUNNER=test_runner
TEST_OBJECTS=$(TEST_SOURCES:.cc=.o)
CC_OBJECTS=src/cppetcd.o src/etcd/etcdserver/etcdserverpb/rpc.pb.o src/etcd/etcdserver/etcdserverpb/rpc.grpc.pb.o \
src/etcd/etcdserver/api/v3lock/v3lockpb/v3lock.pb.o src/etcd/etcdserver/api/v3lock/v3lockpb/v3lock.grpc.pb.o \
src/gogoproto/gogo.pb.o src/google/api/http.pb.o src/google/api/annotations.pb.o \
src/etcd/auth/authpb/auth.pb.o src/etcd/mvcc/mvccpb/kv.pb.o
CC_TARGET=libcppetcd.so

INCLUDES=`pkg-config --cflags protobuf grpc++ grpc` -I./src
CXXFLAGS += $(INCLUDES) -g -std=c++11 -fPIC

LIBS=`pkg-config --libs protobuf grpc++ grpc`
LDFLAGS=$(LIBS) -lgrpc++_reflection -ldl -g -lglog

all: ${CC_TARGET}

${CC_TARGET}: ${CC_OBJECTS}
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(CC_OBJECTS) -shared -o $@ 

test: ${TEST_RUNNER}
	@echo "To run test, locally-running etcd is required."
	LD_LIBRARY_PATH=. ./$< --gmock_verbose=info --gtest_stack_trace_depth=10

${TEST_RUNNER}: ${TEST_OBJECTS} $(CC_TARGET)
	${CXX} ${TEST_OBJECTS} -o $@ -L. -lcppetcd  -lgtest $(LDFLAGS)

clean:
	-rm -rvf *.pb.h *.pb.cc
	-rm -f $(CC_OBJECTS) $(TEST_OBJECTS)

install: ${CC_TARGET}
	install -D ${CC_TARGET} $(prefix)/lib/${CC_TARGET}
	install -D src/cppetcd.h $(prefix)/include/cppetcd.h
## TODO: install protobuf headers here too

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I. -c -o $@ $<

PROTOC_GRPC_CPP_PLUGIN=`which grpc_cpp_plugin`

PROTOC_OPT=--proto_path=protobuf \
	--plugin=protoc-gen-grpc=$(PROTOC_GRPC_CPP_PLUGIN) \
	--proto_path=googleapis \
	--proto_path=. \
	--cpp_out=src --grpc_out=src

pb:
	@echo grpc_cpp_plugin=$(PROTOC_GRPC_CPP_PLUGIN)
	git submodule init
	git submodule update
	protoc $(PROTOC_OPT) etcd/etcdserver/etcdserverpb/rpc.proto
	protoc $(PROTOC_OPT) etcd/etcdserver/api/v3lock/v3lockpb/v3lock.proto
	protoc $(PROTOC_OPT) protobuf/gogoproto/gogo.proto
	protoc $(PROTOC_OPT) etcd/mvcc/mvccpb/kv.proto
	protoc $(PROTOC_OPT) etcd/auth/authpb/auth.proto
	protoc $(PROTOC_OPT) google/api/annotations.proto
	protoc $(PROTOC_OPT) google/api/http.proto
