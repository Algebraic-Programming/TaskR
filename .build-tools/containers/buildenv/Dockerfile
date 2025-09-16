FROM ghcr.io/algebraic-programming/hicr/buildenv:latest

ENV USER=hicr
ENV HOME=/home/$USER

USER $USER 

RUN sudo apt purge -y libopenblas-dev
RUN sudo apt purge -y liblapack-dev
RUN sudo apt purge -y liblapacke-dev
RUN sudo apt autoremove -y
RUN rm -rf /usr/share/doc/libopenblas  /usr/lib/libopenblas* /usr/lib/pkconfig/openblas.pc 
RUN git clone -b v0.3.28 --single-branch --depth 1 https://github.com/OpenMathLib/OpenBLAS.git $HOME/OpenBLAS && \
  cd $HOME/OpenBLAS && \
  sed -i " s/# USE_THREAD = 0/USE_THREAD=0/g " Makefile.rule && \
  sed -i " s/# USE_LOCKING = 1/USE_LOCKING=1/g " Makefile.rule && \
  make && \
  sudo make PREFIX=/usr/local install && \
  sudo rm -rf ${HOME}/OpenBLAS

RUN sudo apt-get install -y python3.10-dev
COPY requirements.txt $HOME
RUN source $HOME/.venv/bin/activate && pip install --upgrade pip && pip install -r $HOME/requirements.txt && rm $HOME/requirements.txt

RUN sudo sed -i '/^\[instrumentation\]/,/^\[/{s/version *= *"ovni"/version = "none"/}' /usr/local/share/nosv.toml 
RUN echo 'export LD_PRELOAD=/usr/local/lib/libnosv.so:$LD_PRELOAD' >> $HOME/.hicr-env.sh
WORKDIR $HOME