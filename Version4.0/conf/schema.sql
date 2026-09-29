CREATE DATABASE IF NOT EXISTS Netdisk
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'netdisk'@'localhost'
  IDENTIFIED BY 'netdisk_dev_password';
GRANT ALL PRIVILEGES ON Netdisk.* TO 'netdisk'@'localhost';
CREATE USER IF NOT EXISTS 'netdisk'@'%'
  IDENTIFIED BY 'netdisk_dev_password';
GRANT ALL PRIVILEGES ON Netdisk.* TO 'netdisk'@'%';
FLUSH PRIVILEGES;

USE Netdisk;

CREATE TABLE IF NOT EXISTS Shadow
(
    User         VARCHAR(64)  NOT NULL PRIMARY KEY,
    PasswordHash VARCHAR(512) NOT NULL
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS Virtual_Dir
(
    FileId   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    Dir      BIGINT UNSIGNED NOT NULL,
    FileName VARCHAR(255)    NOT NULL,
    FileSize BIGINT UNSIGNED NOT NULL DEFAULT 0,
    FileType CHAR(1)         NOT NULL,
    SHA256   CHAR(64)        NULL,
    User     VARCHAR(64)     NOT NULL,
    UNIQUE KEY uk_user_dir_name(User, Dir, FileName),
    KEY idx_user_dir(User, Dir),
    KEY idx_sha256(SHA256)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS Log
(
    Id        BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    User      VARCHAR(64)  NOT NULL,
    Operation VARCHAR(255) NOT NULL,
    time      DATETIME     NOT NULL
) ENGINE=InnoDB;
