#ifdef GL_ES
precision mediump float;
#endif

attribute vec3 aVertexPosition;
attribute vec3 aNormalPosition;
attribute mat3 aPrecomputeLT;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

uniform vec3 uPrecomputeL[9];

varying highp vec3 vColor;

void main(void) {
    gl_Position =
        uProjectionMatrix *
        uViewMatrix *
        uModelMatrix *
        vec4(aVertexPosition, 1.0);

    vec3 LT0 = aPrecomputeLT[0];
    vec3 LT1 = aPrecomputeLT[1];
    vec3 LT2 = aPrecomputeLT[2];

vColor =
    uPrecomputeL[0] * LT0.x +
    uPrecomputeL[1] * LT0.y +
    uPrecomputeL[2] * LT0.z +
    uPrecomputeL[3] * LT1.x +
    uPrecomputeL[4] * LT1.y +
    uPrecomputeL[5] * LT1.z +
    uPrecomputeL[6] * LT2.x +
    uPrecomputeL[7] * LT2.y +
    uPrecomputeL[8] * LT2.z;

vColor += abs(aNormalPosition) * 0.000001;
}