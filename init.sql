-- 创建数据库
CREATE DATABASE IF NOT EXISTS server_data;

-- 切换数据库
USE server_data;

-- 创建 user 表（带主键）
CREATE TABLE IF NOT EXISTS user (
    username VARCHAR(50) NOT NULL PRIMARY KEY,
    password VARCHAR(50) NULL
) ENGINE=InnoDB;

-- 插入初始数据
INSERT INTO user (username, password) VALUES ('root', '123456');