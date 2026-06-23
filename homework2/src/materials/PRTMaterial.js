class PRTMaterial extends Material {
    constructor(vertexShader, fragmentShader) {
        const id = guiParams.envmapId;

        super({
            'uPrecomputeL[0]': { type: '3fv', value: precomputeL[id][0] },
            'uPrecomputeL[1]': { type: '3fv', value: precomputeL[id][1] },
            'uPrecomputeL[2]': { type: '3fv', value: precomputeL[id][2] },
            'uPrecomputeL[3]': { type: '3fv', value: precomputeL[id][3] },
            'uPrecomputeL[4]': { type: '3fv', value: precomputeL[id][4] },
            'uPrecomputeL[5]': { type: '3fv', value: precomputeL[id][5] },
            'uPrecomputeL[6]': { type: '3fv', value: precomputeL[id][6] },
            'uPrecomputeL[7]': { type: '3fv', value: precomputeL[id][7] },
            'uPrecomputeL[8]': { type: '3fv', value: precomputeL[id][8] },
        }, [
            'aPrecomputeLT'
        ], vertexShader, fragmentShader, null);
    }
}

async function buildPRTMaterial(vertexPath, fragmentPath) {
    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);
    return new PRTMaterial(vertexShader, fragmentShader);
}