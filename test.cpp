#include <GLFW/glfw3.h>
#include <iostream>
#define STB_TRUETYPE_IMPLEMENTATION
#include "includes/stb_truetype.hpp"

// Window dimensions
const int W_WIDTH = 800;
const int W_HEIGHT = 100;

// Font texture ID
GLuint fontTexture;
GLFWwindow* window;

// stb_truetype font data
stbtt_bakedchar cdata[96]; // ASCII 32..126
unsigned char ttf_buffer[1 << 20]; // Load font data
unsigned char bitmap[512 * 512 * 4]; // RGBA Font bitmap

stbtt_fontinfo font;

// OpenGL initialization and text rendering
void renderText(const char* text, float x, float y) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    float startX = x;
    float startY = y - (height - 512 - 20);

    while (*text) {
        if (*text >= 32) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &startX, &startY, &q, 1);


            q.y0 = 512 - q.y0;
            q.y1 = 512 - q.y1;

            glBegin(GL_QUADS);
            glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y0);
            glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y0);
            glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y1);
            glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y1);
            glEnd();
        }
        ++text;
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create GLFW window
    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, "GLFW Text Rendering", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Load font
    FILE* f = fopen("textures/TAHOMA.TTF", "rb");
    if (!f) {
        std::cerr << "Failed to load font" << std::endl;
        return -1;
    }
    fread(ttf_buffer, 1, 1 << 20, f);
    fclose(f);

    stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
    unsigned char grayscale_bitmap[512 * 512];
    stbtt_BakeFontBitmap(ttf_buffer, 0, 20.0, grayscale_bitmap, 512, 512, 32, 96, cdata);

    // Convert grayscale bitmap to RGBA
    memset(bitmap, 0, sizeof(bitmap)); // Initialize to transparent
    for (int i = 0; i < 512 * 512; ++i) {
        unsigned char intensity = grayscale_bitmap[i];
        bitmap[i * 4 + 0] = intensity; // R
        bitmap[i * 4 + 1] = intensity; // G
        bitmap[i * 4 + 2] = intensity; // B
        bitmap[i * 4 + 3] = intensity; // A
    }

    // Create texture for font bitmap
    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Set texture environment mode to modulate color with texture
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // OpenGL settings
    glEnable(GL_TEXTURE_2D);
    glViewport(0, 0, W_WIDTH, W_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, W_WIDTH, 0.0, W_HEIGHT, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glColor4f(0.0f, 0.0f, 1.0f, 1.0f); // Red text
        renderText("Nique", 0.0f, 0.0f);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Green text
        renderText("Ma", 0.0f, 20.0f);

        glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // Green text
        renderText("Mere", 0.0f, 40.0f);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}