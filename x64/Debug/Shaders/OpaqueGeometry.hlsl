#include "Common.hlsli"

void VSMain(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID,  out float4 position: SV_Position, out float4 color: TEXCOORD) {
float2 vertexPositions[] = { float2(-0.5, -0.5), float2(+0.5, -0.5), float2(+0.0, +0.5) };
    float4 vertexColors[]    = { float4(1.0, 0.0, 0.0, 1.0), float4(0.0, 1.0, 0.0, 1.0), float4(0.0, 0.0, 1.0, 1.0) };
	float2 instancePositionOffsets[] = { float2(.0, 0.0), float2(0.5, 0.5), float2(-0.5, -0.5), float2(-0.5, 0.5), float2(0.5, -0.5) };
    
    color = vertexColors[vertexID];
    position = float4(vertexPositions[vertexID] + instancePositionOffsets[instanceID], 0.8, 1.0f);
}

float4 PSMain(float4 position : SV_Position, float4 color: TEXCOORD) : SV_Target{
    return float4(color);
}

