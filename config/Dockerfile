FROM debian:buster
RUN apt-get update \
 && apt-get install -y \
      build-essential \
      clang \
      clang-format \
      cmake \
      doxygen \
      git \
      graphviz \
      g++-8 \
      lcov \
      ninja-build \
      valgrind \
 && git clone https://github.com/catchorg/Catch2.git \
 && cd Catch2 \
 && git checkout v2.5.0 \
 && cmake -Bbuild -H. -DBUILD_TESTING=OFF \
 && cmake --build build/ --target install \
 && rm -rf /tmp/Catch2
