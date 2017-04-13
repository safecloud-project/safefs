FROM ubuntu:14.04

RUN apt-get update &&\
    # Install basic requirements
    apt-get install --assume-yes --quiet autoconf automake build-essential byacc dh-exec flex g++ gcc libfuse-dev libgcrypt20-dev libglib2.0-dev libleveldb-dev libsqlite3-dev libssl-dev libsnappy-dev libtool make pkg-config wget &&\
    # Install libunwind to be able to compile gperftools
    # For more details: https://github.com/gperftools/gperftools/blob/master/INSTALL
    cd /tmp &&\
    wget http://download.savannah.gnu.org/releases/libunwind/libunwind-1.1.tar.gz --quiet &&\
    tar xf libunwind-1.1.tar.gz &&\
    cd libunwind-1.1 &&\
    ./configure && make -s && make install -s && ldconfig &&\
    cd /tmp &&\
    rm --recursive --force libunwind-1.1.tar.gz libunwind-1.1 &&\
    # Install gperftools' tcmalloc for leveldb
    cd /tmp &&\
    wget https://github.com/gperftools/gperftools/releases/download/gperftools-2.5/gperftools-2.5.tar.gz --quiet &&\
    tar xf gperftools-2.5.tar.gz &&\
    cd gperftools-2.5 &&\
    ./configure && make -s && make install -s && ldconfig &&\
    cd /tmp &&\
    rm --recursive --force gperftools-2.5.tar.gz gperftools-2.5 &&\
    # Install the snappy compression library for leveldb
    cd /tmp/ &&\
    wget https://github.com/google/snappy/releases/download/1.1.3/snappy-1.1.3.tar.gz --quiet &&\
    tar xf snappy-1.1.3.tar.gz &&\
    cd snappy-1.1.3 &&\
    ./configure && make && make install && ldconfig &&\
    cd /tmp &&\
    rm --recursive --force snappy-1.1.3.tar.gz snappy-1.1.3 &&\
    # Install leveldb's db_bench
    cd /tmp &&\
    wget https://github.com/google/leveldb/archive/v1.19.tar.gz --output-document=leveldb.tgz --quiet &&\
    tar xaf leveldb.tgz &&\
    cd leveldb-1.19 &&\
    make &&\
    cp out-static/db_bench /usr/local/bin/ &&\
    cd /tmp &&\
    sync &&\
    rm --recursive --force /tmp/leveldb-1.19 /tmp/leveldb.tgz &&\
    # Install filebench
    cd /tmp && wget https://github.com/filebench/filebench/archive/1.5-alpha3.tar.gz --quiet && tar xaf 1.5-alpha3.tar.gz && rm 1.5-alpha3.tar.gz && cd filebench-1.5-alpha3 &&\
    ldconfig && libtoolize && aclocal && autoheader && automake --add-missing && autoconf && ./configure && make -s && make install -s &&\
    rm --recursive /tmp/filebench-1.5-alpha3 &&\
    # Install zlog
    wget https://github.com/HardySimpson/zlog/archive/1.2.12.tar.gz --output-document=/tmp/zlog-1.2.12.tar.gz --quiet                  &&\
    cd /tmp/                                                                                                                           &&\
    tar xaf zlog-1.2.12.tar.gz                                                                                                         &&\
    cd zlog-1.2.12                                                                                                                     &&\
    make                                                                                                                               &&\
    make install                                                                                                                       &&\
    cd /                                                                                                                               &&\
    rm --force --recursive /tmp/zlog-1.2.12 /tmp/zlog-1.2.12.tar.gz                                                                    &&\
    ldconfig                                                                                                                           &&\
    # Install inih
    cd /tmp                                                                                                                            &&\
    wget https://github.com/benhoyt/inih/archive/r38.tar.gz --output-document=/tmp/inih.tgz --quiet                                    &&\
    tar xaf inih.tgz                                                                                                                   &&\
    mkdir --parents /usr/local/src/safefs                                                                                              &&\
    mv inih-r38 /usr/local/src/safefs/inih                                                                                             &&\
    rm inih.tgz                                                                                                                        &&\
    ldconfig                                                                                                                           &&\
    #Cleanup
    apt-get -y remove --auto-remove autoconf libtool wget libsnappy-dev                                                                &&\
    apt-get clean                                                                                                                      &&\
    rm -rf /var/lib/apt/lists/* /var/tmp/*                                                                                             &&\
    apt-get update && apt-get -yq install wget ruby-tokyocabinet 																						                           &&\
    cd /                                                                                                                               &&\
	  wget https://gist.githubusercontent.com/vschiavoni/59d4dc51814a89248158011f3a08cd7e/raw/546a9442b34395da74df3049a438dd6704fdffbf/tokyo-cabinet-benchmarks.rb --quiet                                                                                                                            &&\
    # Install liberasurecode
    apt-get update &&\
    apt-get install autoconf automake build-essential git libtool --yes --quiet &&\
    cd /tmp &&\
    git clone https://github.com/openstack/liberasurecode --quiet &&\
    cd liberasurecode &&\
    git checkout 1.2.0 --quiet &&\
    ./autogen.sh && ./configure && make --silent && make install --silent && ldconfig &&\
    apt-get autoremove autoconf automake build-essential git libtool --yes --quiet


COPY . /usr/local/src/safefs

RUN mkdir -p /mnt/fuse  &&\ 
    cd /usr/local/src/safefs/  &&\
    make clean -s && make all -s && mv safefs /usr/local/bin && make clean -s && ldconfig   &&\
    # Copy configuration files in standard location
    mkdir /etc/safefs && cp /usr/local/src/safefs/default.ini /etc/safefs  && cp /usr/local/src/safefs/zlog.conf /etc/safefs          &&\
    # Set up log directory
    mkdir -p /var/log/sfuse/ &&\
    mkdir -p /var/tmp/safefs/dev1 && mkdir -p /var/tmp/safefs/dev2 && mkdir -p /var/tmp/safefs/dev3 &&\
    #Cleanup
    apt-get -y remove --auto-remove autoconf automake build-essential byacc dh-exec flex g++ gcc libtool make &&\
    apt-get clean &&\
    rm -rf /var/lib/apt/lists/* /var/tmp/*
COPY entrypoint.sh mount.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh /usr/local/bin/mount.sh
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
VOLUME /mnt/fuse
WORKDIR /
