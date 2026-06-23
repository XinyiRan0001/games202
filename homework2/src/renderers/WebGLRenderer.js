function normalizeVec3(v) {
    const len = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return [v[0] / len, v[1] / len, v[2] / len];
}

function rotateDirByMat4(dir, mat) {
    const x = dir[0], y = dir[1], z = dir[2];

    return normalizeVec3([
        mat[0] * x + mat[4] * y + mat[8] * z,
        mat[1] * x + mat[5] * y + mat[9] * z,
        mat[2] * x + mat[6] * y + mat[10] * z
    ]);
}

function evalSHLight(coeffs, dir) {
    const sh = SHEval(dir[0], dir[1], dir[2], 3);
    let color = [0, 0, 0];

    for (let i = 0; i < 9; i++) {
        color[0] += coeffs[i][0] * sh[i];
        color[1] += coeffs[i][1] * sh[i];
        color[2] += coeffs[i][2] * sh[i];
    }

    return color;
}

function getRotationPrecomputeL(precomputeL, rotationMat4) {
    const sampleCount = 100;
    const weight = 4.0 * Math.PI / sampleCount;

    let result = [];
    for (let i = 0; i < 9; i++) {
        result.push([0, 0, 0]);
    }

    for (let i = 0; i < sampleCount; i++) {
        const z = 1.0 - 2.0 * (i + 0.5) / sampleCount;
        const r = Math.sqrt(Math.max(0.0, 1.0 - z * z));
        const phi = Math.PI * (3.0 - Math.sqrt(5.0)) * i;

        const dir = [
            r * Math.cos(phi),
            r * Math.sin(phi),
            z
        ];

        let invRotation = mat4.create();
        mat4.invert(invRotation, rotationMat4);

        const rotatedDir = rotateDirByMat4(dir, invRotation);

        const color = evalSHLight(precomputeL, rotatedDir);
        const sh = SHEval(dir[0], dir[1], dir[2], 3);

        for (let j = 0; j < 9; j++) {
            result[j][0] += color[0] * sh[j] * weight;
            result[j][1] += color[1] * sh[j] * weight;
            result[j][2] += color[2] * sh[j] * weight;
        }
    }

    return result;
}

class WebGLRenderer {
    meshes = [];
    shadowMeshes = [];
    lights = [];

    constructor(gl, camera) {
        this.gl = gl;
        this.camera = camera;
    }

    addLight(light) {
        this.lights.push({
            entity: light,
            meshRender: new MeshRender(this.gl, light.mesh, light.mat)
        });
    }
    addMeshRender(mesh) { this.meshes.push(mesh); }
    addShadowMeshRender(mesh) { this.shadowMeshes.push(mesh); }

    render() {
        const gl = this.gl;

        gl.clearColor(0.0, 0.0, 0.0, 1.0); // Clear to black, fully opaque
        gl.clearDepth(1.0); // Clear everything
        gl.enable(gl.DEPTH_TEST); // Enable depth testing
        gl.depthFunc(gl.LEQUAL); // Near things obscure far things

        console.assert(this.lights.length != 0, "No light");
        console.assert(this.lights.length == 1, "Multiple lights");

        const timer = Date.now() * 0.0001;

        for (let l = 0; l < this.lights.length; l++) {
            // Draw light
            this.lights[l].meshRender.mesh.transform.translate = this.lights[l].entity.lightPos;
            this.lights[l].meshRender.draw(this.camera);

            // Shadow pass
            if (this.lights[l].entity.hasShadowMap == true) {
                for (let i = 0; i < this.shadowMeshes.length; i++) {
                    this.shadowMeshes[i].draw(this.camera);
                }
            }

            // Camera pass
            for (let i = 0; i < this.meshes.length; i++) {
                this.gl.useProgram(this.meshes[i].shader.program.glShaderProgram);
                this.gl.uniform3fv(
                    this.meshes[i].shader.program.uniforms.uLightPos,
                    this.lights[l].entity.lightPos
                );

                let cameraModelMatrix = mat4.create();
                mat4.fromRotation(cameraModelMatrix, timer, [0, 1, 0]);

                for (let k in this.meshes[i].material.uniforms) {
                    if (k == 'uMoveWithCamera') {
                        gl.uniformMatrix4fv(
                            this.meshes[i].shader.program.uniforms[k],
                            false,
                            cameraModelMatrix
                        );
                    }
                }

                // Bonus - rotate SH light coefficients for PRT material
                if (this.meshes[i].material.uniforms['uPrecomputeL[0]'] !== undefined) {
                    let rotatedL =
                        getRotationPrecomputeL(precomputeL[guiParams.envmapId], cameraModelMatrix);

                    for (let j = 0; j < 9; j++) {
                        this.meshes[i].material.uniforms[`uPrecomputeL[${j}]`].value = rotatedL[j];
                    }
                }

                this.meshes[i].draw(this.camera);
            }
        }

    }
}