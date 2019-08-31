FROM judge-system-base:4.0

WORKDIR /opt/judge-system
COPY . ./
WORKDIR /opt/judge-system/build
RUN CC=gcc-9 CXX=g++-9 cmake -DBUILD_ENTRY=ON .. && make
WORKDIR /opt/judge-system/runguard/build
RUN CC=gcc-9 CXX=g++-9 cmake -DBUILD_ENTRY=ON .. && make

ENTRYPOINT ["/opt/judge-system/run.sh"]
