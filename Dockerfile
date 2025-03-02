# 构建阶段
FROM ubuntu:20.04 AS builder

# 安装构建工具和依赖
RUN sed -i 's@//.*archive.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list && \
    apt-get update && apt-get install -y \
    g++ \
    cmake \
    libmysqlclient-dev \
    libboost-regex-dev \
    libpthread-stubs0-dev \
    tzdata \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /app

# 复制项目文件
COPY . .

# 编译项目
RUN mkdir build && cd build && cmake .. && make

# 运行阶段
FROM ubuntu:20.04

# 安装运行时依赖
RUN sed -i 's@//.*archive.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list && \
    apt-get update && apt-get install -y \
    libmysqlclient21 \
    libboost-regex1.71.0 \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /app

# 复制编译好的可执行文件
COPY --from=builder /app/bin/my_webServer .
COPY ./bin/resources /app/resources

# 暴露端口（根据你的项目调整）
EXPOSE 30001

# 启动服务器
CMD ["./my_webServer"]