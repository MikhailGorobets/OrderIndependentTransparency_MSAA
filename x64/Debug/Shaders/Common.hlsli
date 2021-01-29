
struct ListNode {
    uint Next;
    uint Color;
    uint Depth;
    uint Coverage;
};

struct ListSubNode {
    float  Depth;
    uint   Color;
};

uint PackColor(float4 color) {
    return (uint(color.r * 255) << 24) | (uint(color.g * 255) << 16) | (uint(color.b * 255) << 8) | uint(color.a * 255);
}

float4 UnpackColor(uint color) {
    float4 result;
    result.r = float((color >> 24) & 0x000000FF) / 255.0f;
    result.g = float((color >> 16) & 0x000000FF) / 255.0f;
    result.b = float((color >> 8)  & 0x000000FF) / 255.0f;
    result.a = float((color >> 0)  & 0x000000FF) / 255.0f;
    return saturate(result);
}
