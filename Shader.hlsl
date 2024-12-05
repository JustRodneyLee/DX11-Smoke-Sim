// 3D texture for smoke density
Texture3D<float> SmokeDensityTexture : register(t0);
SamplerState Sampler : register(s0);

float cubeSDF(float3 p, float3 cubeCenter, float3 cubeSize)
{
    float3 d = abs(p - cubeCenter) - cubeSize;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

// Input and output structures
struct VS_INPUT
{
    float3 position : POSITION; // Vertex position
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Clip-space position
    float2 uv : TEXCOORD; // Screen-space UV coordinates
};

// Vertex Shader
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0); // Pass position to clip space
    output.uv = (input.position.xy + 1.0) * 0.5; // Map from [-1,1] to [0,1] UV
    return output;
}

bool IntersectBoundingBox(float3 rayOrigin, float3 rayDir, float3 boxMin, float3 boxMax, out float tNear, out float tFar)
{
    float3 t1 = (boxMin - rayOrigin) / rayDir;
    float3 t2 = (boxMax - rayOrigin) / rayDir;
    float3 tMin = min(t1, t2);
    float3 tMax = max(t1, t2);
    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar = min(min(tMax.x, tMax.y), tMax.z);
    return tNear <= tFar && tFar >= 0.0;
}

float4 PS(PS_INPUT input) : SV_Target
{
    // Aspect ratio correction
    float aspectRatio = 1000.0 / 800.0;
    float2 correctedUV = float2((input.uv.x - 0.5) * aspectRatio, 0.5 - input.uv.y);

    // Perspective projection
    float fov = radians(90.0);
    float3 cameraPos = float3(2, -1.5, -2);
    float3 cameraTarget = float3(0.0, 0.0, 0.0);
    float3 cameraUp = float3(0.0, 1.0, 0.0);

    // Camera vectors
    float3 forward = normalize(cameraTarget - cameraPos);
    float3 right = normalize(cross(cameraUp, forward));
    float3 up = cross(forward, right);

    float3 rayDir = normalize(forward + correctedUV.x * right + correctedUV.y * up);

    // Bounding Box
    float3 boxMin = float3(-0.5, -1, -0.5);
    float3 boxMax = float3(0.5, 1, 0.5);

    float3 background = float3(0.1, 0.1, 0.1);

    // Get clipping planes
    float tNear, tFar;
    if (!IntersectBoundingBox(cameraPos, rayDir, boxMin, boxMax, tNear, tFar))
    {
        return float4(background, 1.0);
    }
    
    float3 currentPos = cameraPos + tNear * rayDir;
    float sumDensity = 0.0;
    float maxDensity = 1.0;
    float densityThreshold = 1e-50;
    float stepSize = 0.001;

    for (float t = tNear; t < tFar; t += stepSize)
    {
        float3 texCoord = (currentPos - boxMin) / (boxMax - boxMin);
        float density = SmokeDensityTexture.SampleLevel(Sampler, texCoord, 0.0);
        if (density > densityThreshold)
        {
            sumDensity += density * stepSize;
        }
        
        float3 cubePosition = float3(0.0, -0.75, 0.0);
        float3 cubeSize = float3(0.2, 0.2, 0.2);

        // Cube SDF check
        if (cubeSDF(currentPos, cubePosition, cubeSize) <= stepSize)
        {
            float3 lightPosition = float3(1.0, -2.0, -.3);
            float3 lightColor = float3(0.5, 0.5, 0.5);            
            float3 lightDir = normalize(lightPosition - currentPos);
            
            float3 cubeColor = float3(0.6, 0.6, 0.6);
            
            float3 normal = normalize(currentPos - cubePosition); // ehhhhhh not exactly
            
            float3 diffuse = cubeColor * max(dot(normal, lightDir), 0.0);
            
            float4 boxColor = float4(float3(0.1, 0.1, 0.1) + diffuse, 1.0f); // phong shading
            
            float opacity = saturate(sumDensity);
            
            float3 smokeColor = float3(0.6, 0.4, 0.2) * opacity + background;
            
            return float4(smokeColor, opacity) + boxColor;
        }

        currentPos += rayDir * stepSize;
    }
    
    float opacity = saturate(sumDensity);
    float3 smokeColor = float3(0.6, 0.4, 0.2) * opacity + background;
    return float4(smokeColor, opacity);
}

