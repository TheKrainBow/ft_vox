// Skybox.cpp
#include "Skybox.hpp"
#include "ft_vox.hpp"

Skybox::Skybox() {
}

Skybox::~Skybox() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
}

bool Skybox::loadFromPPM(const char* filename, int faceWidth, int faceHeight) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    // Load PPM file
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    // Skip PPM header (P6\n)
    char header[3];
    if (!fgets(header, sizeof(header), file)) {
        fclose(file);
        return false;
    }
    
    // Read dimensions (assuming they're correct for T-shape layout)
    int width, height;
    if (fscanf(file, "%d %d\n", &width, &height) != 2) {
        fclose(file);
        return false;
    }
    
    // Skip max color value
    int maxColor;
    if (fscanf(file, "%d\n", &maxColor) != 1) {
        fclose(file);
        return false;
    }
    
    // Read pixel data
    std::vector<unsigned char> buffer(width * height * 3);
    size_t bytesRead = fread(buffer.data(), 1, buffer.size(), file);
    if (bytesRead != buffer.size()) {
        fclose(file);
        return false;
    }
    fclose(file);
    
    // Function to extract face data from the buffer
    auto extractFace = [&](int startX, int startY, bool flipVertical = false) {
        std::vector<unsigned char> faceData(faceWidth * faceHeight * 3);
        for (int y = 0; y < faceHeight; y++) {
            for (int x = 0; x < faceWidth; x++) {
                int srcY = flipVertical ? (faceHeight - 1 - y) : y;
                int srcIndex = ((startY + srcY) * width + startX + x) * 3;
                int destIndex = (y * faceWidth + x) * 3;
                
                faceData[destIndex] = buffer[srcIndex];       // R
                faceData[destIndex + 1] = buffer[srcIndex + 1]; // G
                faceData[destIndex + 2] = buffer[srcIndex + 2]; // B
            }
        }
        return faceData;
    };
    
    // Calculate face positions in the T-layout
    // Assuming the width is 4 face widths and height is 3 face heights
    
    // Load each face into the cube map in the correct order
    // Right face (GL_TEXTURE_CUBE_MAP_POSITIVE_X)
    auto rightFace = extractFace(2 * faceWidth, faceHeight);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, rightFace.data());
    
    // Left face (GL_TEXTURE_CUBE_MAP_NEGATIVE_X)
    auto leftFace = extractFace(0, faceHeight);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, leftFace.data());
    
    // Top face (GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
    auto topFace = extractFace(faceWidth, 0, true);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, topFace.data());
    
    // Bottom face (GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
    auto bottomFace = extractFace(faceWidth, 2 * faceHeight);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, bottomFace.data());
    
    // Front face (GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    auto frontFace = extractFace(faceWidth, faceHeight);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, frontFace.data());
    
    // Back face (GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
    auto backFace = extractFace(3 * faceWidth, faceHeight);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, faceWidth, faceHeight, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, backFace.data());
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    return true;
}

void Skybox::render() {
    glDepthFunc(GL_LEQUAL);
    
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    glDepthFunc(GL_LESS);
}

GLuint Skybox::getTextureID()
{
	return textureID;
}