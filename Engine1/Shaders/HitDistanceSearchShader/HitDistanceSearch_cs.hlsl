#pragma pack_matrix(column_major) //informs only about the memory layout of input matrices

#include "Common\Constants.hlsl"
#include "Common\SampleWeighting.hlsl"

cbuffer ConstantBuffer : register( b0 )
{
    float3 cameraPos;
    float  pad1;
    float2 outputTextureSize;
    float2 pad2;
    float2 inputTextureSize;
    float2 pad3;
    float  positionThreshold;
    float3 pad4;
    float  positionDiffMul;
    float3 pad5;
    float  normalDiffMul;
    float3 pad6;
    float  positionNormalThreshold;
    float3 pad7;
};

SamplerState g_linearSamplerState;
SamplerState g_pointSamplerState;

// Input.
Texture2D<float4> g_positionTexture     : register( t0 );
Texture2D<float4> g_normalTexture       : register( t1 ); 
Texture2D<float>  g_hitDistanceTexture  : register( t2 );

// Input / Output.
RWTexture2D<float> g_finalDistToOccluder : register( u0 );

static const float maxHitDistance = 50.0f; // Should be less than the initial ray length. 

void sampleWeightedHitDistance( 
    const SamplerState samplerState, const float2 texcoords, const float mipmap, 
    const float2 centerTexcoords, const float centerSampleValue, const float3 centerPosition, const float3 centerNormal, 
    out float sampleValue, out float sampleWeight 
);

// SV_GroupID - group id in the whole computation.
// SV_GroupThreadID - thread id within its group.
// SV_DispatchThreadID - thread id in the whole computation.
// SV_GroupIndex - index of the group within the whole computation.
[numthreads(16, 16, 1)]
void main( uint3 groupId : SV_GroupID,
           uint3 groupThreadId : SV_GroupThreadID,
           uint3 dispatchThreadId : SV_DispatchThreadID,
           uint  groupIndex : SV_GroupIndex )
{
    // Note: Calculate texcoords for the pixel center.
    const float2 texcoords = ((float2)dispatchThreadId.xy + 0.5f) / outputTextureSize;

    const float2 inputPixelSize0 = 1.0f / inputTextureSize;

    const float3 centerPosition     = g_positionTexture.SampleLevel( g_pointSamplerState, texcoords, 0.0 ).xyz;
    const float3 centerNormal       = g_normalTexture.SampleLevel( g_pointSamplerState, texcoords, 0.0 ).xyz;

    const float3 vectorToCamera = cameraPos - centerPosition;
    const float3 dirToCamera    = normalize( vectorToCamera );
    const float  distToCamera   = length( vectorToCamera );

    //#TODO: Select mipmap based on central hitDistance - maybe a log of what we need - the rest through avaraging.
    // Why reflection doesn't blur through edges?
    // Check linear/point sampling.
    // Avarage pixels from some region.

    const float centerHitDistance = min( maxHitDistance, g_hitDistanceTexture.SampleLevel( g_pointSamplerState, texcoords, 0.0f ) );
    
    float mipmap          = 2.0f;
    float hitDistance     = 0.0f;
    float sampleWeightSum = 0.0001f;

    do
    {
        // #TODO: If only mipmap increases by one - could be optimized through multiplication by 2 instead of calculating power.
        const float2 inputPixelSize = inputPixelSize0 * pow( 2.0f, mipmap );

        const float searchRadius = 5.0f;
        for ( float y = -searchRadius; y <= searchRadius; y += 1.0f )
        {
            for ( float x = -searchRadius; x <= searchRadius; x += 1.0f )
            {
                const float2 sampleTexcoords = texcoords + float2( x * inputPixelSize.x, y * inputPixelSize.y );

                float sampleHitDistance = 0.0f;
                float sampleWeight      = 0.0f;

                sampleWeightedHitDistance( 
                    g_pointSamplerState, sampleTexcoords, mipmap, 
                    texcoords, centerHitDistance, centerPosition, centerNormal, 
                    sampleHitDistance, sampleWeight 
                );

                hitDistance     += min( maxHitDistance, sampleHitDistance ) * sampleWeight;
                sampleWeightSum += sampleWeight;
            }
        }

        mipmap += 1.0;

    } while ( mipmap <= 6.0 && sampleWeightSum < 0.001f );

    hitDistance /= sampleWeightSum;

    g_finalDistToOccluder[ dispatchThreadId.xy ] = hitDistance;
}

void sampleWeightedHitDistance( 
    const SamplerState samplerState, const float2 texcoords, const float mipmap, 
    const float2 centerTexcoords, const float centerSampleValue, const float3 centerPosition, const float3 centerNormal, 
    out float sampleValue, out float sampleWeight )
{
    sampleValue = /*min( maxHitDistance, */g_hitDistanceTexture.SampleLevel( samplerState, texcoords, mipmap ) /*)*/;

    // Weight depanding on difference in position/normal between center and the sample.
    const float3 samplePosition = g_positionTexture.SampleLevel( g_pointSamplerState, texcoords, 0.0f ).xyz; 
    const float3 sampleNormal   = g_normalTexture.SampleLevel( g_pointSamplerState, texcoords, 0.0f ).xyz; 


    const float  positionDiff   = length( samplePosition - centerPosition );
    const float  normalDiff     = 1.0f - dot(centerNormal, sampleNormal);
    //const float  power          = -( positionDiffMul * positionDiff * positionDiff + normalDiffMul * normalDiff ) / positionNormalThreshold;
    //const float  sampleWeight2  = pow( e, power );
    const float samplesHitDistDiff = /*max( 0.0f, */abs(sampleValue - centerSampleValue)/* )*/;

    //const float samplesPosNormDiff = positionDiffMul * positionDiff * positionDiff + normalDiffMul * normalDiff;
    const float samplesPosNormDiff = positionDiffMul * samplesHitDistDiff * samplesHitDistDiff /*+ normalDiffMul * normalDiff*/;
    const float sampleWeight1 = 1.0f;//1.0f - positionDiffMul * ( sampleHitDistance / centerHitDistance );//max(0.0f, 1.0f - (positionDiffMul * samplesHitDistDiff));//getSampleWeightSimilarSmooth( samplesPosNormDiff, positionNormalThreshold );

    // Weight diminishing importance of samples hitting the sky if any other samples are available.
    const float sampleWeight2 = max( 0.0f, maxHitDistance - sampleValue );
	//if ( sampleHitDistance > maxHitDistance)
	//    sampleWeight1 = 0.0f;

    // Discard samples which are off-screen (zero dist-to-occluder).
    const float sampleWeight3 = getSampleWeightGreaterThan( sampleValue, 0.0f );

    // Lower the weight of samples, which are far from center in screen-space 
    // (to fucus on near samples if they are available, or on far samples if they are the only ones available).
    const float texcoordDiff = length(centerTexcoords - texcoords);
    const float sampleWeight4 = 1.0f - (texcoordDiff * texcoordDiff);

    sampleWeight = sampleWeight1 * sampleWeight2 * sampleWeight3 * sampleWeight4;
}