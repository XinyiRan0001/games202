#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <random>
#include "vec.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

const int resolution = 128;

typedef struct samplePoints {
    std::vector<Vec3f> directions;
    std::vector<float> PDFs;
}samplePoints;

samplePoints squareToCosineHemisphere(int sample_count) {
    samplePoints samlpeList;
    const int sample_side = static_cast<int>(floor(sqrt(sample_count)));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> rng(0.0, 1.0);
    for (int t = 0; t < sample_side; t++) {
        for (int p = 0; p < sample_side; p++) {
            double samplex = (t + rng(gen)) / sample_side;
            double sampley = (p + rng(gen)) / sample_side;

            double theta = 0.5f * acos(1 - 2 * samplex);
            double phi = 2 * PI * sampley;
            Vec3f wi = Vec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float pdf = wi.z / PI;

            samlpeList.directions.push_back(wi);
            samlpeList.PDFs.push_back(pdf);
        }
    }
    return samlpeList;
}

float DistributionGGX(Vec3f N, Vec3f H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = std::max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / std::max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float roughness, float NoV, float NoL) {
    float ggx2 = GeometrySchlickGGX(NoV, roughness);
    float ggx1 = GeometrySchlickGGX(NoL, roughness);

    return ggx1 * ggx2;
}

Vec3f IntegrateBRDF(Vec3f V, float roughness, float NdotV) {
    float A = 0.0f;
    float B = 0.0f;
    float C = 0.0f;

    const int sample_count = 1024;
    Vec3f N = Vec3f(0.0f, 0.0f, 1.0f);

    samplePoints sampleList = squareToCosineHemisphere(sample_count);

    for (int i = 0; i < sample_count; i++) {
        Vec3f L = normalize(sampleList.directions[i]);
        float pdf = sampleList.PDFs[i];

        Vec3f H = normalize(V + L);

        float NdotL = std::max(dot(N, L), 0.0f);

        if (NdotL > 0.0f && NdotV > 0.0f && pdf > 0.0f) {
            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(roughness, NdotV, NdotL);

            float numerator = NDF * G;
            float denominator = 4.0f * NdotV * NdotL;

            float value = numerator / denominator * NdotL / pdf;

            A += value;
            B += value;
            C += value;
        }
    }

    return Vec3f(
        A / sample_count,
        B / sample_count,
        C / sample_count
    );
}

int main() {
    uint8_t* data = new uint8_t[resolution * resolution * 3];
    float step = 1.0 / resolution;
    for (int i = 0; i < resolution; i++) {
        for (int j = 0; j < resolution; j++) {
            float roughness = step * (static_cast<float>(i) + 0.5f);
            float NdotV = step * (static_cast<float>(j) + 0.5f);
            Vec3f V = Vec3f(std::sqrt(1.f - NdotV * NdotV), 0.f, NdotV);

            Vec3f irr = IntegrateBRDF(V, roughness, NdotV);

            int index = (i * resolution + j) * 3;

            data[index + 0] = uint8_t(std::min(irr.x, 1.0f) * 255.0f);
            data[index + 1] = uint8_t(std::min(irr.y, 1.0f) * 255.0f);
            data[index + 2] = uint8_t(std::min(irr.z, 1.0f) * 255.0f);
        }
    }
    stbi_flip_vertically_on_write(true);
    stbi_write_png("GGX_E_MC_LUT.png", resolution, resolution, 3, data, resolution * 3);

    std::cout << "Finished precomputed!" << std::endl;
    return 0;
}