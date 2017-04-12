FROM rogerp/sdsfuse:0.1

# Install liberasurecode
RUN apt-get update &&\
    apt-get install autoconf automake build-essential git libtool --yes --quiet &&\
    cd /tmp &&\
    git clone https://github.com/openstack/liberasurecode --quiet &&\
    cd liberasurecode &&\
    git checkout 1.2.0 --quiet &&\
    ./autogen.sh && ./configure && make --silent && make install --silent && ldconfig &&\
    apt-get autoremove autoconf automake build-essential git libtool --yes --quiet


COPY . /usr/local/src/sfuse

RUN mkdir -p /mnt/fuse  &&\ 
    cd /usr/local/src/sfuse/  &&\
    make clean -s && make all -s && mv safefs /usr/local/bin && make clean -s && ldconfig   &&\
    # Copy configuration files in standard location
    mkdir /etc/sdsfuse && cp /usr/local/src/sfuse/default.ini /etc/sdsfuse  && cp /usr/local/src/sfuse/zlog.conf /etc/sdsfuse          &&\
    # Set up log directory
    mkdir -p /var/log/sfuse/ &&\
    mkdir -p /var/tmp/sdsfuse/dev1 && mkdir -p /var/tmp/sdsfuse/dev2 && mkdir -p /var/tmp/sdsfuse/dev3 &&\
    #Cleanup
    apt-get -y remove --auto-remove autoconf automake build-essential byacc dh-exec flex g++ gcc libtool make                          &&\
    apt-get clean                                                                                                                      &&\
    rm -rf /var/lib/apt/lists/* /var/tmp/*
COPY entrypoint.sh /usr/local/bin/
COPY mount.sh /usr/local/bin
RUN chmod +x /usr/local/bin/entrypoint.sh /usr/local/bin/mount.sh
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
WORKDIR /tmp/filebench-1.5-alpha3
VOLUME /mnt/fuse
