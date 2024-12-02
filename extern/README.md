# DetectR

DetectR is a lirbary used for instrumentation. To use DetectR one has to first install [ovni](https://gitlab.huaweirc.ch/zrc-von-neumann-lab/runtime-system-innovations/ovni) by typing this:

```
mkdir extern/detectr/extern/ovni/build; pushd extern/detectr/extern/ovni/build; cmake .. -DCMAKE_INSTALL_PREFIX=$prefix; make -j24; make install; popd
```

with `$prefix` being the wanted install directory for ovni: (e.g. `export prefix=$HOME/src/ovni`)

then add these exports to your `.bashrc` file:

```
# ovni library paths
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$HOME/src/ovni/lib/pkgconfig
export PATH=$HOME/src/ovni/bin:$PATH # executables
```

Test if ovni is installed correctly by going into the build folder (`cd extern/ovni/build`)
and run `make test`

## Paraver

To visualize the traces, one needs to download and install [Paraver](https://tools.bsc.es/paraver):

```
wget https://ftp.tools.bsc.es/wxparaver/wxparaver-4.11.4-Linux_x86_64.tar.bz2
```

For more informations on how to use ovni, go to their [documentations](https://ovni.readthedocs.io/en/master/).
