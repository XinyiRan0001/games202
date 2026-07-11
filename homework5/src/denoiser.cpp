#include "denoiser.h"

Denoiser::Denoiser() : m_useTemportal(false) {}

void Denoiser::Reprojection(const FrameInfo &frameInfo) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;

    // Previous frame World-to-Screen matrix
    Matrix4x4 preWorldToScreen =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 1];

    // The last two matrices are not object transformation matrices
    int curObjectCount = static_cast<int>(frameInfo.m_matrix.size()) - 2;

    int preObjectCount = static_cast<int>(m_preFrameInfo.m_matrix.size()) - 2;

#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Initialize the reprojection result as invalid
            m_valid(x, y) = false;
            m_misc(x, y) = Float3(0.0f);

            // Get the object ID of the current pixel
            int objectId = static_cast<int>(frameInfo.m_id(x, y));

            // Background pixels have an object ID of -1
            if (objectId < 0 || objectId >= curObjectCount ||
                objectId >= preObjectCount) {
                continue;
            }

            // Get the current world-space position
            Float3 curWorldPosition = frameInfo.m_position(x, y);

            // Current frame Object-to-World matrix
            Matrix4x4 curObjectToWorld = frameInfo.m_matrix[objectId];

            // Previous frame Object-to-World matrix
            Matrix4x4 preObjectToWorld = m_preFrameInfo.m_matrix[objectId];

            /*
             * Current world-space position
             *      ↓ inverse(current Object-to-World)
             * Object-space position
             *      ↓ previous Object-to-World
             * Previous world-space position
             */
            Float3 objectPosition =
                Inverse(curObjectToWorld)(curWorldPosition, Float3::Point);

            Float3 preWorldPosition = preObjectToWorld(objectPosition, Float3::Point);

            // Project the previous world-space position to screen space
            Float3 preScreenPosition = preWorldToScreen(preWorldPosition, Float3::Point);

            // Use the nearest pixel
            int preX = static_cast<int>(preScreenPosition.x + 0.5f);

            int preY = static_cast<int>(preScreenPosition.y + 0.5f);

            // Check whether the projected pixel is inside the screen
            if (preX < 0 || preX >= width || preY < 0 || preY >= height) {
                continue;
            }

            // Validate the reprojection using the object ID
            int preObjectId = static_cast<int>(m_preFrameInfo.m_id(preX, preY));

            if (preObjectId != objectId) {
                continue;
            }

            // Store the reprojected history color
            m_misc(x, y) = m_accColor(preX, preY);
            m_valid(x, y) = true;
        }
    }

    // Store the reprojected history in m_accColor
    std::swap(m_misc, m_accColor);
}

void Denoiser::TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor) {

    int height = m_accColor.m_height;
    int width = m_accColor.m_width;

    // Use a 7 x 7 neighborhood
    int kernelRadius = 3;

#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            Float3 currentColor = curFilteredColor(x, y);

            // Use only the current frame if reprojection is invalid
            if (!m_valid(x, y)) {
                m_misc(x, y) = currentColor;
                continue;
            }

            // Calculate the local mean and variance
            Float3 colorSum(0.0f);
            Float3 colorSquareSum(0.0f);
            int sampleCount = 0;

            for (int dy = -kernelRadius; dy <= kernelRadius; dy++) {

                for (int dx = -kernelRadius; dx <= kernelRadius; dx++) {

                    int nx = x + dx;
                    int ny = y + dy;

                    // Skip pixels outside the image
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                        continue;
                    }

                    Float3 sampleColor = curFilteredColor(nx, ny);

                    colorSum += sampleColor;
                    colorSquareSum += Sqr(sampleColor);
                    sampleCount++;
                }
            }

            Float3 mean = colorSum / static_cast<float>(sampleCount);

            // Variance = E[x^2] - E[x]^2
            Float3 variance =
                colorSquareSum / static_cast<float>(sampleCount) - Sqr(mean);

            // Prevent small negative values caused by floating-point errors
            variance = Max(variance, Float3(0.0f));

            Float3 standardDeviation = SafeSqrt(variance);

            // Define the valid history color range
            Float3 colorMin = mean - standardDeviation * m_colorBoxK;

            Float3 colorMax = mean + standardDeviation * m_colorBoxK;

            Float3 historyColor = m_accColor(x, y);

            // Clamp the history color to reduce temporal artifacts
            Float3 clampedHistoryColor = Clamp(historyColor, colorMin, colorMax);

            /*
             * Exponential moving average:
             *
             * result =
             *     (1 - alpha) * history
             *     + alpha * current
             */
            float alpha = m_alpha;

            m_misc(x, y) = Lerp(clampedHistoryColor, currentColor, alpha);
        }
    }

    // Store the accumulated result
    std::swap(m_misc, m_accColor);
}

Buffer2D<Float3> Denoiser::Filter(const FrameInfo &frameInfo) {

    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;

    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);

    // Radius 16 produces a 33 x 33 filtering kernel
    int kernelRadius = 16;

#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // Get the center pixel information
            Float3 centerColor = frameInfo.m_beauty(x, y);

            Float3 centerNormal = frameInfo.m_normal(x, y);

            Float3 centerPosition = frameInfo.m_position(x, y);

            Float3 colorSum(0.0f);
            float weightSum = 0.0f;

            // Traverse neighboring pixels
            for (int dy = -kernelRadius; dy <= kernelRadius; dy++) {

                for (int dx = -kernelRadius; dx <= kernelRadius; dx++) {

                    int nx = x + dx;
                    int ny = y + dy;

                    // Skip pixels outside the image
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                        continue;
                    }

                    Float3 neighborColor = frameInfo.m_beauty(nx, ny);

                    Float3 neighborNormal = frameInfo.m_normal(nx, ny);

                    Float3 neighborPosition = frameInfo.m_position(nx, ny);

                    // Calculate the screen-space coordinate distance
                    float coordDistance2 = static_cast<float>(dx * dx + dy * dy);

                    // Calculate the RGB color distance
                    float colorDistance2 = SqrDistance(centerColor, neighborColor);

                    // Calculate the angle between surface normals
                    float normalDot = Dot(centerNormal, neighborNormal);

                    float normalDistance = SafeAcos(normalDot);

                    // Calculate the plane distance
                    Float3 positionDifference = neighborPosition - centerPosition;

                    float positionDistance = Length(positionDifference);

                    float planeDistance = 0.0f;

                    if (positionDistance > 1e-6f) {
                        Float3 direction = positionDifference / positionDistance;

                        planeDistance = Dot(centerNormal, direction);
                    }

                    // Calculate the joint bilateral filter weight
                    float exponent = -coordDistance2 / (2.0f * Sqr(m_sigmaCoord)) -
                                     colorDistance2 / (2.0f * Sqr(m_sigmaColor)) -
                                     Sqr(normalDistance) / (2.0f * Sqr(m_sigmaNormal)) -
                                     Sqr(planeDistance) / (2.0f * Sqr(m_sigmaPlane));

                    float weight = std::exp(exponent);

                    colorSum += neighborColor * weight;

                    weightSum += weight;
                }
            }

            // Normalize the accumulated color
            if (weightSum > 0.0f) {
                filteredImage(x, y) = colorSum / weightSum;
            } else {
                filteredImage(x, y) = centerColor;
            }
        }
    }

    return filteredImage;
}

Buffer2D<Float3> Denoiser::AtrousFilter(const FrameInfo &frameInfo) {

    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;

    // Ping-pong buffers used between filtering iterations
    Buffer2D<Float3> ping = CreateBuffer2D<Float3>(width, height);

    Buffer2D<Float3> pong = CreateBuffer2D<Float3>(width, height);

    // The first iteration uses the original noisy image
    ping.Copy(frameInfo.m_beauty);

    /*
     * À-Trous 5-tap wavelet kernel:
     *
     * [1, 4, 6, 4, 1] / 16
     *
     * The two-dimensional spatial weight is calculated
     * by multiplying the horizontal and vertical weights.
     */
    const float kernel[5] = {1.0f / 16.0f, 4.0f / 16.0f, 6.0f / 16.0f, 4.0f / 16.0f,
                             1.0f / 16.0f};

    // Use four iterations with step sizes 1, 2, 4, and 8
    const int iterationCount = 4;

    for (int iteration = 0; iteration < iterationCount; iteration++) {

        int step = 1 << iteration;

#pragma omp parallel for
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                Float3 centerColor = ping(x, y);

                Float3 centerNormal = frameInfo.m_normal(x, y);

                Float3 centerPosition = frameInfo.m_position(x, y);

                Float3 colorSum(0.0f);
                float weightSum = 0.0f;

                for (int ky = -2; ky <= 2; ky++) {
                    for (int kx = -2; kx <= 2; kx++) {

                        int nx = x + kx * step;
                        int ny = y + ky * step;

                        // Skip pixels outside the image
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                            continue;
                        }

                        Float3 neighborColor = ping(nx, ny);

                        Float3 neighborNormal = frameInfo.m_normal(nx, ny);

                        Float3 neighborPosition = frameInfo.m_position(nx, ny);

                        // Calculate the À-Trous spatial kernel weight
                        float spatialWeight = kernel[kx + 2] * kernel[ky + 2];

                        /*
                         * Use relative luminance difference for HDR input
                         * instead of absolute RGB distance.
                         */
                        float centerLuminance = std::max(Luminance(centerColor), 1e-3f);

                        float neighborLuminance =
                            std::max(Luminance(neighborColor), 1e-3f);

                        float luminanceDifference =
                            std::fabs(centerLuminance - neighborLuminance);

                        float relativeColorDistance =
                            luminanceDifference /
                            std::max(centerLuminance, neighborLuminance);

                        float colorDistance2 = Sqr(relativeColorDistance);

                        // Calculate the angle between surface normals
                        float normalDot = Dot(centerNormal, neighborNormal);

                        float normalDistance = SafeAcos(normalDot);

                        // Calculate the plane distance
                        Float3 positionDifference = neighborPosition - centerPosition;

                        float positionDistance = Length(positionDifference);

                        float planeDistance = 0.0f;

                        if (positionDistance > 1e-6f) {
                            Float3 direction = positionDifference / positionDistance;

                            planeDistance = Dot(centerNormal, direction);
                        }

                        // Calculate the edge-stopping weight
                        float edgeExponent =
                            -colorDistance2 / (2.0f * Sqr(m_sigmaColor)) -
                            Sqr(normalDistance) / (2.0f * Sqr(m_sigmaNormal)) -
                            Sqr(planeDistance) / (2.0f * Sqr(m_sigmaPlane));

                        float edgeWeight = std::exp(edgeExponent);

                        float weight = spatialWeight * edgeWeight;

                        colorSum += neighborColor * weight;

                        weightSum += weight;
                    }
                }

                // Normalize the accumulated color
                if (weightSum > 1e-6f) {
                    pong(x, y) = colorSum / weightSum;
                } else {
                    pong(x, y) = centerColor;
                }
            }
        }

        // Use the current result as the input to the next iteration
        std::swap(ping, pong);
    }

    return ping;
}

void Denoiser::Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor) {

    m_accColor.Copy(filteredColor);

    int height = m_accColor.m_height;
    int width = m_accColor.m_width;

    m_misc = CreateBuffer2D<Float3>(width, height);

    m_valid = CreateBuffer2D<bool>(width, height);
}

void Denoiser::Maintain(const FrameInfo &frameInfo) { m_preFrameInfo = frameInfo; }

Buffer2D<Float3> Denoiser::ProcessFrame(const FrameInfo &frameInfo) {

    // Apply À-Trous Wavelet filtering to the current frame
    Buffer2D<Float3> filteredColor;
    filteredColor = AtrousFilter(frameInfo);

    // Reproject and accumulate the previous frame history
    if (m_useTemportal) {
        Reprojection(frameInfo);
        TemporalAccumulation(filteredColor);
    } else {
        Init(frameInfo, filteredColor);
    }

    // Store the current frame information for the next frame
    Maintain(frameInfo);

    if (!m_useTemportal) {
        m_useTemportal = true;
    }

    return m_accColor;
}