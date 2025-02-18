<h1 align="center">Libosi</h1>

Operating System Introspection library to support PANDA.

Currently, the following profiles are supported:

| Profile            | Level of Support |
| ------------------ | ---------------- |
| windows-32-7sp0    | full             |
| windows-32-7sp1    | full             |
| windows-64-7sp0    | full             |
| windows-64-7sp1    | full             |
| windows-32-xpsp2   | full             |
| windows-32-xpsp3   | full             |
| windows-32-2000    | **experimental** |
| linux-32-3.16      | full             |
| linux-64-3.16      | full             |

### Easy mode (apt releases)

See our releases page for the latest CI generated deb packages for Ubuntu 20.04 and 22.04.

### Prerequisites

Install dependencies. On Ubuntu, this can be done with:

```bash
sudo apt-get update

sudo apt-get install cmake ninja-build rapidjson-dev
```

### Building

To build libosi, from the root of this repo run:

```bash
mkdir build && cd $_
cmake -GNinja ..
ninja
```

### Installing

Installing libosi includes running:

```bash
cd build && ninja package
sudo dpkg -i libosi-[version].deb
```

### Testing

Testing is currently implemented for offset and iohal. To run the tests, you will 
first need to install dependencies and enable testing:

```bash
sudo apt-get install libgtest-dev

cd build
cmake -GNinja -DENABLE_TESTING=ON ..
ninja
```

You can then run the tests with just `ninja test`.

### Development 

Adding support for other Windows and Linux kernels can be as simple as adding a new profile 
to `src/offset/profiles`. However, in some kernels, struct names may have changed. In these 
cases, you may need to add/port API functions in `src/osi/windows/api.cc`, where these names 
are assumed. Additionally, Linux support is for a rather old kernel version (v3.16). Supporting
a newer kernel would likely be some development work.

Support is currently limited to i386 and amd64. Support for more architectures includes writing
a new translator within `src/iohal/translators`.

Pull Requests are welcome.

### Style

Currently, the code is formatted with clang-format, using the style provided in `.clang-format`.
