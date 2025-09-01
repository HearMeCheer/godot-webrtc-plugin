# godot-webrtc-plugin
GDNative WebRTC plugin for Godot using Pion library

This plugin is based on the official Godot Engine's webrtc extension: [webrtc-native](https://github.com/godotengine/webrtc-native?tab=readme-ov-file).

## Prerequisities

### MinGW
Install MSYS2 from [MSYS2 homepage](https://www.msys2.org/).
Install required packages:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make mingw-w64-x86_64-binutils mingw-w64-x86_64-tools mingw-w64-x86_64-go scons
```

### Git
After cloning the repo you need to get submodules:
```bash
git submodule init
git submodule update
```

Alternatively you can use `git clone --recurse-submodules repo_url` when clonning the repo. 

### Python

#### pyenv

On MacOS:
```bash
brew install pyenv pyenv-virtualenv
```
On Linux:
```bash
 curl https://pyenv.run | bash
```

Install python version needed by Godot and scons:
```bash
pyenv install 3.12.2
pyenv virtualenv 3.12.2 godot
```

Use new environment at current directory:
```bash
pyenv local godot
```

#### Scons

Scons is Godot's build system written in python.

MacOS:
```bash
python -m pip install scons
```

Linux:
```bash
sudo apt install scons
```

#### Opus

Opus requirements:

Linux:
```bash
sudo apt-get install git autoconf automake libtool gcc make
```
MacOS:
```bash
brew install autoconf automake libtool
```

On linux we need to pass -fPIC flag:

```
./autogen.sh
./configure CFLAGS="-fPIC"
make
```

### Go

#### Linux

```bash
sudo apt install golang-go
```

#### Windows

```shell
choco install make
choco install mingw
choco install golang
```

or when using MSYS2

```shell
pacman -S mingw-w64-x86_64-go
```

You need to set `GOROOT` path to your `.bashrc`:

```bash
export PATH=/c/msys64/mingw64/bin:$PATH
export GOROOT=/c/msys64/mingw64/lib/go
```

To fix https://learn.microsoft.com/en-us/answers/questions/1187748/error-compiling-a-c-file-generated-from-golang-err
we need to replace `GoComplex64` definition to:
```cpp
#ifdef _MSC_VER
#  if _MSVC_LANG <= 201402L
#    include <complex.h>
     typedef _Fcomplex GoComplex64;
     typedef _Dcomplex GoComplex128;
#  else
#    include <complex>
     typedef std::complex<float> GoComplex64;
     typedef std::complex<double> GoComplex128;
#  endif
#else
  typedef float _Complex GoComplex64;
  typedef double _Complex GoComplex128;
#endif
```
