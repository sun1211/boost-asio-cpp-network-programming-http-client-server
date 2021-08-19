# Boost Asio C++ Network Programming HTTP Client Server
Implementation of basic HTTP server application using Boost.Asio

[Implementing the HTTP server application](server/README.md)

[Implementing the HTTP client application](client/README.md)

# Setup environment
**1. Install CMake**
```
cd ~
wget https://github.com/Kitware/CMake/releases/download/v3.14.5/cmake-3.14.5.tar.gz
tar xf cmake-3.14.5.tar.gz
cd cmake-3.14.5
./bootstrap --parallel=10
make -j4
sudo make -j4 install
```
**2. Install Boost**
```
cd ~
wget https://boostorg.jfrog.io/artifactory/main/release/1.69.0/source/boost_1_69_0.tar.gz
tar xf boost_1_69_0.tar.gz
cd boost_1_69_0
./bootstrap.sh
./b2 ... cxxflags="-std=c++0x -stdlib=libc++" linkflags="-stdlib=libc++" ...
sudo ./b2 toolset=gcc -j4 install
```

# How to build
```
mkdir build
cd build
cmake ..
cmake --build .
```

# How to Run
## Run server
```
./bin/server

```

After that we can use browser to test


## Run client
```
./bin/client 

Error occured! Error code = 125. Message: Operation canceledRequest #1 has been cancelled by the user.

Request #1 has completed. Response: <!DOCTYPE html>
<html>
<head>
<title>Server</title>
</head>
<body>

    <h1><strong>Implementing the HTTP server application.</strong></h1>
    <p>requirements that our application must satisfy:</p>
    <p>- It should support the HTTP 1.1 protocol</p>
    <p>- It should support the GET method</p>
    <p>- It should be able to process multiple requests in parallel, that is, it should be an 
asynchronous parallel server</p>

</body>
</html>
```
