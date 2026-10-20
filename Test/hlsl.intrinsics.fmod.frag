// fmod truncates its quotient, so its result has the sign of x: -1.5 and 1.5 here, where mod's floor gives 0.5
// and -0.5, folded as constants and worked out alike, for vectors and each column of a matrix.
float4 main(float4 a : TEXCOORD0, float4 b : TEXCOORD1, float2x2 m : TEXCOORD2) : SV_Target0
{
    float2x2 r = fmod(m, float2x2(b.x, b.y, b.z, b.w));
    return float4(fmod(-7.5, 2.0), fmod(7.5, -2.0), r[0][1], r[1][0]) + fmod(a, b);
}
