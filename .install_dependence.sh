#! /bin/bash
git submodule update --init --recursive
 #install GCC/G++ 7
#add-apt-repository ppa:ubuntu-toolchain-r/test
#apt-get update
#apt-get install gcc-7 g++-7 -y

 #install cmake


 #install Boost
#wget http://downloads.sourceforge.net/project/boost/boost/${boost_version}/${boost_dir}.tar.gz --no-check-certificate \
    #&& tar xfz ${boost_dir}.tar.gz \
    #&& rm ${boost_dir}.tar.gz \
    #&& cd ${boost_dir} \
    #&& ./bootstrap.sh \
    #&& ./b2 --without-python --prefix=/usr -j 4 link=shared runtime-link=shared install \
    #&& cd .. && rm -rf ${boost_dir} && ldconfig
