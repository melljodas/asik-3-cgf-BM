#include <glew/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <ctime>

struct Point3D {
    float x, y, z;
    Point3D(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

class Texture2D {
private:
    GLuint textureID;
    int width, height;

public:
    Texture2D() : textureID(0), width(0), height(0) {}

    bool createProcedural(int w, int h) {
        width = w;
        height = h;

        
        std::vector<unsigned char> imageData(w * h * 3);

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int index = (y * w + x) * 3;

                
                float fx = (float)x / w * 8.0f;
                float fy = (float)y / h * 8.0f;

                
                float r = 0.5f + 0.5f * sin(fx * 2.0f + fy * 1.0f);
                float g = 0.5f + 0.5f * sin(fx * 1.5f + fy * 2.0f);
                float b = 0.5f + 0.5f * sin(fx * 1.0f + fy * 1.5f);

                
                float dx = (x - w / 2.0f) / (w / 2.0f);
                float dy = (y - h / 2.0f) / (h / 2.0f);
                float dist = sqrt(dx * dx + dy * dy);
                float radial = 1.0f - fmin(dist, 1.0f);

                r = r * 0.7f + radial * 0.3f;
                g = g * 0.7f + radial * 0.3f;
                b = b * 0.7f + radial * 0.3f;

                imageData[index] = (unsigned char)(r * 255);
                imageData[index + 1] = (unsigned char)(g * 255);
                imageData[index + 2] = (unsigned char)(b * 255);
            }
        }

        return uploadToGPU(imageData.data());
    }

    bool uploadToGPU(unsigned char* data) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture created: " << width << "x" << height << std::endl;
        return true;
    }

    void bind() {
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

    GLuint getID() const { return textureID; }
};

class BezierPatch {
private:
    std::vector<Point3D> controlPoints;
    std::vector<Point3D> vertices;
    std::vector<Point3D> normals;
    std::vector<float> texCoords;
    std::vector<int> indices;

    int resolution;

public:
    BezierPatch() : resolution(12) { // 12x12 как требуетс€
        initializeControlPoints();
    }

    void initializeControlPoints() {
        // 4x4 grid of control points for bicubic Bezier patch
        controlPoints = {
            // Row 0
            Point3D(-1.5f, 0.0f, -1.5f), Point3D(-0.5f, 2.0f, -1.5f), Point3D(0.5f, 2.0f, -1.5f), Point3D(1.5f, 0.0f, -1.5f),
            // Row 1
            Point3D(-1.5f, 1.0f, -0.5f), Point3D(-0.5f, 3.0f, -0.5f), Point3D(0.5f, 3.0f, -0.5f), Point3D(1.5f, 1.0f, -0.5f),
            // Row 2
            Point3D(-1.5f, 1.0f, 0.5f), Point3D(-0.5f, 3.0f, 0.5f), Point3D(0.5f, 3.0f, 0.5f), Point3D(1.5f, 1.0f, 0.5f),
            // Row 3
            Point3D(-1.5f, 0.0f, 1.5f), Point3D(-0.5f, 2.0f, 1.5f), Point3D(0.5f, 2.0f, 1.5f), Point3D(1.5f, 0.0f, 1.5f)
        };
        tessellate();
    }

    float bernstein(int i, int n, float t) {
        float binomial = 1.0f;
        for (int j = 1; j <= i; j++) {
            binomial *= float(n - j + 1) / float(j);
        }
        return binomial * powf(t, i) * powf(1 - t, n - i);
    }

    Point3D evaluate(float u, float v) {
        Point3D result(0, 0, 0);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float bu = bernstein(i, 3, u);
                float bv = bernstein(j, 3, v);
                int index = i * 4 + j;
                result.x += controlPoints[index].x * bu * bv;
                result.y += controlPoints[index].y * bu * bv;
                result.z += controlPoints[index].z * bu * bv;
            }
        }
        return result;
    }

    Point3D calculateNormal(float u, float v) {
        float du = 0.01f;
        float dv = 0.01f;

        Point3D p1 = evaluate(u, v);
        Point3D p2 = evaluate(u + du, v);
        Point3D p3 = evaluate(u, v + dv);

        Point3D du_vec = Point3D(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
        Point3D dv_vec = Point3D(p3.x - p1.x, p3.y - p1.y, p3.z - p1.z);

        Point3D normal;
        normal.x = du_vec.y * dv_vec.z - du_vec.z * dv_vec.y;
        normal.y = du_vec.z * dv_vec.x - du_vec.x * dv_vec.z;
        normal.z = du_vec.x * dv_vec.y - du_vec.y * dv_vec.x;

        float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }

        return normal;
    }

    void tessellate() {
        vertices.clear();
        normals.clear();
        texCoords.clear();
        indices.clear();

        // Iterative evaluation (not recursive)
        for (int i = 0; i <= resolution; i++) {
            float u = float(i) / resolution;
            for (int j = 0; j <= resolution; j++) {
                float v = float(j) / resolution;

                Point3D vertex = evaluate(u, v);
                Point3D normal = calculateNormal(u, v);

                vertices.push_back(vertex);
                normals.push_back(normal);
                // “екстурные координаты = (u,v) параметры
                texCoords.push_back(u);
                texCoords.push_back(v);
            }
        }

        // Create triangle indices for 12x12 grid
        for (int i = 0; i < resolution; i++) {
            for (int j = 0; j < resolution; j++) {
                int topLeft = i * (resolution + 1) + j;
                int topRight = topLeft + 1;
                int bottomLeft = (i + 1) * (resolution + 1) + j;
                int bottomRight = bottomLeft + 1;

                // First triangle
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                // Second triangle
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        std::cout << "Bezier patch tessellated: " << vertices.size() << " vertices, "
            << indices.size() / 3 << " triangles" << std::endl;
    }

    void render() {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);

        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < indices.size(); i++) {
            int idx = indices[i];
            Point3D normal = normals[idx];
            Point3D vertex = vertices[idx];
            float u = texCoords[idx * 2];
            float v = texCoords[idx * 2 + 1];

            glNormal3f(normal.x, normal.y, normal.z);
            glTexCoord2f(u, v);
            glVertex3f(vertex.x, vertex.y, vertex.z);
        }
        glEnd();

        glDisable(GL_TEXTURE_2D);
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

        gluLookAt(eyeX, eyeY, eyeZ, 0.0f, 1.5f, 0.0f, 0.0f, 1.0f, 0.0f);
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
BezierPatch patch;
Camera camera;
Texture2D texture;

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Light properties
    float lightAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    float lightDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    float lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lightPosition[] = { 5.0f, 5.0f, 5.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    // Material properties with specular highlight (as required)
    float materialAmbient[] = { 0.6f, 0.2f, 0.2f, 1.0f };
    float materialDiffuse[] = { 0.9f, 0.1f, 0.1f, 1.0f };
    float materialSpecular[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    float shininess = 80.0f;

    glMaterialfv(GL_FRONT, GL_AMBIENT, materialAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, materialDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, materialSpecular);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.apply();
    setupLighting();

    // Bind and enable texture
    texture.bind();
    glEnable(GL_TEXTURE_2D);

    // Draw the textured Bezier patch
    patch.render();

    glDisable(GL_TEXTURE_2D);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27: exit(0); break; // ESC
    case 'r': case 'R': camera.reset(); break;
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

    // Create procedural texture
    if (!texture.createProcedural(512, 512)) {
        std::cout << "Failed to create texture!" << std::endl;
    }

    std::cout << "Part 3.1: 2D Texture Mapping on Bezier Patch" << std::endl;
    std::cout << "Controls: Arrow keys to rotate, Page Up/Down to zoom, R to reset" << std::endl;
    std::cout << "Texture coordinates: (u,v) parameters used for texture mapping" << std::endl;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Assignment 4 - Part 3.1: 2D Texture Mapping on Bezier Patch");

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cout << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    glutMainLoop();
    return 0;
}