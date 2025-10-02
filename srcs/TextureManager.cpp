#include "TextureManager.hpp"
#include "stb_image.h"

// Load the PPM texture file
unsigned char* loadTexturePPM(const std::string& filename, int& width, int& height) {
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "Failed to open PPM file: " << filename << std::endl;
		return nullptr;
	}

	std::string header;
	int maxColor;
	file >> header >> width >> height >> maxColor;
	file.ignore(1); // Skip the single whitespace

	if (header != "P6" || maxColor != 255) {
		std::cerr << "Invalid PPM format (Only P6 with 255 max color supported)." << std::endl;
		return nullptr;
	}

	size_t imageSize = (width) * (height) * 3;
	unsigned char* data = new unsigned char[imageSize];
	file.read(reinterpret_cast<char*>(data), imageSize);
	file.close();

	size_t rgbaSize = width * height * 4;  // RGBA has 4 channels
	unsigned char* rgbaData = new unsigned char[rgbaSize];

	for (size_t i = 0, j = 0; i < imageSize; i += 3, j += 4) {
		rgbaData[j] = data[i];       // Red
		rgbaData[j + 1] = data[i + 1]; // Green
		rgbaData[j + 2] = data[i + 2]; // Blue
		rgbaData[j + 3] = 255;           // Alpha (fully opaque)
	}

	delete[] data;

	return rgbaData;
}

void TextureManager::loadTexturesArray(std::vector<std::pair<TextureType, std::string>> data) {
	glGenTextures(1, &_textureArrayID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _textureArrayID);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_LOD_BIAS, 0.0f);

	// Set Anisotropic Filtering (only if supported)
	GLfloat maxAniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	if (maxAniso > 1.0f) {
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
	}

	// Allocate storage for the texture array
	int width = TEXTURE_SIZE, height = TEXTURE_SIZE;
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, N_TEXTURES, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Helper: load PPM or PNG/JPG via STB and ensure RGBA 16x16
	auto loadTextureGeneric = [&](const std::string& filename)->std::unique_ptr<unsigned char[]> {
		// Detect by extension
		auto ends_with = [](const std::string& s, const std::string& suf){
			if (s.size() < suf.size()) return false;
			return std::equal(suf.rbegin(), suf.rend(), s.rbegin(), [](char a, char b){ return std::tolower(a)==std::tolower(b);});
		};
		int w = 0, h = 0;
		unsigned char* src = nullptr;
		if (ends_with(filename, ".ppm")) {
			unsigned char* ppm = loadTexturePPM(filename, w, h);
			src = ppm; // already RGBA
		} else {
			int ch = 0;
			src = stbi_load(filename.c_str(), &w, &h, &ch, 4);
			if (!src) {
				std::cerr << "Failed to load image: " << filename << std::endl;
				return nullptr;
			}
			// Zero RGB where fully transparent to avoid fringes
			for (int i = 0; i < w*h; ++i) { if (src[4*i+3] == 0) { src[4*i+0]=src[4*i+1]=src[4*i+2]=0; } }
		}

		// Resize to TEXTURE_SIZE x TEXTURE_SIZE if needed (nearest)
		std::unique_ptr<unsigned char[]> out(new unsigned char[TEXTURE_SIZE*TEXTURE_SIZE*4]);
		for (int y = 0; y < TEXTURE_SIZE; ++y) {
			for (int x = 0; x < TEXTURE_SIZE; ++x) {
				int sx = (w > 0) ? (x * w / TEXTURE_SIZE) : 0;
				int sy = (h > 0) ? (y * h / TEXTURE_SIZE) : 0;
				unsigned char* sp = src + 4 * (sy * std::max(w,1) + sx);
				unsigned char* dp = out.get() + 4 * (y * TEXTURE_SIZE + x);
				dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=sp[3];
			}
		}
		// Free source if it came from STB; for PPM path, src was new[]
		if (!ends_with(filename, ".ppm")) {
			stbi_image_free(src);
		} else {
			delete [] src;
		}
		return out;
	};

	// Load and upload textures
	for (int i = 0; i < N_TEXTURES; i++) {
		if (data.empty()) continue;

		// Special case: Leaves color from full_leaves.ppm, alpha from leave.png
		if (data[i].first == T_LEAF) {
			auto colorRGB = loadTextureGeneric("textures/full_leaves.ppm");
			auto alphaPNG = loadTextureGeneric("textures/leave.png");
			std::vector<unsigned char> out(TEXTURE_SIZE * TEXTURE_SIZE * 4, 0);
			unsigned char* col = colorRGB ? colorRGB.get() : nullptr;
			unsigned char* alp = alphaPNG ? alphaPNG.get() : nullptr;
			for (int pix = 0; pix < TEXTURE_SIZE*TEXTURE_SIZE; ++pix) {
				unsigned char r = col ? col[4*pix+0] : 0;
				unsigned char g = col ? col[4*pix+1] : 255;
				unsigned char b = col ? col[4*pix+2] : 0;
				unsigned char a = alp ? alp[4*pix+3] : 255;
				// Zero RGB where fully transparent to avoid fringes
				if (a == 0) { r = g = b = 0; }
				out[4*pix+0]=r; out[4*pix+1]=g; out[4*pix+2]=b; out[4*pix+3]=a;
			}
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0,0,i, TEXTURE_SIZE, TEXTURE_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
            // Average color (ignore fully transparent)
            long rr=0, gg=0, bb=0, cnt=0;
            for (int pix = 0; pix < TEXTURE_SIZE*TEXTURE_SIZE; ++pix) {
                unsigned char a = out[4*pix+3];
                if (a) { rr += out[4*pix+0]; gg += out[4*pix+1]; bb += out[4*pix+2]; cnt++; }
            }
            if (cnt == 0) cnt = 1;
            _avgColors[i] = glm::vec3((float)rr/(255.0f*cnt), (float)gg/(255.0f*cnt), (float)bb/(255.0f*cnt));
            // Build palette (non-transparent)
            _palettes[i].clear();
            _palettes[i].reserve(TEXTURE_SIZE*TEXTURE_SIZE);
            for (int pix = 0; pix < TEXTURE_SIZE*TEXTURE_SIZE; ++pix) {
                if (out[4*pix+3])
                    _palettes[i].push_back(glm::vec3(out[4*pix+0]/255.0f, out[4*pix+1]/255.0f, out[4*pix+2]/255.0f));
            }
            continue;
		}

		auto tex = loadTextureGeneric(data[i].second);
		if (!tex) {
			// Fallback to a solid magenta checker on failure
			std::vector<unsigned char> fallback(TEXTURE_SIZE*TEXTURE_SIZE*4, 255);
			for (int y=0;y<TEXTURE_SIZE;y++) for (int x=0;x<TEXTURE_SIZE;x++) {
				bool a=((x/2)%2)^((y/2)%2);
				unsigned char* p = fallback.data()+4*(y*TEXTURE_SIZE+x);
				p[0]=a?255:0; p[1]=0; p[2]=a?255:0; p[3]=255;
			}
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0,0,i, TEXTURE_SIZE, TEXTURE_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallback.data());
            _avgColors[i] = glm::vec3(1.0f, 0.0f, 1.0f);
            _palettes[i].assign(1, _avgColors[i]);
		} else {
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, TEXTURE_SIZE, TEXTURE_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, tex.get());
            // Average color
            long rr=0, gg=0, bb=0;
            for (int pix = 0; pix < TEXTURE_SIZE*TEXTURE_SIZE; ++pix) {
                rr += tex.get()[4*pix+0];
                gg += tex.get()[4*pix+1];
                bb += tex.get()[4*pix+2];
            }
            int cnt = TEXTURE_SIZE*TEXTURE_SIZE;
            _avgColors[i] = glm::vec3((float)rr/(255.0f*cnt), (float)gg/(255.0f*cnt), (float)bb/(255.0f*cnt));
            // Build palette (all texels)
            _palettes[i].clear();
            _palettes[i].reserve(TEXTURE_SIZE*TEXTURE_SIZE);
            for (int pix = 0; pix < TEXTURE_SIZE*TEXTURE_SIZE; ++pix) {
                _palettes[i].push_back(glm::vec3(tex.get()[4*pix+0]/255.0f, tex.get()[4*pix+1]/255.0f, tex.get()[4*pix+2]/255.0f));
            }
		}
	}

	// Generate mipmaps after all textures are uploaded
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

GLuint TextureManager::getTextureArray() const
{
    return _textureArrayID;
}

TextureManager::TextureManager() {
    for (int i = 0; i < N_TEXTURES; ++i) _avgColors[i] = glm::vec3(1.0f);
}

TextureManager::~TextureManager() {
    if (_textureArrayID) {
        glDeleteTextures(1, &_textureArrayID);
        _textureArrayID = 0;
    }
}

std::vector<glm::vec3> TextureManager::sampleColors(TextureType t, int count, std::mt19937 &rng) const
{
    std::vector<glm::vec3> out;
    if (count <= 0) return out;
    const auto &pal = _palettes[(int)t];
    if (pal.empty()) {
        out.assign(count, _avgColors[(int)t]);
        return out;
    }
    std::uniform_int_distribution<size_t> pick(0, pal.size()-1);
    out.reserve(count);
    for (int i = 0; i < count; ++i)
        out.push_back(pal[pick(rng)]);
    return out;
}
