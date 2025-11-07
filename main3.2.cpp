#include <glew/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Структура для вершины
struct Vertex {
    float x, y, z;
    float nx, ny, nz;
};

// Структура для треугольника
struct Triangle {
    int v1, v2, v3;
};

class Model {
public:
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;

    bool loadSMF(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << filename << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") {
                Vertex v;
                iss >> v.x >> v.y >> v.z;
                v.nx = v.ny = v.nz = 0.0f;
                vertices.push_back(v);
            }
            else if (prefix == "f") {
                Triangle t;
                iss >> t.v1 >> t.v2 >> t.v3;
                // SMF использует 1-индексацию
                t.v1--; t.v2--; t.v3--;
                triangles.push_back(t);
            }
        }

        calculateNormals();
        return true;
    }

private:
    void calculateNormals() {
        // Инициализируем нормали нулями
        for (auto& v : vertices) {
            v.nx = v.ny = v.nz = 0.0f;
        }

        // Вычисляем нормали для каждого треугольника
        for (const auto& tri : triangles) {
            glm::vec3 v1(vertices[tri.v1].x, vertices[tri.v1].y, vertices[tri.v1].z);
            glm::vec3 v2(vertices[tri.v2].x, vertices[tri.v2].y, vertices[tri.v2].z);
            glm::vec3 v3(vertices[tri.v3].x, vertices[tri.v3].y, vertices[tri.v3].z);

            glm::vec3 edge1 = v2 - v1;
            glm::vec3 edge2 = v3 - v1;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            // Добавляем нормаль к вершинам треугольника
            vertices[tri.v1].nx += normal.x;
            vertices[tri.v1].ny += normal.y;
            vertices[tri.v1].nz += normal.z;

            vertices[tri.v2].nx += normal.x;
            vertices[tri.v2].ny += normal.y;
            vertices[tri.v2].nz += normal.z;

            vertices[tri.v3].nx += normal.x;
            vertices[tri.v3].ny += normal.y;
            vertices[tri.v3].nz += normal.z;
        }

        // Нормализуем нормали
        for (auto& v : vertices) {
            glm::vec3 normal(v.nx, v.ny, v.nz);
            if (glm::length(normal) > 0.0f) {
                normal = glm::normalize(normal);
                v.nx = normal.x;
                v.ny = normal.y;
                v.nz = normal.z;
            }
        }
    }
};

class Shader {
public:
    GLuint program;

    Shader(const char* vertexSource, const char* fragmentSource) {
        // Компиляция вершинного шейдера
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        // Компиляция фрагментного шейдера
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        // Линковка шейдеров
        program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        checkCompileErrors(program, "PROGRAM");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void use() {
        glUseProgram(program);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) {
        glUniformMatrix4fv(glGetUniformLocation(program, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setVec3(const std::string& name, const glm::vec3& value) {
        glUniform3fv(glGetUniformLocation(program, name.c_str()), 1, glm::value_ptr(value));
    }

    void setFloat(const std::string& name, float value) {
        glUniform1f(glGetUniformLocation(program, name.c_str()), value);
    }

private:
    void checkCompileErrors(GLuint shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};

// Глобальные переменные
Model model;
Shader* shader = nullptr;
GLuint VAO, VBO, EBO;

glm::mat4 modelMatrix = glm::mat4(1.0f);
glm::mat4 viewMatrix;
glm::mat4 projectionMatrix;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

glm::vec3 lightPos = glm::vec3(2.0f, 2.0f, 2.0f);
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

float lastX = 400, lastY = 300;
float yaw = -90.0f, pitch = 0.0f;
bool firstMouse = true;

// Исходный код шейдеров
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

// Функция для генерации мраморной текстуры
vec3 marbleTexture(vec3 pos) {
    float scale = 5.0;
    float turbulence = 10.0;
    float period = 1.5;

    // Базовый цвет мрамора
    vec3 baseColor = vec3(0.8, 0.8, 0.9); // Светло-серый с голубым оттенком
    vec3 veinColor = vec3(0.3, 0.2, 0.1);  // Тёмные прожилки

    // Используем синусоидальную функцию для создания прожилок
    float noise = sin((pos.x + pos.y + pos.z) * scale + sin(pos.y * turbulence) * period);
    noise = (noise + 1.0) * 0.5; // Приводим к диапазону [0, 1]

    // Смешиваем базовый цвет и цвет прожилок
    return mix(baseColor, veinColor, noise);
}

void main() {
    
    vec3 textureColor = marbleTexture(FragPos);

    
    vec3 norm = normalize(Normal);
    
    
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * lightColor;
    
    
    vec3 result = (diffuse + 0.3 * specular) * textureColor;
    FragColor = vec4(result, 1.0);
}
)";

void init() {
    glEnable(GL_DEPTH_TEST);

    
    if (!model.loadSMF("D:/For CGF/bunny_69k.smf")) {
        std::cout << "Using default cube model..." << std::endl;
        
        model.vertices = {
            
            {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
            
            {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
            
            {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, -1.0f},
            
            {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f},
            
            {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
            
            {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f}
        };

        model.triangles = {
            
            {0, 1, 2}, {2, 3, 0},
            
            {4, 5, 6}, {6, 7, 4},
            
            {8, 9, 10}, {10, 11, 8},
            
            {12, 13, 14}, {14, 15, 12},
            
            {16, 17, 18}, {18, 19, 16},
            
            {20, 21, 22}, {22, 23, 20}
        };

        
        for (auto& tri : model.triangles) {
            glm::vec3 v1(model.vertices[tri.v1].x, model.vertices[tri.v1].y, model.vertices[tri.v1].z);
            glm::vec3 v2(model.vertices[tri.v2].x, model.vertices[tri.v2].y, model.vertices[tri.v2].z);
            glm::vec3 v3(model.vertices[tri.v3].x, model.vertices[tri.v3].y, model.vertices[tri.v3].z);

            glm::vec3 edge1 = v2 - v1;
            glm::vec3 edge2 = v3 - v1;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            model.vertices[tri.v1].nx = normal.x; model.vertices[tri.v1].ny = normal.y; model.vertices[tri.v1].nz = normal.z;
            model.vertices[tri.v2].nx = normal.x; model.vertices[tri.v2].ny = normal.y; model.vertices[tri.v2].nz = normal.z;
            model.vertices[tri.v3].nx = normal.x; model.vertices[tri.v3].ny = normal.y; model.vertices[tri.v3].nz = normal.z;
        }
    }

    
    shader = new Shader(vertexShaderSource, fragmentShaderSource);

    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(Vertex), model.vertices.data(), GL_STATIC_DRAW);

    
    std::vector<unsigned int> indices;
    for (const auto& tri : model.triangles) {
        indices.push_back(tri.v1);
        indices.push_back(tri.v2);
        indices.push_back(tri.v3);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    
    viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    projectionMatrix = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
}

void display() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->use();

    
    viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("model", modelMatrix);

    
    shader->setVec3("lightPos", lightPos);
    shader->setVec3("viewPos", cameraPos);
    shader->setVec3("lightColor", lightColor);

    
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, model.triangles.size() * 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glutSwapBuffers();
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    projectionMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
}

void mouseMotion(int x, int y) {
    if (firstMouse) {
        lastX = x;
        lastY = y;
        firstMouse = false;
    }

    float xoffset = x - lastX;
    float yoffset = lastY - y;
    lastX = x;
    lastY = y;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    float cameraSpeed = 0.1f;
    switch (key) {
    case 'w':
        cameraPos += cameraSpeed * cameraFront;
        break;
    case 's':
        cameraPos -= cameraSpeed * cameraFront;
        break;
    case 'a':
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        break;
    case 'd':
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        break;
    case 'r': 
        cameraPos = glm::vec3(0.0f, 0.0f, 5.0f);
        cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        yaw = -90.0f;
        pitch = 0.0f;
        break;
    case 27:
        exit(0);
        break;
    }
    glutPostRedisplay();
}

void cleanup() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    delete shader;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("3D Procedural Texture - Marble Effect");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutPassiveMotionFunc(mouseMotion);
    glutKeyboardFunc(keyboard);

    std::cout << "Controls:" << std::endl;
    std::cout << "WASD - Move camera" << std::endl;
    std::cout << "Mouse - Look around" << std::endl;
    std::cout << "R - Reset view" << std::endl;
    std::cout << "ESC - Exit" << std::endl;

    glutMainLoop();

    cleanup();
    return 0;
}