# GAMES202 Homework 5: Real-Time Ray Tracing Denoising

This repository contains my implementation of Homework 5 for GAMES202: Real-Time High Quality Rendering.

The assignment implements a denoising pipeline for noisy ray-traced image sequences using single-frame filtering, temporal reprojection, temporal accumulation, and an additional À-Trous Wavelet filtering method.

## Implemented Features

### 1. Single-Frame Joint Bilateral Filtering

Implemented the joint bilateral filter in `Denoiser::Filter`.

The filter uses multiple G-Buffer features to calculate the filtering weights:

- Screen-space coordinate distance
- RGB color difference
- Surface normal difference
- Plane distance based on world-space positions

The filter reduces Monte Carlo noise while preserving geometric boundaries.

### 2. Temporal Reprojection

Implemented temporal reprojection in `Denoiser::Reprojection`.

For each pixel in the current frame:

1. Retrieve the current world-space position.
2. Transform the position back into object space using the inverse current Object-to-World matrix.
3. Transform the object-space position using the previous frame's Object-to-World matrix.
4. Project the previous world-space position into the previous screen space.
5. Validate the projected pixel using screen boundaries and object IDs.
6. Reproject the accumulated history color to the current frame.

### 3. Temporal Accumulation

Implemented temporal accumulation in `Denoiser::TemporalAccumulation`.

For each valid reprojected pixel, the implementation calculates the local mean and variance from a 7 x 7 neighborhood of the current filtered image.

The previous accumulated color is clamped to:

`mean - k * standardDeviation`

and

`mean + k * standardDeviation`

The final color is calculated using an exponential moving average between the clamped history color and the current filtered color.

Invalid reprojection results use only the current frame color.

### 4. Bonus: À-Trous Wavelet Filtering

Implemented an additional À-Trous Wavelet filter in `Denoiser::AtrousFilter`.

The implementation uses a separable 5-tap wavelet kernel:

`[1, 4, 6, 4, 1] / 16`

Four filtering iterations are performed with increasing sampling steps:

`1, 2, 4, 8`

This increases the effective filtering radius without requiring a large dense sampling kernel.

The À-Trous implementation uses the following edge-stopping information:

- Relative luminance difference
- Surface normal difference
- Plane distance

Relative luminance difference is used for HDR OpenEXR input to prevent large absolute RGB differences from rejecting most neighboring samples.

The final denoising pipeline uses:

`À-Trous Filtering -> Temporal Reprojection -> Temporal Accumulation`

## Modified Files

### `src/denoiser.cpp`

Implemented:

- `Denoiser::Filter`
- `Denoiser::Reprojection`
- `Denoiser::TemporalAccumulation`
- `Denoiser::AtrousFilter`
- Final denoising pipeline in `Denoiser::ProcessFrame`

### `src/denoiser.h`

Added:

- Declaration for `Denoiser::AtrousFilter`

## Parameters

The implementation uses the following parameters:

```cpp
float m_alpha = 0.2f;
float m_sigmaPlane = 0.1f;
float m_sigmaColor = 0.6f;
float m_sigmaNormal = 0.1f;
float m_sigmaCoord = 32.0f;
float m_colorBoxK = 1.0f;