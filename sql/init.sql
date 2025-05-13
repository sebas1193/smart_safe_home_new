-- 1. Dueños de casa (adultos mayores), sin referencias a nodos aquí
CREATE TABLE house_owner (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    first_name      VARCHAR(50) NOT NULL,
    last_name       VARCHAR(50) NOT NULL,
    phone           VARCHAR(20) NOT NULL,
    address         VARCHAR(150) NOT NULL,
    created_at      DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 2. Nodos de dispositivo, cada uno ligado a un owner
CREATE TABLE nodes (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    id_node         VARCHAR(100)    NOT NULL UNIQUE,
    owner_id        INT             NULL,
    location        ENUM('front_door','living_room','kitchen','other') NOT NULL,
    description     VARCHAR(200)    NULL,
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (owner_id) REFERENCES house_owner(id) ON DELETE CASCADE
);

-- 3. Usuarios de la app (admin y contactos de emergencia)
CREATE TABLE app_user (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    username        VARCHAR(50)     NOT NULL UNIQUE,
    password_hash   VARCHAR(255)    NOT NULL,
    role            ENUM('admin','emergency_contact') NOT NULL,
    first_name      VARCHAR(50)     NULL,
    last_name       VARCHAR(50)     NULL,
    email           VARCHAR(100)    NULL,
    phone           VARCHAR(20)     NULL,
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 4. Relación N a N entre owners y contacts
CREATE TABLE owner_contact (
    owner_id   INT NOT NULL,
    user_id    INT NOT NULL,
    PRIMARY KEY (owner_id, user_id),
    FOREIGN KEY (owner_id) REFERENCES house_owner(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)  REFERENCES app_user(id)    ON DELETE CASCADE
);

-- 5. Tablas de lecturas, apuntando a nodes.id_node con FK
CREATE TABLE front_door (
    id        INT AUTO_INCREMENT PRIMARY KEY,
    id_node   VARCHAR(100)     NOT NULL,
    door      BOOLEAN          NOT NULL,
    buzzer    BOOLEAN          NOT NULL,
    identification BOOLEAN      NOT NULL,
    sent_at       DATETIME         NOT NULL,
    recorded_at DATETIME       NOT NULL,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

CREATE TABLE living_room (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)     NOT NULL,
    presence      BOOLEAN          NOT NULL,
    lights        BOOLEAN          NOT NULL,
    sent_at       DATETIME         NOT NULL,
    recorded_at   DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

CREATE TABLE kitchen (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)     NOT NULL,
    temperature   FLOAT            NOT NULL,
    humidity      FLOAT            NOT NULL,
    gas           INT              NOT NULL,
    sent_at       DATETIME         NOT NULL,                        
    recorded_at   DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,  
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

CREATE TABLE status_logs (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)     NOT NULL,
    status_alert  INT              NOT NULL,
    location      VARCHAR(100)     NOT NULL,
    sent_at       DATETIME         NOT NULL,                        
    recorded_at   DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,  
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- EJEMPLO 1: John Doe
-- 1) Propietario
INSERT INTO house_owner (first_name, last_name, phone, address)
VALUES ('John', 'Doe', '555-1234', '123 Main St.');

-- 2) Nodo asociado a John Doe
INSERT INTO nodes (id_node, owner_id, location, description)
VALUES ('NODE_A', 1, 'front_door', 'Sensor principal de la puerta frontal');

-- 3) Contacto de emergencia (usuario de la app)
INSERT INTO app_user (username, password_hash, role, first_name, last_name, email, phone)
VALUES ('jdoe_contact', '$2b$12$hashDeEjemplo1', 'emergency_contact', 'Mary', 'Smith', 'mary.smith@example.com', '555-5678');

-- 4) Relación owner–contact
INSERT INTO owner_contact (owner_id, user_id)
VALUES (1, 1);

-- 5) Lecturas de sensores para NODE_A
--   a) Puerta delantera
INSERT INTO front_door (id_node, door, buzzer, identification, recorded_at)
VALUES ('NODE_A', TRUE, FALSE, TRUE, '2025-05-12 10:05:00');

--   b) Sala
INSERT INTO living_room (id_node, presence, lights, sent_at, recorded_at)
VALUES ('NODE_A', FALSE, FALSE, '2025-05-12 10:04:30', '2025-05-12 10:04:35');

--   c) Cocina
INSERT INTO kitchen (id_node, temperature, humidity, gas, sent_at, recorded_at)
VALUES ('NODE_A', 22.5, 45.0, 0, '2025-05-12 10:06:00', '2025-05-12 10:06:05');

--   d) Logs de estado
INSERT INTO status_logs (id_node, status_alert, location, sent_at, recorded_at)
VALUES ('NODE_A', 0, 'front_door', '2025-05-12 10:05:05', '2025-05-12 10:05:10');


-- EJEMPLO 2: Jane Doe
-- 1) Propietaria
INSERT INTO house_owner (first_name, last_name, phone, address)
VALUES ('Jane', 'Doe', '555-9876', '456 Elm St.');

-- 2) Nodo asociado a Jane Doe
INSERT INTO nodes (id_node, owner_id, location, description)
VALUES ('NODE_B', 2, 'kitchen', 'Sensor de ambiente en cocina');

-- 3) Contacto de emergencia
INSERT INTO app_user (username, password_hash, role, first_name, last_name, email, phone)
VALUES ('jdoe_emerg', '$2b$12$hashDeEjemplo2', 'emergency_contact', 'Bob', 'Johnson', 'bob.johnson@example.com', '555-4321');

-- 4) Relación owner–contact
INSERT INTO owner_contact (owner_id, user_id)
VALUES (2, 2);

-- 5) Lecturas de sensores para NODE_B
--   a) Puerta delantera (aunque el nodo está en cocina, simulamos otra lectura)
INSERT INTO front_door (id_node, door, buzzer, identification, recorded_at)
VALUES ('NODE_B', FALSE, FALSE, FALSE, '2025-05-12 11:15:00');

--   b) Sala (simulada)
INSERT INTO living_room (id_node, presence, lights, sent_at, recorded_at)
VALUES ('NODE_B', TRUE, TRUE, '2025-05-12 11:14:30', '2025-05-12 11:14:35');

--   c) Cocina (principal)
INSERT INTO kitchen (id_node, temperature, humidity, gas, sent_at, recorded_at)
VALUES ('NODE_B', 24.0, 50.0, 1, '2025-05-12 11:16:00', '2025-05-12 11:16:05');

--   d) Logs de estado
INSERT INTO status_logs (id_node, status_alert, location, sent_at, recorded_at)
VALUES ('NODE_B', 2, 'kitchen', '2025-05-12 11:15:05', '2025-05-12 11:15:10');
