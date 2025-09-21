// Skybox.cpp
#include "Skybox.hpp"
#include "ft_vox.hpp"
#include "stb_image.h"
#include <fstream>
#include <cmath>
#include <cstring>

// (PPM/XPM loaders removed; using PNG equirectangular only)

Skybox::Skybox() : textureID(0), VAO(0), VBO(0) {}

Skybox::~Skybox() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (textureID) glDeleteTextures(1, &textureID);
}

// (PPM loader removed)

void Skybox::render() {
    // Lazy-create VAO/VBO after GL is ready
    if (VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    // Skybox as background: don't write depth, but test so terrain can occlude
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

GLuint Skybox::getTextureID()
{
    return textureID;
}

bool Skybox::loadFromPNG(const std::array<std::string, 6>& faces, bool fixSeams)
{
    struct Img { int w=0,h=0,c=0; std::vector<unsigned char> rgba; } imgs[6];

    // Load and convert to RGBA
    for (int i=0;i<6;++i) {
        int w,h,n; stbi_uc* d = stbi_load(faces[i].c_str(), &w, &h, &n, 0);
        if (!d) { std::cerr << "[Skybox] PNG load failed: " << faces[i] << std::endl; return false; }
        imgs[i].w=w; imgs[i].h=h; imgs[i].c=4;
        imgs[i].rgba.resize((size_t)w*h*4);
        // Convert to RGBA
        if (n==4) {
            std::memcpy(imgs[i].rgba.data(), d, (size_t)w*h*4);
        } else if (n==3) {
            for (int p=0;p<w*h;++p){ imgs[i].rgba[p*4+0]=d[p*3+0]; imgs[i].rgba[p*4+1]=d[p*3+1]; imgs[i].rgba[p*4+2]=d[p*3+2]; imgs[i].rgba[p*4+3]=255; }
        } else if (n==1) {
            for (int p=0;p<w*h;++p){ unsigned char v=d[p]; imgs[i].rgba[p*4+0]=v; imgs[i].rgba[p*4+1]=v; imgs[i].rgba[p*4+2]=v; imgs[i].rgba[p*4+3]=255; }
        } else {
            stbi_image_free(d); std::cerr << "[Skybox] Unsupported channels in "<<faces[i]<<" :"<<n<<std::endl; return false;
        }
        stbi_image_free(d);
        if (i>0 && (imgs[i].w!=imgs[0].w || imgs[i].h!=imgs[0].h)) {
            std::cerr << "[Skybox] Face size mismatch in PNG set" << std::endl; return false;
        }
    }

    // Optional simple seam fix: average outermost rows/cols between opposite faces where orientation matches.
    // This is conservative and assumes images are correctly oriented; if not, edges may still show.
    if (fixSeams) {
        int BW = 0;
        auto avgRows = [&](Img& A, Img& B, bool topA_vs_bottomB){
            int w=A.w, h=A.h; int yA0 = topA_vs_bottomB?0:(h-1); int yB0 = topA_vs_bottomB?(B.h-1):0;
            for (int k=0;k<BW;++k){ int yA = topA_vs_bottomB? (yA0 + k) : (yA0 - k); int yB = topA_vs_bottomB? (yB0 - k) : (yB0 + k);
                for (int x=0;x<w;++x){ unsigned char* pa = &A.rgba[(yA*w + x)*4]; unsigned char* pb = &B.rgba[(yB*w + x)*4]; for(int c=0;c<3;++c){ unsigned int m=pa[c]+pb[c]; pa[c]=pb[c]=(unsigned char)(m/2);} }
            }
        };
        auto avgCols = [&](Img& A, Img& B, bool leftA_vs_rightB){
            int w=A.w, h=A.h; int xA0 = leftA_vs_rightB?0:(w-1); int xB0 = leftA_vs_rightB?(B.w-1):0;
            for (int k=0;k<BW;++k){ int xA = leftA_vs_rightB? (xA0 + k) : (xA0 - k); int xB = leftA_vs_rightB? (xB0 - k) : (xB0 + k);
                for (int y=0;y<h;++y){ unsigned char* pa = &A.rgba[(y*w + xA)*4]; unsigned char* pb = &B.rgba[(y*w + xB)*4]; for(int c=0;c<3;++c){ unsigned int m=pa[c]+pb[c]; pa[c]=pb[c]=(unsigned char)(m/2);} }
            }
        };
        // Adjacency (assuming standard orientation). Adjust if your images differ.
        // +X(0) with +Y(2) top edge, -Y(3) bottom; +Z(4) left, -Z(5) right
        avgRows(imgs[0], imgs[2], true ); // +X top with +Y bottom
        avgRows(imgs[0], imgs[3], false); // +X bottom with -Y top
        avgCols(imgs[0], imgs[4], true ); // +X left with +Z right
        avgCols(imgs[0], imgs[5], false); // +X right with -Z left
        // -X(1)
        avgRows(imgs[1], imgs[2], true );
        avgRows(imgs[1], imgs[3], false);
        avgCols(imgs[1], imgs[5], true );
        avgCols(imgs[1], imgs[4], false);
        // +Z(4) with +Y top, -Y bottom; -X left, +X right
        avgRows(imgs[4], imgs[2], true );
        avgRows(imgs[4], imgs[3], false);
        avgCols(imgs[4], imgs[1], true );
        avgCols(imgs[4], imgs[0], false);
        // -Z(5)
        avgRows(imgs[5], imgs[2], true );
        avgRows(imgs[5], imgs[3], false);
        avgCols(imgs[5], imgs[0], true );
        avgCols(imgs[5], imgs[1], false);
        // +Y(2) with -Z top, +Z bottom; -X left, +X right (heuristic)
        avgRows(imgs[2], imgs[5], true );
        avgRows(imgs[2], imgs[4], false);
        avgCols(imgs[2], imgs[1], true );
        avgCols(imgs[2], imgs[0], false);
        // -Y(3) with +Z top, -Z bottom; -X left, +X right
        avgRows(imgs[3], imgs[4], true );
        avgRows(imgs[3], imgs[5], false);
        avgCols(imgs[3], imgs[1], true );
        avgCols(imgs[3], imgs[0], false);
    }

    if (!textureID) glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    GLenum targets[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    for (int i=0;i<6;++i) {
        glTexImage2D(targets[i], 0, GL_RGBA, imgs[i].w, imgs[i].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, imgs[i].rgba.data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, fixSeams ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if (!fixSeams) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}
// (XPM loaders removed)

static inline void sampleEquirectBilinear(const unsigned char* src, int w, int h, int ch,
                                          float u, float v, unsigned char* outRGBA)
{
    // Wrap u horizontally [0,1), clamp v vertically [0,1]
    u = u - std::floor(u);
    v = std::clamp(v, 0.0f, 1.0f);
    float x = u * (w - 1);
    float y = v * (h - 1);
    int x0 = (int)std::floor(x);
    int y0 = (int)std::floor(y);
    int x1 = std::min(x0 + 1, w - 1);
    int y1 = std::min(y0 + 1, h - 1);
    float tx = x - x0;
    float ty = y - y0;
    auto pix = [&](int xi, int yi, int c){ return src[(yi * w + xi) * ch + c]; };
    for (int c = 0; c < 4; ++c) {
        int ci = (c < ch) ? c : (ch - 1); // if RGB, duplicate B into A then set A below
        float p00 = pix(x0, y0, std::min(ci, ch-1));
        float p10 = pix(x1, y0, std::min(ci, ch-1));
        float p01 = pix(x0, y1, std::min(ci, ch-1));
        float p11 = pix(x1, y1, std::min(ci, ch-1));
        float p0 = p00 * (1 - tx) + p10 * tx;
        float p1 = p01 * (1 - tx) + p11 * tx;
        float pr = p0 * (1 - ty) + p1 * ty;
        outRGBA[c] = (unsigned char)std::clamp(pr, 0.0f, 255.0f);
    }
    outRGBA[3] = 255;
}

bool Skybox::loadFromEquirectPNG(const char* filename, int faceSize)
{
    int w=0,h=0,n=0;
    stbi_uc* data = stbi_load(filename, &w, &h, &n, 0);
    if (!data) {
        std::cerr << "[Skybox] Failed to load PNG: " << filename << std::endl;
        return false;
    }
    if (w <= 0 || h <= 0 || (n != 3 && n != 4)) {
        std::cerr << "[Skybox] Unsupported PNG format: " << filename << std::endl;
        stbi_image_free(data);
        return false;
    }
    if (faceSize <= 0) {
        // Heuristic: use min(w/4, h/2)
        faceSize = std::max(16, std::min(w / 4, h / 2));
    }

    if (!textureID) glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    auto faceDir = [&](int face, float a, float b){
        // a,b in [-1,1]
        glm::vec3 d(0.0f);
        switch (face) {
            case 0: d = glm::normalize(glm::vec3( 1.0f,   -b,   -a)); break; // +X
            case 1: d = glm::normalize(glm::vec3(-1.0f,   -b,    a)); break; // -X
            case 2: d = glm::normalize(glm::vec3(  a,  1.0f,    b)); break; // +Y
            case 3: d = glm::normalize(glm::vec3(  a, -1.0f,   -b)); break; // -Y
            case 4: d = glm::normalize(glm::vec3(  a,   -b,  1.0f)); break; // +Z
            case 5: d = glm::normalize(glm::vec3( -a,   -b, -1.0f)); break; // -Z
        }
        return d;
    };

    std::vector<unsigned char> face(faceSize * faceSize * 4);
    for (int f = 0; f < 6; ++f) {
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                float a = 2.0f * ((x + 0.5f) / faceSize) - 1.0f;
                float b = 2.0f * ((y + 0.5f) / faceSize) - 1.0f;
                glm::vec3 d = faceDir(f, a, b);
                // Equirectangular mapping
                float u = (std::atan2(d.z, d.x) + (float)M_PI) / (2.0f * (float)M_PI);
                float v = std::acos(std::clamp(d.y, -1.0f, 1.0f)) / (float)M_PI;
                sampleEquirectBilinear(data, w, h, n, u, v, &face[(y * faceSize + x) * 4]);
            }
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA, faceSize, faceSize, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, face.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    stbi_image_free(data);
    std::cerr << "[Skybox] Built cubemap from equirect PNG: " << filename
              << " faceSize=" << faceSize << std::endl;
    return true;
}

// Rotate an interleaved RGBA square image 90 degrees clockwise
// static void rotateRGBA90CW(std::vector<unsigned char>& img, int size)
// {
//     if (img.empty() || size <= 0) return;
//     std::vector<unsigned char> out((size_t)size * size * 4);
//     for (int y = 0; y < size; ++y) {
//         for (int x = 0; x < size; ++x) {
//             int src = (y * size + x) * 4;
//             int rx = size - 1 - y;
//             int ry = x;
//             int dst = (ry * size + rx) * 4;
//             out[dst + 0] = img[src + 0];
//             out[dst + 1] = img[src + 1];
//             out[dst + 2] = img[src + 2];
//             out[dst + 3] = img[src + 3];
//         }
//     }
//     img.swap(out);
// }

bool Skybox::loadFromSinglePNG(const char* filename, bool fixSeams)
{
    int W=0,H=0,N=0;
    stbi_uc* data = stbi_load(filename, &W, &H, &N, 0);
    if (!data) { std::cerr << "[Skybox] Failed to open single PNG: " << filename << std::endl; return false; }
    if (N!=3 && N!=4) { std::cerr << "[Skybox] PNG must be RGB/RGBA: " << filename << std::endl; stbi_image_free(data); return false; }

    auto toRGBA = [&](const stbi_uc* src, int w, int h, int n){
        std::vector<unsigned char> out((size_t)w*h*4);
        if (n==4) {
            std::memcpy(out.data(), src, (size_t)w*h*4);
        } else {
            for (int i=0;i<w*h;++i){ out[i*4+0]=src[i*3+0]; out[i*4+1]=src[i*3+1]; out[i*4+2]=src[i*3+2]; out[i*4+3]=255; }
        }
        return out;
    };
    std::vector<unsigned char> rgba = toRGBA(data, W, H, N);
    stbi_image_free(data);

    auto extract = [&](int sx, int sy, int fw, int fh, bool flipV=false){
        std::vector<unsigned char> face((size_t)fw*fh*4);
        for (int y=0; y<fh; ++y){
            int srcY = flipV? (fh-1 - y) : y;
            for (int x=0; x<fw; ++x){
                int si = ((sy+srcY)*W + (sx+x))*4;
                int di = (y*fw + x)*4;
                face[di+0]=rgba[si+0]; face[di+1]=rgba[si+1]; face[di+2]=rgba[si+2]; face[di+3]=rgba[si+3];
            }
        }
        return face;
    };

    // Try to detect layout and face size
    int face = 0;
    enum Layout { HCROSS, VCROSS, HSTRIP, VSTRIP, GRID3x2, GRID2x3, UNKNOWN } layout = UNKNOWN;
    if (W%4==0 && H%3==0 && (W/4)==(H/3)) { face=W/4; layout=HCROSS; }
    else if (W%3==0 && H%4==0 && (W/3)==(H/4)) { face=W/3; layout=VCROSS; }
    else if (W%6==0 && (H==(W/6))) { face=W/6; layout=HSTRIP; }
    else if (H%6==0 && (W==(H/6))) { face=H/6; layout=VSTRIP; }
    else if (W%3==0 && H%2==0 && (W/3)==(H/2)) { face=W/3; layout=GRID3x2; }
    else if (W%2==0 && H%3==0 && (W/2)==(H/3)) { face=W/2; layout=GRID2x3; }

    if (layout==UNKNOWN || face<=0) {
        std::cerr << "[Skybox] Unknown single-PNG layout: "<<W<<"x"<<H<<" — expected 4x3, 3x4, 6x1, 1x6, 3x2 or 2x3" << std::endl;
        return false;
    }

    std::vector<unsigned char> facesRGBA[6];
    auto up = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

    if (layout==HCROSS) {
        // Map like previous T-layout (4x3):
        // (x,y) in face units
        facesRGBA[0] = extract(2*face, 1*face, face, face);          // +X right
        facesRGBA[1] = extract(0*face, 1*face, face, face);          // -X left
        facesRGBA[2] = extract(1*face, 0*face, face, face, true);    // +Y top (flipV)
        facesRGBA[3] = extract(1*face, 2*face, face, face);          // -Y bottom
        facesRGBA[4] = extract(1*face, 1*face, face, face);          // +Z front
        facesRGBA[5] = extract(3*face, 1*face, face, face);          // -Z back
    } else if (layout==VCROSS) {
        // Vertical cross (3x4): common mapping
        facesRGBA[2] = extract(1*face, 0*face, face, face, true);    // +Y
        facesRGBA[0] = extract(2*face, 1*face, face, face);          // +X
        facesRGBA[4] = extract(1*face, 1*face, face, face);          // +Z
        facesRGBA[1] = extract(0*face, 1*face, face, face);          // -X
        facesRGBA[5] = extract(1*face, 2*face, face, face);          // -Z
        facesRGBA[3] = extract(1*face, 3*face, face, face);          // -Y
    } else if (layout==HSTRIP) {
        // Assume order: +X -X +Y -Y +Z -Z
        facesRGBA[0] = extract(0*face, 0, face, face);
        facesRGBA[1] = extract(1*face, 0, face, face);
        facesRGBA[2] = extract(2*face, 0, face, face, true);
        facesRGBA[3] = extract(3*face, 0, face, face);
        facesRGBA[4] = extract(4*face, 0, face, face);
        facesRGBA[5] = extract(5*face, 0, face, face);
    } else if (layout==VSTRIP) {
        // Assume order: +X -X +Y -Y +Z -Z (top to bottom)
        facesRGBA[0] = extract(0, 0*face, face, face);
        facesRGBA[1] = extract(0, 1*face, face, face);
        facesRGBA[2] = extract(0, 2*face, face, face, true);
        facesRGBA[3] = extract(0, 3*face, face, face);
        facesRGBA[4] = extract(0, 4*face, face, face);
        facesRGBA[5] = extract(0, 5*face, face, face);
    } else if (layout==GRID3x2) {
        // 3x2 grid with this order (columns x rows):
        // Row 0: DOWN, UP, BACK
        // Row 1: LEFT, FRONT, RIGHT
        // Map to GL cubemap faces:
        facesRGBA[0] = extract(2*face, 1*face, face, face);         // +X (RIGHT)
        facesRGBA[1] = extract(0*face, 1*face, face, face);         // -X (LEFT)
        // Use DOWN tile as TOP face (vertical flip + 90° CW for alignment)
        facesRGBA[2] = extract(0*face, 0*face, face, face, true);   // +Y (TOP)
        facesRGBA[3] = extract(0*face, 0*face, face, face);         // -Y (DOWN)
        facesRGBA[4] = extract(1*face, 1*face, face, face);         // +Z (FRONT)
        facesRGBA[5] = extract(2*face, 0*face, face, face);         // -Z (BACK)
    } else if (layout==GRID2x3) {
        facesRGBA[0] = extract(0*face, 0*face, face, face); // +X
        facesRGBA[1] = extract(1*face, 0*face, face, face); // -X
        facesRGBA[2] = extract(0*face, 1*face, face, face, true); // +Y
        facesRGBA[3] = extract(1*face, 1*face, face, face); // -Y
        facesRGBA[4] = extract(0*face, 2*face, face, face); // +Z
        facesRGBA[5] = extract(1*face, 2*face, face, face); // -Z
    }

    // Optional seam smoothing (same simple average as in loadFromPNG)
    if (fixSeams) {
        int BW = 0; // blend width in pixels
        auto avgRows = [&](std::vector<unsigned char>& A, std::vector<unsigned char>& B, bool topA_vs_bottomB){
            int w=face,h=face; int yA0 = topA_vs_bottomB?0:(h-1); int yB0 = topA_vs_bottomB?(h-1):0;
            for (int k=0;k<BW;++k){ int yA = topA_vs_bottomB? (yA0 + k) : (yA0 - k); int yB = topA_vs_bottomB? (yB0 - k) : (yB0 + k);
                for (int x=0;x<w;++x){ unsigned char* pa = &A[(yA*w + x)*4]; unsigned char* pb = &B[(yB*w + x)*4]; for(int c=0;c<3;++c){ unsigned int m=pa[c]+pb[c]; pa[c]=pb[c]=(unsigned char)(m/2);} }
            }
        };
        auto avgCols = [&](std::vector<unsigned char>& A, std::vector<unsigned char>& B, bool leftA_vs_rightB){
            int w=face,h=face; int xA0 = leftA_vs_rightB?0:(w-1); int xB0 = leftA_vs_rightB?(w-1):0;
            for (int k=0;k<BW;++k){ int xA = leftA_vs_rightB? (xA0 + k) : (xA0 - k); int xB = leftA_vs_rightB? (xB0 - k) : (xB0 + k);
                for (int y=0;y<h;++y){ unsigned char* pa = &A[(y*w + xA)*4]; unsigned char* pb = &B[(y*w + xB)*4]; for(int c=0;c<3;++c){ unsigned int m=pa[c]+pb[c]; pa[c]=pb[c]=(unsigned char)(m/2);} }
            }
        };
        // Same adjacency heuristic as in loadFromPNG
        avgRows(facesRGBA[0], facesRGBA[2], true );
        avgRows(facesRGBA[0], facesRGBA[3], false);
        avgCols(facesRGBA[0], facesRGBA[4], true );
        avgCols(facesRGBA[0], facesRGBA[5], false);

        avgRows(facesRGBA[1], facesRGBA[2], true );
        avgRows(facesRGBA[1], facesRGBA[3], false);
        avgCols(facesRGBA[1], facesRGBA[5], true );
        avgCols(facesRGBA[1], facesRGBA[4], false);

        avgRows(facesRGBA[4], facesRGBA[2], true );
        avgRows(facesRGBA[4], facesRGBA[3], false);
        avgCols(facesRGBA[4], facesRGBA[1], true );
        avgCols(facesRGBA[4], facesRGBA[0], false);

        avgRows(facesRGBA[5], facesRGBA[2], true );
        avgRows(facesRGBA[5], facesRGBA[3], false);
        avgCols(facesRGBA[5], facesRGBA[0], true );
        avgCols(facesRGBA[5], facesRGBA[1], false);

        avgRows(facesRGBA[2], facesRGBA[5], true );
        avgRows(facesRGBA[2], facesRGBA[4], false);
        avgCols(facesRGBA[2], facesRGBA[1], true );
        avgCols(facesRGBA[2], facesRGBA[0], false);

        avgRows(facesRGBA[3], facesRGBA[4], true );
        avgRows(facesRGBA[3], facesRGBA[5], false);
        avgCols(facesRGBA[3], facesRGBA[1], true );
        avgCols(facesRGBA[3], facesRGBA[0], false);
    }

    if (!textureID) glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for (int i=0;i<6;++i) {
        glTexImage2D(up + i, 0, GL_RGBA, face, face, 0, GL_RGBA, GL_UNSIGNED_BYTE, facesRGBA[i].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    std::cerr << "[Skybox] Built cubemap from single PNG: " << filename << " faceSize="<<face<<" layout="<<layout << std::endl;
    return true;
}
