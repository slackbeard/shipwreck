FROM ubuntu:16.04
RUN apt-get update && \
	apt-get install -y cmake && \
	apt-get install -y clang && \
	apt-get install -y nasm && \
	apt-get install -y g++-multilib

RUN ["/bin/mkdir", "/shipwreck"]
WORKDIR /shipwreck

CMD ["./build.sh"]
