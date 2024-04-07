#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <limits>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int width  = 800;
const int height = 800;
const int depth  = 255;

Model *model = NULL;
int *zbuffer = NULL;
Vec3f light_dir = Vec3f(1,-1,1).normalize();
Vec3f eye(0,0,2);
Vec3f center(0,0,0);

Matrix viewport(int x, int y, int w, int h) {
    Matrix m = Matrix::identity(4);
    m[0][3] = x+w/2.f;
    m[1][3] = y+h/2.f;
    m[2][3] = depth/2.f;

    m[0][0] = w/2.f;
    m[1][1] = h/2.f;
    m[2][2] = depth/2.f;
    return m;
}

Matrix rotationZ(float degrees) {
    float radians = degrees * M_PI / 180.0f; // Convert degrees to radians
    Matrix rot = Matrix::identity(4); // Start with an identity matrix

    rot[0][0] = cos(radians);
    rot[0][1] = -sin(radians);
    rot[1][0] = sin(radians);
    rot[1][1] = cos(radians);
    // rot[2][2] remains 1 as it is in the identity matrix, no change needed for Z-axis rotation
    // rot[3][3] remains 1 to preserve homogenous coordinates

    return rot;
    // For rotation around the X-axis, you would adjust indices [1][1], [1][2], [2][1], and [2][2].
    // For rotation around the Y-axis, adjust indices [0][0], [0][2], [2][0], and [2][2].
}

Matrix rotationY(float degrees) {
    float radians = degrees * M_PI / 180.0f; // Convert degrees to radians
    Matrix rot = Matrix::identity(4); // Start with an identity matrix

    rot[0][0] = cos(radians);
    rot[0][2] = -sin(radians);
    rot[2][0] = sin(radians);
    rot[2][2] = cos(radians);
    // rot[2][2] remains 1 as it is in the identity matrix, no change needed for Z-axis rotation
    // rot[3][3] remains 1 to preserve homogenous coordinates

    return rot;
    // For rotation around the X-axis, you would adjust indices [1][1], [1][2], [2][1], and [2][2].
    // For rotation around the Y-axis, adjust indices [0][0], [0][2], [2][0], and [2][2].
}

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f z = (eye-center).normalize();
    Vec3f x = (up^z).normalize();
    Vec3f y = (z^x).normalize();
    Matrix res = Matrix::identity(4);
    for (int i=0; i<3; i++) {
        res[0][i] = x[i];
        res[1][i] = y[i];
        res[2][i] = z[i];
        res[i][3] = -center[i];
    }
    return res;
}

void triangle(Vec3i t0, Vec3i t1, Vec3i t2, float ity0, float ity1, float ity2, Vec2i uv0, Vec2i uv1, Vec2i uv2, TGAImage &image, int *zbuffer) {
    if (t0.y==t1.y && t0.y==t2.y) return;
    if (t0.y>t1.y) { std::swap(t0, t1); std::swap(ity0, ity1); std::swap(uv0, uv1); }
    if (t0.y>t2.y) { std::swap(t0, t2); std::swap(ity0, ity2); std::swap(uv0, uv2); }
    if (t1.y>t2.y) { std::swap(t1, t2); std::swap(ity1, ity2); std::swap(uv1, uv2);  }

    int total_height = t2.y-t0.y;
    for (int i=0; i<total_height; i++) {
        bool second_half = i>t1.y-t0.y || t1.y==t0.y;
        int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y;
        float alpha = (float)i/total_height;
        float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height;
        Vec3i A    =               t0  + Vec3f(t2-t0  )*alpha;
        Vec3i B    = second_half ? t1  + Vec3f(t2-t1  )*beta : t0  + Vec3f(t1-t0  )*beta;
        float ityA =               ity0 +   (ity2-ity0)*alpha;
        float ityB = second_half ? ity1 +   (ity2-ity1)*beta : ity0 +   (ity1-ity0)*beta;
        Vec2i uvA =               uv0 +      (uv2-uv0)*alpha;
        Vec2i uvB = second_half ? uv1 +      (uv2-uv1)*beta : uv0 +      (uv1-uv0)*beta;
        if (A.x>B.x) { std::swap(A, B); std::swap(ityA, ityB); std::swap(uvA, uvB); }
        for (int j=A.x; j<=B.x; j++) {
            float phi = B.x==A.x ? 1. : (float)(j-A.x)/(B.x-A.x);
            Vec3i    P = Vec3f(A) +  Vec3f(B-A)*phi;
            Vec2i uvP =     uvA +   (uvB-uvA)*phi;
            float ityP =    ityA  + (ityB-ityA)*phi;
            ityP*=1.2;
            int idx = P.x+P.y*width;
            if (P.x>=width||P.y>=height||P.x<0||P.y<0) continue;
            if (zbuffer[idx]<P.z) {
                zbuffer[idx] = P.z;
                TGAColor color = model->diffuse(uvP);
                image.set(P.x, P.y, TGAColor(color.bgra[0]*ityP, color.bgra[1]*ityP, color.bgra[2]*ityP));
            }
        }
    }
}


int main(int argc, char** argv) {
    if (2 == argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("../obj/african_head/african_head.obj");
    }

    sf::RenderWindow window(sf::VideoMode(width, height), "Model Rotation");
    sf::Texture texture;
    texture.create(width, height);
    sf::Sprite sprite(texture);
    sprite.setPosition(0, 0);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear(sf::Color::Black);

        zbuffer = new int[width*height];

        for (float angle = 0; angle < 360; angle += 10) {

            TGAImage image(width, height, TGAImage::RGB);
            for (int i = 0; i < width*height; i++) {
                zbuffer[i] = std::numeric_limits<int>::min();
            }

            // Rotation around Y-axis
            Matrix ModelView  = lookat(eye, center, Vec3f(0,1,0));
            Matrix Projection = Matrix::identity(4);
            Projection[3][2] = -1.f / (eye - center).norm();
            Matrix ViewPort   = viewport(width/8, height/8, width*3/4, height*3/4);
            Matrix Rotation   = rotationY(angle);

            Matrix Transformation = ViewPort * Projection * ModelView * Rotation;

            // Draw the model with the current rotation
            // Assuming triangle() and other relevant functions are defined elsewhere
            for (int i = 0; i < model->nfaces(); i++) {
                // Similar loop to draw the model as before but using Transformation matrix
                std::vector<int> face = model->face(i);
                Vec3i screen_coords[3];
                Vec3f world_coords[3];
                float intensity[3];
                Vec2i uv[3];
                Vec3f current_light_dir = Vec3f(Transformation*Matrix(light_dir)).normalize();
                for (int j=0; j<3; j++) {
                    Vec3f v = model->vert(face[j]);
                    screen_coords[j] =  Vec3f(Transformation*Matrix(v));
                    world_coords[j]  = v;
                    intensity[j] = Vec3f(Transformation*Matrix(model->norm(i, j))).normalize()*light_dir;
                    uv[j] = model->uv(i, j);
                }
                triangle(screen_coords[0], screen_coords[1], screen_coords[2], intensity[0], intensity[1], intensity[2],  uv[0], uv[1], uv[2], image, zbuffer);
            }
            image.flip_vertically();

            // Convert TGAImage to SFML texture
            sf::Image sfImage;
            sfImage.create(width, height, sf::Color::Black);
            for (int x = 0; x < width; x++) {
                for (int y = 0; y < height; y++) {
                    TGAColor col = image.get(x, y);
                    sfImage.setPixel(x, y, sf::Color(col.bgra[2], col.bgra[1], col.bgra[0]));
                }
            }

            texture.update(sfImage);
            window.draw(sprite);
            window.display();

            std::this_thread::sleep_for(std::chrono::milliseconds (5)); // Sleep to slow down the rotation speed
        }

        delete[] zbuffer;
    }

    delete model;
    return 0;
}