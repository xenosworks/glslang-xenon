// DX9's boolean and integer registers, each a $Global member's UserSemantic beside any semantic it has; a cbuffer's
// b# stays its binding.
bool Flip : register(b3);
bool Tint : TINT : register(b5);
int Count : register(i2);
float4 Step : register(c0);
cbuffer Material : register(b0) { float4 Colour; };

float4 main(float4 uv : TEXCOORD0) : COLOR0
{
    float4 c = uv;
    for (int i = 0; i < Count; i++)
        c += Step;
    if (Flip)
        c = c.wzyx;
    return c + (Tint ? Colour : 0);
}
