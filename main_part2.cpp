#include <glew/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <ctime>

struct Vertex {
    float x, y, z;
    Vertex(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Color {
    float r, g, b;
    Color(float r = 1.0f, float g = 1.0f, float b = 1.0f) : r(r), g(g), b(b) {}
};

class MeshObject {
private:
    std::vector<Vertex> vertices;
    std::vector<Vertex> normals;
    std::vector<int> indices;
    Color diffuseColor;
    int objectId;
    Vertex position;
    float scale;

public:
    MeshObject(const Vertex& pos, const Color& color, int id, float scale = 1.0f)
        : position(pos), diffuseColor(color), objectId(id), scale(scale) {
        std::cout << "Creating object " << id << " with color R=" << color.r << " G=" << color.g << " B=" << color.b << std::endl;
        generateGeometry();
    }

    void generateGeometry() {
        vertices.clear();
        normals.clear();
        indices.clear();

        float s = scale;
        vertices = {
            Vertex(-s, -s, -s), Vertex(s, -s, -s), Vertex(s, s, -s), Vertex(-s, s, -s),
            Vertex(-s, -s, s), Vertex(s, -s, s), Vertex(s, s, s), Vertex(-s, s, s)
        };

        normals = {
            Vertex(0, 0, -1), Vertex(0, 0, -1), Vertex(0, 0, -1), Vertex(0, 0, -1),
            Vertex(0, 0, 1), Vertex(0, 0, 1), Vertex(0, 0, 1), Vertex(0, 0, 1),
            Vertex(-1, 0, 0), Vertex(-1, 0, 0), Vertex(-1, 0, 0), Vertex(-1, 0, 0),
            Vertex(1, 0, 0), Vertex(1, 0, 0), Vertex(1, 0, 0), Vertex(1, 0, 0),
            Vertex(0, -1, 0), Vertex(0, -1, 0), Vertex(0, -1, 0), Vertex(0, -1, 0),
            Vertex(0, 1, 0), Vertex(0, 1, 0), Vertex(0, 1, 0), Vertex(0, 1, 0)
        };

        indices = {
            0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
            0, 3, 7, 7, 4, 0, 1, 2, 6, 6, 5, 1,
            0, 1, 5, 5, 4, 0, 3, 2, 6, 6, 7, 3
        };
    }

    void render(bool pickingMode = false) {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);

        if (pickingMode) {
            int r = (objectId & 0xFF0000) >> 16;
            int g = (objectId & 0x00FF00) >> 8;
            int b = (objectId & 0x0000FF);

            glColor3ub(r, g, b);
            glDisable(GL_LIGHTING);
        }
        else {
            glEnable(GL_LIGHTING);

            // ОТКЛЮЧАЕМ цветовой материал - используем только glMaterial
            glDisable(GL_COLOR_MATERIAL);

            // Устанавливаем материал с цветом объекта
            float ambient[] = { diffuseColor.r * 0.3f, diffuseColor.g * 0.3f, diffuseColor.b * 0.3f, 1.0f };
            float diffuse[] = { diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f };
            float specular[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            float shininess = 50.0f;

            glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
            glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
            glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
            glMaterialf(GL_FRONT, GL_SHININESS, shininess);

            // Отладочный вывод
            std::cout << "Rendering object " << objectId << " with material - "
                << "Ambient: " << ambient[0] << "," << ambient[1] << "," << ambient[2] << " | "
                << "Diffuse: " << diffuse[0] << "," << diffuse[1] << "," << diffuse[2] << std::endl;
        }

        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < indices.size(); i++) {
            int vertexIndex = indices[i] % 8;
            int normalIndex = indices[i] / 8 * 4 + (i % 6) / 3;

            if (!pickingMode) {
                glNormal3f(normals[normalIndex].x, normals[normalIndex].y, normals[normalIndex].z);
            }
            glVertex3f(vertices[vertexIndex].x, vertices[vertexIndex].y, vertices[vertexIndex].z);
        }
        glEnd();

        if (pickingMode) {
            glEnable(GL_LIGHTING);
        }

        glPopMatrix();
    }

    void setDiffuseColor(const Color& color) {
        std::cout << "Changing object " << objectId << " color from "
            << "R=" << diffuseColor.r << " G=" << diffuseColor.g << " B=" << diffuseColor.b
            << " to R=" << color.r << " G=" << color.g << " B=" << color.b << std::endl;
        diffuseColor = color;
    }

    int getObjectId() const {
        return objectId;
    }

    Color getDiffuseColor() const {
        return diffuseColor;
    }
};

class PickingSystem {
private:
    GLuint fbo;
    GLuint colorBuffer;
    GLuint depthBuffer;
    int windowWidth, windowHeight;

public:
    PickingSystem() : fbo(0), colorBuffer(0), depthBuffer(0), windowWidth(800), windowHeight(600) {}

    bool initialize() {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Color buffer
        glGenTextures(1, &colorBuffer);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);

        // Depth buffer
        glGenRenderbuffers(1, &depthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Framebuffer is not complete! Status: " << status << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::cout << "FBO initialized successfully!" << std::endl;
        return true;
    }

    int pickObject(int mouseX, int mouseY, const std::vector<MeshObject*>& objects) {
        // Сохраняем текущий FBO
        GLint oldFbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);

        // Рендерим в FBO для пикинга
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Очищаем черным цветом (ID = 0)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Настраиваем вьюпорт
        glViewport(0, 0, windowWidth, windowHeight);

        setupPickingView();

        // Рендерим объекты с цветами пикинга
        std::cout << "Rendering objects for picking:" << std::endl;
        for (MeshObject* obj : objects) {
            std::cout << "Object ID: " << obj->getObjectId() << std::endl;
            obj->render(true);
        }

        // Читаем пиксель
        unsigned char pixel[3];
        glReadPixels(mouseX, windowHeight - mouseY - 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

        // Восстанавливаем основной FBO
        glBindFramebuffer(GL_FRAMEBUFFER, oldFbo);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Восстанавливаем цвет очистки

        // Конвертируем в ID
        int clickedId = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];

        std::cout << "Mouse: " << mouseX << ", " << mouseY << " -> ID: " << clickedId
            << " (R:" << (int)pixel[0] << " G:" << (int)pixel[1] << " B:" << (int)pixel[2] << ")" << std::endl;

        // Ищем объект
        for (size_t i = 0; i < objects.size(); i++) {
            if (objects[i]->getObjectId() == clickedId && clickedId != 0) {
                std::cout << "Found object: " << i << std::endl;
                return static_cast<int>(i);
            }
        }

        std::cout << "No object found!" << std::endl;
        return -1;
    }

    void cleanup() {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (colorBuffer) glDeleteTextures(1, &colorBuffer);
        if (depthBuffer) glDeleteRenderbuffers(1, &depthBuffer);
    }

    void setWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
        cleanup();
        initialize();
    }

private:
    void setupPickingView() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, float(windowWidth) / windowHeight, 0.1f, 100.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float eyeX = 8.0f * cosf(45.0f * 3.14159f / 180.0f) * sinf(45.0f * 3.14159f / 180.0f);
        float eyeY = 8.0f * cosf(45.0f * 3.14159f / 180.0f);
        float eyeZ = 8.0f * sinf(45.0f * 3.14159f / 180.0f) * sinf(45.0f * 3.14159f / 180.0f);

        gluLookAt(eyeX, eyeY, eyeZ, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
};

class Camera {
private:
    float cameraDistance;
    float cameraAngleX, cameraAngleY;

public:
    Camera() : cameraDistance(8.0f), cameraAngleX(45.0f), cameraAngleY(45.0f) {}

    void apply() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, 800.0f / 600.0f, 0.1f, 100.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float radX = cameraAngleX * 3.14159f / 180.0f;
        float radY = cameraAngleY * 3.14159f / 180.0f;

        float eyeX = cameraDistance * cosf(radY) * sinf(radX);
        float eyeY = cameraDistance * cosf(radX);
        float eyeZ = cameraDistance * sinf(radY) * sinf(radX);

        gluLookAt(eyeX, eyeY, eyeZ, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }

    void rotate(float dx, float dy) {
        cameraAngleX += dy;
        cameraAngleY += dx;
    }

    void zoom(float delta) {
        cameraDistance += delta;
        cameraDistance = std::max(1.0f, std::min(20.0f, cameraDistance));
    }

    void reset() {
        cameraDistance = 8.0f;
        cameraAngleX = 45.0f;
        cameraAngleY = 45.0f;
    }
};

// Global variables
std::vector<MeshObject*> objects;
Camera camera;
PickingSystem picker;
bool antiAliasing = false;
int windowWidth = 800, windowHeight = 600;

Color randomColor() {
    static int colorIndex = 0;
    Color colors[] = {
        Color(1.0f, 0.0f, 0.0f), Color(0.0f, 1.0f, 0.0f), Color(0.0f, 0.0f, 1.0f),
        Color(1.0f, 1.0f, 0.0f), Color(1.0f, 0.0f, 1.0f), Color(0.0f, 1.0f, 1.0f),
        Color(1.0f, 0.5f, 0.0f), Color(0.5f, 0.0f, 1.0f), Color(1.0f, 0.8f, 0.2f),
        Color(0.2f, 0.8f, 0.2f), Color(0.8f, 0.2f, 0.8f), Color(0.2f, 0.8f, 0.8f)
    };

    Color result = colors[colorIndex % 12];
    colorIndex++;

    std::cout << "New color: R=" << result.r << " G=" << result.g << " B=" << result.b << std::endl;
    return result;
}

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    float lightAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    float lightDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    float lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lightPosition[] = { 5.0f, 5.0f, 5.0f, 1.0f }; // Статический свет, а не привязанный к камере

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    // ВАЖНО: Отключаем GL_COLOR_MATERIAL - он переопределяет материалы
    glDisable(GL_COLOR_MATERIAL);
}

void initializeObjects() {
    // Создаем объекты с РАЗНЫМИ начальными цветами
    objects.push_back(new MeshObject(Vertex(-2.0f, 0.0f, 0.0f), Color(1.0f, 0.0f, 0.0f), 0xFF0000, 0.8f)); // Красный
    objects.push_back(new MeshObject(Vertex(0.0f, 0.0f, 0.0f), Color(0.0f, 1.0f, 0.0f), 0x00FF00, 0.8f));  // Зеленый  
    objects.push_back(new MeshObject(Vertex(2.0f, 0.0f, 0.0f), Color(0.0f, 0.0f, 1.0f), 0x0000FF, 0.8f));  // Синий

    std::cout << "Objects initialized:" << std::endl;
    for (size_t i = 0; i < objects.size(); i++) {
        Color objColor = objects[i]->getDiffuseColor();
        std::cout << "Object " << i << " - ID: " << objects[i]->getObjectId()
            << " Color: R=" << objColor.r << " G=" << objColor.g << " B=" << objColor.b << std::endl;
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.apply();
    setupLighting();



    for (MeshObject* obj : objects) {
        obj->render(false);
    }

    // Display info
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, windowHeight, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1, 1, 1);
    std::string info = "Click objects to change color | Anti-aliasing: " + std::string(antiAliasing ? "ON" : "OFF");

    glRasterPos2f(10, 20);
    for (char c : info) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
    picker.setWindowSize(w, h);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        std::cout << "\n=== Mouse Click ===" << std::endl;
        int pickedObject = picker.pickObject(x, y, objects);
        if (pickedObject != -1) {
            objects[pickedObject]->setDiffuseColor(randomColor());
            std::cout << ">>> Object " << pickedObject << " clicked! Color changed." << std::endl;
            glutPostRedisplay();
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27: exit(0); break; // ESC
    case 'r': case 'R': camera.reset(); break;
    case 's': case 'S':
        antiAliasing = !antiAliasing;
        if (antiAliasing) {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_POLYGON_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
            glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        }
        else {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_POLYGON_SMOOTH);
        }
        break;
    }

    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_LEFT: camera.rotate(-5, 0); break;
    case GLUT_KEY_RIGHT: camera.rotate(5, 0); break;
    case GLUT_KEY_UP: camera.rotate(0, -5); break;
    case GLUT_KEY_DOWN: camera.rotate(0, 5); break;
    case GLUT_KEY_PAGE_UP: camera.zoom(-0.5f); break;
    case GLUT_KEY_PAGE_DOWN: camera.zoom(0.5f); break;
    }
    glutPostRedisplay();
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_NORMALIZE);

    srand(static_cast<unsigned int>(time(NULL)));
    initializeObjects();

    if (!picker.initialize()) {
        std::cout << "Failed to initialize picking system!" << std::endl;
    }
}

void cleanup() {
    for (MeshObject* obj : objects) {
        delete obj;
    }
    objects.clear();
    picker.cleanup();
}

void printControls() {
    std::cout << "\n=== Part 2: Anti-aliasing and Picking Controls ===" << std::endl;
    std::cout << "Mouse Click: Select object (changes color)" << std::endl;
    std::cout << "Arrow keys: Rotate camera" << std::endl;
    std::cout << "Page Up/Down: Zoom in/out" << std::endl;
    std::cout << "R: Reset view" << std::endl;
    std::cout << "S: Toggle anti-aliasing" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Assignment 4 - Part 2: Anti-aliasing and Picking");

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cout << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    if (!GLEW_EXT_framebuffer_object) {
        std::cout << "FBO not supported!" << std::endl;
        return 1;
    }

    init();
    printControls();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    glutMainLoop();
    cleanup();
    return 0;
}