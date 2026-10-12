#include <metal_stdlib>
using namespace metal;
struct VertexOut { float4 position [[position]]; float2 uv; };
vertex VertexOut dxVertex(uint id [[vertex_id]]) {
    const float2 positions[] = {float2(-1, 1), float2(-1, -1), float2(1, 1), float2(1, -1)};
    const float2 coords[] = {float2(0, 0), float2(0, 1), float2(1, 0), float2(1, 1)};
    return {float4(positions[id], 0, 1), coords[id]};
}
fragment float4 dxFragment(VertexOut in [[stage_in]], texture2d<float> screen [[texture(0)]]) {
    constexpr sampler pixels(coord::normalized, address::clamp_to_edge, filter::nearest);
    return float4(screen.sample(pixels, in.uv).rgb, 1);
}
