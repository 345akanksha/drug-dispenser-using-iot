CREATE DATABASE medicinedb;
USE medicinedb;

CREATE TABLE caretakers (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(64),
    email VARCHAR(128) UNIQUE NOT NULL,
    password VARCHAR(128) NOT NULL  -- Store hash in production!
);

CREATE TABLE schedule (
    id INT AUTO_INCREMENT PRIMARY KEY,
    medicine_name VARCHAR(64) NOT NULL,
    time TIME NOT NULL,
    taken BOOLEAN DEFAULT FALSE,
    caretaker_id INT NOT NULL,
    FOREIGN KEY (caretaker_id) REFERENCES caretakers(id)
);

CREATE TABLE push_tokens (
    id INT AUTO_INCREMENT PRIMARY KEY,
    caretaker_id INT NOT NULL,
    token VARCHAR(255),
    FOREIGN KEY (caretaker_id) REFERENCES caretakers(id)
);
