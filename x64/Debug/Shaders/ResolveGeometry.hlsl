
#include "Common.hlsli"

#define FRAGMENT_COUNT    32
#define MSAA_SAMPLE_COUNT 4

RWTexture2D<unorm float4>  BackBuffer      : register(u0);
Texture2D<uint>            HeadPointersSRV : register(t0);
StructuredBuffer<ListNode> LinkedListSRV   : register(t1);


[numthreads(8, 8, 1)]
void CSMain(uint3 id: SV_DispatchThreadID) {
       
    float4 backBuffer    = BackBuffer[id.xy];
    float4 resolveBuffer = float4(0.0, 0.0, 0.0, 0.0f);
    
    ListSubNode nodes[FRAGMENT_COUNT];   
    for (uint sampleIdx = 0; sampleIdx < MSAA_SAMPLE_COUNT; sampleIdx++) {     
        uint count   = 0;
        uint nodeIdx = HeadPointersSRV[id.xy];
        if (nodeIdx == 0xFFFFFFFF)
            return;       
            
        while (nodeIdx != 0xFFFFFFFF && count < FRAGMENT_COUNT) {
            ListNode node = LinkedListSRV[nodeIdx];
            if (node.Coverage & (1 << sampleIdx)) {
                nodes[count].Depth = asfloat(node.Depth);
                nodes[count].Color = node.Color;
                count++;
            }
            nodeIdx = node.Next;
        }
              
        for (uint i = 1; i < count; i++) {
            ListSubNode t = nodes[i];
            uint j = i;
            while (j > 0 && (nodes[j - 1].Depth < t.Depth)) {
                nodes[j] = nodes[j - 1];
                j--;
            }
            nodes[j] = t;
        }
         
        float4 dstPixelColor = backBuffer;
        for (uint index = 0; index < count; index++) {
            float4 srcPixelColor = UnpackColor(nodes[index].Color);
            dstPixelColor = lerp(dstPixelColor, srcPixelColor, srcPixelColor.a);
        }
        resolveBuffer += dstPixelColor;
    }
   
    BackBuffer[id.xy] = resolveBuffer / MSAA_SAMPLE_COUNT;
}
