-- 1. Dueños de casa (adultos mayores)
CREATE TABLE IF NOT EXISTS house_owner (
    id           INT AUTO_INCREMENT PRIMARY KEY,
    first_name   VARCHAR(50)     NOT NULL,
    last_name    VARCHAR(50)     NOT NULL,
    phone        VARCHAR(20)     NOT NULL,
    address      VARCHAR(150)    NOT NULL,
    created_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 2. Nodos de dispositivo, cada uno ligado opcionalmente a un owner
CREATE TABLE IF NOT EXISTS nodes (
    id           INT AUTO_INCREMENT PRIMARY KEY,
    id_node      VARCHAR(100)    NOT NULL UNIQUE,   -- identificador MQTT
    owner_id     INT               NULL,             -- referencia a casa
    location     ENUM('front_door','living_room','kitchen','other') NOT NULL,
    description  VARCHAR(200)      NULL,
    created_at   DATETIME          NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (owner_id) REFERENCES house_owner(id) ON DELETE SET NULL
);

-- 3. Usuarios de la app (admin y contactos de emergencia)
CREATE TABLE IF NOT EXISTS app_user (
    id             INT AUTO_INCREMENT PRIMARY KEY,
    username       VARCHAR(50)     NOT NULL UNIQUE,
    password_hash  VARCHAR(255)    NOT NULL,
    role           ENUM('admin','emergency_contact') NOT NULL,
    first_name     VARCHAR(50)     NULL,
    last_name      VARCHAR(50)     NULL,
    email          VARCHAR(100)    NULL,
    phone          VARCHAR(20)     NULL,
    created_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 4. Relación N a N entre propietarios y contactos de emergencia
CREATE TABLE IF NOT EXISTS owner_contact (
    owner_id  INT NOT NULL,
    user_id   INT NOT NULL,
    PRIMARY KEY (owner_id, user_id),
    FOREIGN KEY (owner_id) REFERENCES house_owner(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)  REFERENCES app_user(id)    ON DELETE CASCADE
);

-- 5. Datos enviados por ESP32: sala (living_room)
CREATE TABLE IF NOT EXISTS living_room (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)    NOT NULL,               -- referencia a nodes.id_node
    presence      TINYINT(1)      NOT NULL,               -- 0/1
    lights        TINYINT(1)      NOT NULL,               -- 0/1
    manual_on     TINYINT(1)      NOT NULL,               -- 0/1
    manual_off    TINYINT(1)      NOT NULL,               -- 0/1
    auto          TINYINT(1)      NOT NULL,               -- 0/1
    remote_on     TINYINT(1)      NOT NULL,               -- 0/1
    remote_off    TINYINT(1)      NOT NULL,               -- 0/1
    sent_at       DATETIME        NOT NULL,               -- timestamp del ESP32
    recorded_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- 6. Acciones recibidas por ESP32: sala (living_room_actions)
CREATE TABLE IF NOT EXISTS living_room_actions (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)    NOT NULL,               -- referencia a nodes.id_node
    turn_off      TINYINT(1)      NOT NULL,               -- 0/1
    action_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- 7. Datos enviados por ESP32: puerta principal (front_door)
CREATE TABLE IF NOT EXISTS front_door (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    id_node         VARCHAR(100)    NOT NULL,
    door            TINYINT(1)      NOT NULL,             -- 0=cerrada,1=abierta
    buzzer          TINYINT(1)      NOT NULL,             -- 0/1
    identification  TINYINT(1)      NOT NULL,             -- 0/1
    sent_at         DATETIME        NOT NULL,
    recorded_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- 8. Datos enviados por ESP32: cocina (kitchen)
CREATE TABLE IF NOT EXISTS kitchen (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)    NOT NULL,
    temperature   FLOAT           NOT NULL,
    humidity      FLOAT           NOT NULL,
    gas           INT             NOT NULL,
    sent_at       DATETIME        NOT NULL,
    recorded_at   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- 9. Acciones recibidas por ESP32: cocina (kitchen_actions)
CREATE TABLE IF NOT EXISTS kitchen_actions (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    id_node       VARCHAR(100)    NOT NULL,
    servo         TINYINT(1)      NOT NULL,               -- 0/1
    action_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_node) REFERENCES nodes(id_node) ON DELETE CASCADE
);

-- 10. Inserción de nodos iniciales
INSERT INTO nodes (id_node, owner_id, location, description) VALUES
    ('NODE_A',  NULL, 'front_door',  'Sensor de puerta frontal'),
    ('NODE_A1', NULL, 'living_room', 'Sensor de presencia en sala'),
    ('NODE_B',  NULL, 'kitchen',     'Sensor de ambiente en cocina');

-- 11. Nuevo propietario vinculado a todos los nodos
INSERT INTO house_owner (first_name, last_name, phone, address)
VALUES ('Carlos', 'Pérez', '555-0000', '789 Oak St.');

-- Capturar el ID del nuevo owner
SET @new_owner_id = LAST_INSERT_ID();

-- 12. Nuevo usuario de la app (contacto de emergencia) vinculado al nuevo owner
INSERT INTO app_user (username, password_hash, role, first_name, last_name, email, phone)
VALUES ('jperez_user', '$2b$12$hashDeEjemplo3', 'emergency_contact', 'Juanito', 'Pérez', 'Juanito.perez@example.com', '555-0000');

-- Capturar el ID del nuevo app_user
SET @new_user_id = LAST_INSERT_ID();

-- 13. Relacionar el nuevo owner con el nuevo app_user
INSERT INTO owner_contact (owner_id, user_id)
VALUES (@new_owner_id, @new_user_id);

-- 14. Asignar el nuevo owner a todos los nodos existentes
UPDATE nodes
SET owner_id = @new_owner_id
WHERE id_node IN ('NODE_A', 'NODE_A1', 'NODE_B');
