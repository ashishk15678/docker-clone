FROM ubuntu:latest
LABEL maintainer="docker-clone"
RUN apt-get update && apt-get install -y curl
WORKDIR /app
COPY . /app
EXPOSE 8080
ENV NODE_ENV=production
CMD ["echo", "Hello from docker-clone!"]

