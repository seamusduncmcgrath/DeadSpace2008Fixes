// FXAA for the game's EdgeAA pass, replacing its EdgeAAFast / DeferredEdgeAA pixel shaders.
// The quality path of FXAA 3.11 by Timothy Lottes (NVIDIA), ported to plain ps_3_0 HLSL.
//
// The pass (sub_695350) copies the finished, colour graded frame into a texture, sets it to
// linear filtering with clamp, and draws a fullscreen quad back with this shader. The game binds
// parameters by name from this shader's constant table and skips any it can't find, so only these
// two are declared, on the same registers the original shaders used:
//   FXAA_INPUT          the frame copy (bb_texture for EdgeAAFast, ColorTexture for DeferredEdgeAA)
//   g_halfPixelOffset   (0, 0, 0.5 / width, 0.5 / height), set every time the pass runs
//
// Built by build_shaders.bat into FXAA_EdgeAAFast.h and FXAA_DeferredEdgeAA.h.

#ifndef FXAA_INPUT
#define FXAA_INPUT bb_texture
#endif

#ifndef FXAA_HALF_PIXEL_REGISTER
#define FXAA_HALF_PIXEL_REGISTER c1
#endif

//Tuning, the values are explained in the FXAA 3.11 notes.
//How much sub-pixel aliasing gets softened: 0 is off, 0.75 is FXAA's default, 1 is softest.
#define FXAA_SUBPIX 0.5
//Minimum local contrast (relative to the brightest neighbour) before a pixel counts as an edge.
//0.166 is FXAA's default, 0.125 is its high quality setting.
#define FXAA_EDGE_THRESHOLD 0.125
//Edges darker than this are left alone. 0.0625 is FXAA's high quality setting, lower picks up more
//dark edges (Dead Space has a lot of them) but starts smoothing noise too.
#define FXAA_EDGE_THRESHOLD_MIN 0.0625

sampler2D FXAA_INPUT : register(s0);
float4 g_halfPixelOffset : register(FXAA_HALF_PIXEL_REGISTER);

//FXAA quality preset 29: how far each step of the edge-end search moves, in pixels.
#define FXAA_SEARCH_STEPS 12
static const float kSearchStep[FXAA_SEARCH_STEPS] = { 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

float4 Tex(float2 uv)
{
	return tex2Dlod(FXAA_INPUT, float4(uv, 0.0, 0.0));
}

float Luma(float4 colour)
{
	return dot(colour.rgb, float3(0.299, 0.587, 0.114));
}

float4 main(float2 texcoord : TEXCOORD0) : COLOR0
{
	float2 rcpFrame = g_halfPixelOffset.zw * 2.0;

	//the same pixel centre fix the original shaders apply
	float2 posM = texcoord + g_halfPixelOffset.zw;

	float4 rgbyM = Tex(posM);
	float lumaM = Luma(rgbyM);
	float lumaS = Luma(Tex(posM + float2( 0.0,  1.0) * rcpFrame));
	float lumaE = Luma(Tex(posM + float2( 1.0,  0.0) * rcpFrame));
	float lumaN = Luma(Tex(posM + float2( 0.0, -1.0) * rcpFrame));
	float lumaW = Luma(Tex(posM + float2(-1.0,  0.0) * rcpFrame));

	float rangeMax = max(max(lumaN, lumaW), max(lumaE, max(lumaS, lumaM)));
	float rangeMin = min(min(lumaN, lumaW), min(lumaE, min(lumaS, lumaM)));
	float range = rangeMax - rangeMin;

	//not enough contrast to be an edge
	if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
		return rgbyM;

	float lumaNW = Luma(Tex(posM + float2(-1.0, -1.0) * rcpFrame));
	float lumaSE = Luma(Tex(posM + float2( 1.0,  1.0) * rcpFrame));
	float lumaNE = Luma(Tex(posM + float2( 1.0, -1.0) * rcpFrame));
	float lumaSW = Luma(Tex(posM + float2(-1.0,  1.0) * rcpFrame));

	//which way the edge runs
	float lumaNS = lumaN + lumaS;
	float lumaWE = lumaW + lumaE;
	float subpixRcpRange = 1.0 / range;
	float subpixNSWE = lumaNS + lumaWE;
	float edgeHorz1 = (-2.0 * lumaM) + lumaNS;
	float edgeVert1 = (-2.0 * lumaM) + lumaWE;

	float lumaNESE = lumaNE + lumaSE;
	float lumaNWNE = lumaNW + lumaNE;
	float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
	float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;

	float lumaNWSW = lumaNW + lumaSW;
	float lumaSWSE = lumaSW + lumaSE;
	float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
	float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
	float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
	float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
	float edgeHorz = abs(edgeHorz3) + edgeHorz4;
	float edgeVert = abs(edgeVert3) + edgeVert4;

	float subpixNWSWNESE = lumaNWSW + lumaNESE;
	float lengthSign = rcpFrame.x;
	bool horzSpan = edgeHorz >= edgeVert;
	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;

	if (!horzSpan)
	{
		lumaN = lumaW;
		lumaS = lumaE;
	}
	else
	{
		lengthSign = rcpFrame.y;
	}

	float subpixB = (subpixA * (1.0 / 12.0)) - lumaM;

	//which side of the pixel the edge is on
	float gradientN = lumaN - lumaM;
	float gradientS = lumaS - lumaM;
	float lumaNN = lumaN + lumaM;
	float lumaSS = lumaS + lumaM;
	bool pairN = abs(gradientN) >= abs(gradientS);
	float gradient = max(abs(gradientN), abs(gradientS));
	if (pairN)
		lengthSign = -lengthSign;
	float subpixC = saturate(abs(subpixB) * subpixRcpRange);

	float2 posB = posM;
	float2 offNP = horzSpan ? float2(rcpFrame.x, 0.0) : float2(0.0, rcpFrame.y);
	if (!horzSpan)
		posB.x += lengthSign * 0.5;
	else
		posB.y += lengthSign * 0.5;

	float subpixD = ((-2.0) * subpixC) + 3.0;
	float subpixE = subpixC * subpixC;
	if (!pairN)
		lumaNN = lumaSS;
	float gradientScaled = gradient * (1.0 / 4.0);
	float lumaMM = lumaM - lumaNN * 0.5;
	float subpixF = subpixD * subpixE;
	bool lumaMLTZero = lumaMM < 0.0;

	//walk along the edge both ways until each end is found
	float2 posN = posB - offNP * kSearchStep[0];
	float2 posP = posB + offNP * kSearchStep[0];
	float lumaEndN = Luma(Tex(posN)) - lumaNN * 0.5;
	float lumaEndP = Luma(Tex(posP)) - lumaNN * 0.5;
	bool doneN = abs(lumaEndN) >= gradientScaled;
	bool doneP = abs(lumaEndP) >= gradientScaled;
	if (!doneN)
		posN -= offNP * kSearchStep[1];
	if (!doneP)
		posP += offNP * kSearchStep[1];

	[unroll]
	for (int i = 2; i < FXAA_SEARCH_STEPS; i++)
	{
		if (doneN && doneP)
			break;

		if (!doneN)
			lumaEndN = Luma(Tex(posN)) - lumaNN * 0.5;
		if (!doneP)
			lumaEndP = Luma(Tex(posP)) - lumaNN * 0.5;
		doneN = abs(lumaEndN) >= gradientScaled;
		doneP = abs(lumaEndP) >= gradientScaled;
		if (!doneN)
			posN -= offNP * kSearchStep[i];
		if (!doneP)
			posP += offNP * kSearchStep[i];
	}

	//how far to move the sample across the edge
	float dstN = horzSpan ? (posM.x - posN.x) : (posM.y - posN.y);
	float dstP = horzSpan ? (posP.x - posM.x) : (posP.y - posM.y);

	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
	float spanLength = dstP + dstN;
	float spanLengthRcp = 1.0 / spanLength;

	bool directionN = dstN < dstP;
	float dst = min(dstN, dstP);
	bool goodSpan = directionN ? goodSpanN : goodSpanP;
	float subpixG = subpixF * subpixF;
	float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
	float subpixH = subpixG * FXAA_SUBPIX;

	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
	if (!horzSpan)
		posM.x += pixelOffsetSubpix * lengthSign;
	else
		posM.y += pixelOffsetSubpix * lengthSign;

	//keep the frame's own alpha, later passes might use it
	return float4(Tex(posM).rgb, rgbyM.a);
}
