# pybind11 installation

To install pybind11 do: `git clone --branch stable https://github.com/pybind/pybind11.git`

and then:

```
mkdir build
cd build
cmake ..
make check -j$(nproc)
```

Be sure all the tests are passing!

Now, inside the `build` folder it will create another folder called `mock_install/share/pkgconfig` and this pkg-config file `pybind11.pc` type this:

```
prefix=/path/to/pybind11
includedir=${prefix}/include

Name: pybind11
Description: Seamless operability between C++11 and Python
Version: <current-stable-version>
Cflags: -I${includedir}
```

and add the PKG-CONFIG PATH in your `.bashrc` file:

```
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/path/to/pybind11/build/mock_install/share/pkgconfig
```

run `source ~/.bashrc` and now, you should be good-to-go with running `meson setup`.
