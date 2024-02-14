# syntax=docker/dockerfile:1

FROM ubuntu:22.04

RUN apt update -y && apt upgrade -y && apt install libsodium-dev build-essential -y

WORKDIR /app

COPY . .

RUN g++ -o libnext.so -Iinclude source/*.cpp -shared  -fPIC -lsodium -lpthread -lm -DNEXT_DEVELOPMENT=1
RUN g++ -o complex_client -Iinclude examples/complex_client.cpp libnext.so -lpthread -lm -DNEXT_DEVELOPMENT=1
RUN mv /app/libnext.so /usr/local/lib && ldconfig

CMD [ "/app/complex_client" ]
