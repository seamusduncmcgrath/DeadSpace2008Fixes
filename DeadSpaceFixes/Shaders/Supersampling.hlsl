// Scales the supersampled frame down to the back buffer for Patches/Supersampling.cpp.
//
// Each back buffer pixel averages the frame pixels it covers. TAPS x TAPS bilinear samples are spread
// evenly over that footprint, so at whole number scales up to TAPS every frame pixel gets the same
// weight (a box filter), and in between the bilinear filtering blends the edges of the footprint.
// With sRGB reads and writes switched on the averaging happens in linear light.
//
// Built by build_shaders.bat into Supersampling.h: one vertex shader and a pixel shader per tap count.

#ifndef TAPS
#define TAPS 2
#endif

sampler2D g_Frame : register(s0);

// vertex shader, xy: half a back buffer pixel in clip space (-1/width, 1/height), which lines pixels up with texels
float4 g_PixelOffset : register(c0);
// pixel shader
// xy: one back buffer pixel in texture coordinates (1/width, 1/height)
float4 g_DestPixel : register(c0);

struct VSOutput
{
	float4 position : POSITION;
	float2 uv : TEXCOORD0;
};

// Draws one triangle that covers the screen, the vertices are in clip space
VSOutput VSMain(float2 position : POSITION)
{
	VSOutput output;
	output.position = float4(position + g_PixelOffset.xy, 0.0, 1.0);
	output.uv = position * float2(0.5, -0.5) + 0.5;
	return output;
}

float4 PSMain(float2 uv : TEXCOORD0) : COLOR0
{
	float3 total = 0.0;

	[unroll] for (int y = 0; y < TAPS; y++)
	{
		[unroll] for (int x = 0; x < TAPS; x++)
		{
			float2 offset = (float2(x, y) + 0.5) / TAPS - 0.5;
			total += tex2Dlod(g_Frame, float4(uv + offset * g_DestPixel.xy, 0.0, 0.0)).rgb;
		}
	}

	return float4(total / (TAPS * TAPS), 1.0);
}
