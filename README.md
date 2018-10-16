# cppetcd

C++ etcd client for v3 protocol

## Prerequisites

This software depends on protocol buffers and C++ gRPC library.  Those
headers and libraries must be installed where the compiler, linkers
and loaders are accessible. For example in Arch Linux, run

```
$ yaourt -S grpc protobuf protobuf-c gtest
...
$ which protoc
/usr/bin/protoc
$ which grpc_cpp_plugin 
/usr/bin/grpc_cpp_plugin
```

## Build && Test locally

```
$ git clone git://github.com/kuenishi/cppetcd
...
$ cd cppetcd
$ make pb
$ make -j
$ make test
$ sudo make install prefix=/opt/local
```
