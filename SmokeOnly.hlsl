// 3D texture for smoke density
Texture3D<float> SmokeDensityTexture : register(t0);
SamplerState Sampler : register(s0);

// Input and output structures
struct VS_INPUT {
    float3 position : POSITION; // Vertex position
};

struct PS_INPUT {
    float4 position : SV_POSITION; // Clip-space position
    float2 uv : TEXCOORD;          // Screen-space UV coordinates
};

// Vertex Shader
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.position = float4(input.position, 1.0); // Pass position to clip space
    output.uv = (input.position.xy + 1.0) * 0.5;   // Map from [-1,1] to [0,1] UV
    return output;
}

// Helper: Ray-box intersection
bool IntersectBoundingBox(float3 rayOrigin, float3 rayDir, float3 boxMin, float3 boxMax, out float tNear, out float tFar) {
    float3 t1 = (boxMin - rayOrigin) / rayDir;
    float3 t2 = (boxMax - rayOrigin) / rayDir;
    float3 tMin = min(t1, t2);
    float3 tMax = max(t1, t2);
    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar = min(min(tMax.x, tMax.y), tMax.z);
    return tNear <= tFar && tFar >= 0.0;
}

// Pixel Shader
float4 PS(PS_INPUT input) : SV_Target{
    // Aspect ratio correction
    float aspectRatio = 1000.0 / 800.0; // Correct aspect ratio
    float2 correctedUV = float2((input.uv.x - 0.5) * aspectRatio, 0.5 - input.uv.y);

    // Perspective projection
    float fov = radians(90.0);  // Field of view
    float nearPlane = 0.01;
    float farPlane = 10.0;
    float3 cameraPos = float3(2, -1.5, -2); // Camera position
    float3 cameraTarget = float3(0.0, 0.0, 0.0); // Look at the origin
    float3 cameraUp = float3(0.0, 1.0, 0.0);     // Up vector

    // Camera setup (view matrix)
    float3 forward = normalize(cameraTarget - cameraPos);
    float3 right = normalize(cross(cameraUp, forward));
    float3 up = cross(forward, right);

    // Compute ray direction in world space
    float3 rayDir = normalize(forward + correctedUV.x * right + correctedUV.y * up);

    // Bounding box in world space
    float3 boxMin = float3(-0.5, -1, -0.5);
    float3 boxMax = float3(0.5, 1, 0.5);

    float3 background = float3(0.2, 0.2, 0.2);

    // Intersect the ray with the bounding box
    float tNear, tFar;
    if (!IntersectBoundingBox(cameraPos, rayDir, boxMin, boxMax, tNear, tFar)) {
        return float4(background, 1.0); // Background color
    }

    // Raymarching setup
    float3 currentPos = cameraPos + tNear * rayDir; // Start at the entry point
    float3 step = rayDir * 0.01;                   // Step size along the ray
    float accumulatedDensity = 0.0;               // Accumulated density
    float maxDensity = 1.0;                       // Maximum density for normalization
    float densityThreshold = 1e-50;                // Ignore very small densities

    // March along the ray
    for (float t = tNear; t < tFar; t += 0.01) {
        // Transform world space to texture space ([0, 1] range)
        float3 texCoord = (currentPos - boxMin) / (boxMax - boxMin);

        // Sample the smoke density texture
        float density = SmokeDensityTexture.SampleLevel(Sampler, texCoord, 0.0);

        // Accumulate density if above threshold
        if (density > densityThreshold) {
            accumulatedDensity += density * 0.01;
        }

        // Advance the ray
        currentPos += step;
    }

    // Final color and opacity
    float opacity = saturate(accumulatedDensity); // Normalize and clamp opacity
    float3 smokeColor = float3(0.7, 0.5, 0.25) * opacity + background;
    return float4(smokeColor, opacity);
}
