# Prerequisites

These are the base requirements to build and use qsfs from a source package (as described below): 
- GNU Compiler Collection (GCC) 4.1.2 or later
- [CMake][cmake install link] v3.0 or later

### Additional Requirements
**qsfs** is integrated with QingStor via the [QingStor SDK for C++][qs-sdk-cpp link]. **qsfs** uses glog for logging and gtest for unit testing. During the CMake build's configure, these dependencies will be downloaded from their github repo and get installed. So, basically you can just leave them alone.

**qsfs** is a fuse based filesystem, so you must have libfuse installed. QingStor SDK requires libcurl and libopenssl, so you also must have them installed. Typically, you'll find these packages in your system's package manager.

You also need to install [git][git install link] in order to clone the source code from GitHub.

To install these packages on Debian/Ubuntu-based systems:
```sh
 $ [sudo] apt-get install gcc g++ git fuse libfuse-dev libcurl4-openssl-dev libssl-dev
```

To install these packages on Redhat/Fedora-based systems:
```sh
 $ [sudo] yum install gcc gcc-c++ git fuse fuse-devel libcurl-devel openssl-devel
```

# Build from Source using CMake

Clone the qsfs source from [yunify/qsfs][qsfs github link] on GitHub:
```sh
 $ git clone https://github.com/yunify/qsfs-fuse.git
```

Enter the project directory containing the package's source code:
```sh
 $ cd qsfs
```

It is always a good idea to not pollute the source with build files,
so create a directory in which to create your build files:
```sh
 $ mkdir build
 $ cd build
```

Run cmake to configure and install dependencies, NOTICE as we install
dependencies in cmake configure step, the terminal will wait for you
to type password in order to get root privileges:
```sh
 $ cmake ..
```

Notice, if you want to enable unit test, specfiy -DBUILD_TESTING=ON in cmake configure step; 
```sh
 $ cmake -DBUILD_TESTING=ON ..
```

Run make to build:
```sh
 $ make
```

Run unit tests:
```sh
 $ make test
```
  or
```sh
 $ ctest -R qsfs -V
```

Install the programs and any data files and documentation:
```sh
 $ [sudo] make install
```

To remove all installed files:
```sh
 $ [sudo] make uninstall
```

To clean the generated build files, just remove the folder of build:
```sh
 $ rm -rf build
```

To clean program binaries, juse remove the folder of bin:
```sh
 $ rm -rf bin
```


[qsfs github link]: https://github.com/yunify/qsfs-fuse
[qs-sdk-cpp link]: https://github.com/yunify/qingstor-sdk-cpp
[git install link]: https://git-scm.com/book/en/v2/Getting-Started-Installing-Git
[cmake install link]: https://cmake.org/install/