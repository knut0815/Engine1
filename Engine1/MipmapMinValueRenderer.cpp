#include "MipmapMinValueRenderer.h"

#include "Direct3DRendererCore.h"

#include "GenerateMipmapMinValueComputeShader.h"
#include "GenerateMipmapMinValueVertexShader.h"
#include "GenerateMipmapMinValueFragmentShader.h"

#include <d3d11.h>

using namespace Engine1;

using Microsoft::WRL::ComPtr;

MipmapMinValueRenderer::MipmapMinValueRenderer( Direct3DRendererCore& rendererCore ) :
    m_rendererCore( rendererCore ),
    m_initialized( false ),
    m_generateMipmapMinValueComputeShader( std::make_shared< GenerateMipmapMinValueComputeShader >() ),
    m_generateMipmapMinValueVertexShader( std::make_shared< GenerateMipmapMinValueVertexShader >() ),
    m_generateMipmapMinValueFragmentShader( std::make_shared< GenerateMipmapMinValueFragmentShader >() )
{}

MipmapMinValueRenderer::~MipmapMinValueRenderer()
{}

void MipmapMinValueRenderer::initialize( ComPtr< ID3D11Device > device,
                                         ComPtr< ID3D11DeviceContext > deviceContext )
{
    this->m_device        = device;
    this->m_deviceContext = deviceContext;

    m_rasterizerState = createRasterizerState( *device.Get() );
    m_blendState      = createBlendState( *device.Get() );

    loadAndCompileShaders( device );

    { // Load default rectangle mesh to GPU.
        m_rectangleMesh.loadCpuToGpu( *device.Get() );
    }

    m_initialized = true;
}

//void MipmapMinValueRenderer::generateMipmapsMinValue( std::shared_ptr< Texture2D< TexUsage::Default, TexBind::RenderTarget_UnorderedAccess_ShaderResource, float > >& texture )
//{
//    m_rendererCore.disableRenderingPipeline();
//
//    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::UnorderedAccess, float > > >         unorderedAccessTargetsF1;
//    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::UnorderedAccess, float2 > > >        unorderedAccessTargetsF2;
//    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::UnorderedAccess, float4 > > >        unorderedAccessTargetsF4;
//    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::UnorderedAccess, unsigned char > > > unorderedAccessTargetsU1;
//    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::UnorderedAccess, uchar4 > > >        unorderedAccessTargetsU4;
//
//    unorderedAccessTargetsF1.push_back( texture );
//
//    const int mipmapCount = texture->getMipMapCountOnGpu();
//
//    for ( int srcMipmapLevel = 0; srcMipmapLevel + 1 < mipmapCount; ++srcMipmapLevel )
//    {
//        const int destMipmapLevel = srcMipmapLevel + 1;
//
//        m_rendererCore.enableUnorderedAccessTargets( unorderedAccessTargetsF1, unorderedAccessTargetsF2, unorderedAccessTargetsF4,
//                                                     unorderedAccessTargetsU1, unorderedAccessTargetsU4, destMipmapLevel );
//
//        m_generateMipmapMinValueComputeShader->setParameters( *m_deviceContext.Get(), *texture, srcMipmapLevel );
//
//        m_rendererCore.enableComputeShader( m_generateMipmapMinValueComputeShader );
//
//        uint3 groupCount( texture->getWidth( destMipmapLevel ) / 8, texture->getHeight( destMipmapLevel ) / 8, 1 );
//
//        m_rendererCore.compute( groupCount );
//    }
//
//    m_generateMipmapMinValueComputeShader->unsetParameters( *m_deviceContext.Get() );
//
//    m_rendererCore.disableComputePipeline();
//}

void MipmapMinValueRenderer::generateMipmapsMinValue( std::shared_ptr< Texture2D< TexUsage::Default, TexBind::RenderTarget_UnorderedAccess_ShaderResource, float > >& texture )
{
    if ( !m_initialized ) 
        throw std::exception( "MipmapMinValueRenderer::generateMipmapsMinValue - renderer not initialized." );


    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::RenderTarget, float > > >         renderTargetsF1;
    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::RenderTarget, float2 > > >        renderTargetsF2;
    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::RenderTarget, float4 > > >        renderTargetsF4;
    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::RenderTarget, unsigned char > > > renderTargetsU1;
    std::vector< std::shared_ptr< Texture2DSpecBind< TexBind::RenderTarget, uchar4 > > >        renderTargetsU4;

    renderTargetsF1.push_back( texture );

    const int mipmapCount = texture->getMipMapCountOnGpu();

    m_rendererCore.enableRasterizerState( *m_rasterizerState.Get() );
    m_rendererCore.enableBlendState( *m_blendState.Get() );
    m_rendererCore.enableRenderingShaders( m_generateMipmapMinValueVertexShader, m_generateMipmapMinValueFragmentShader );

    for ( int srcMipmapLevel = 0; srcMipmapLevel + 1 < mipmapCount; ++srcMipmapLevel ) 
    {
        const int destMipmapLevel = srcMipmapLevel + 1;

        m_rendererCore.setViewport( (float2)texture->getDimensions( destMipmapLevel ) );

        m_rendererCore.enableRenderTargets( renderTargetsF1, renderTargetsF2, renderTargetsF4, renderTargetsU1, renderTargetsU4, nullptr, destMipmapLevel );

        m_generateMipmapMinValueVertexShader->setParameters( *m_deviceContext.Get() );
        m_generateMipmapMinValueFragmentShader->setParameters( *m_deviceContext.Get(), *texture, srcMipmapLevel );

        m_rendererCore.draw( m_rectangleMesh );
    }

    m_generateMipmapMinValueFragmentShader->unsetParameters( *m_deviceContext.Get() );

    m_rendererCore.disableRenderTargetViews();
}

ComPtr<ID3D11RasterizerState> MipmapMinValueRenderer::createRasterizerState( ID3D11Device& device )
{
    D3D11_RASTERIZER_DESC         rasterDesc;
    ComPtr<ID3D11RasterizerState> rasterizerState;

    ZeroMemory( &rasterDesc, sizeof( rasterDesc ) );

    rasterDesc.AntialiasedLineEnable = false;
    rasterDesc.CullMode              = D3D11_CULL_NONE; // Culling disabled.
    rasterDesc.DepthBias             = 0;
    rasterDesc.DepthBiasClamp        = 0.0f;
    rasterDesc.DepthClipEnable       = false; // Depth test disabled.
    rasterDesc.FillMode              = D3D11_FILL_SOLID;
    rasterDesc.FrontCounterClockwise = false;
    rasterDesc.MultisampleEnable     = false;
    rasterDesc.ScissorEnable         = false;
    rasterDesc.SlopeScaledDepthBias  = 0.0f;

    HRESULT result = device.CreateRasterizerState( &rasterDesc, rasterizerState.ReleaseAndGetAddressOf() );
    if ( result < 0 ) throw std::exception( "MipmapMinValueRenderer::createRasterizerState - creation of rasterizer state failed" );

    return rasterizerState;
}

ComPtr<ID3D11BlendState> MipmapMinValueRenderer::createBlendState( ID3D11Device& device )
{
    ComPtr<ID3D11BlendState> blendState;
    D3D11_BLEND_DESC         blendDesc;

    ZeroMemory( &blendDesc, sizeof( blendDesc ) );

    blendDesc.AlphaToCoverageEnable  = false;
    blendDesc.IndependentBlendEnable = false;

    // Disable blending.
    blendDesc.RenderTarget[ 0 ].BlendEnable           = false;
    blendDesc.RenderTarget[ 0 ].SrcBlend              = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[ 0 ].DestBlend             = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[ 0 ].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[ 0 ].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[ 0 ].DestBlendAlpha        = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[ 0 ].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE; // Don't write alpha.

    HRESULT result = device.CreateBlendState( &blendDesc, blendState.ReleaseAndGetAddressOf() );
    if ( result < 0 ) 
        throw std::exception( "MipmapMinValueRenderer::createBlendState - creation of blend state failed." );

    return blendState;
}

void MipmapMinValueRenderer::loadAndCompileShaders( ComPtr< ID3D11Device >& device )
{
    m_generateMipmapMinValueComputeShader->loadAndInitialize( "Shaders/GenerateMipmapMinValueShader/GenerateMipmapMinValue_cs.cso", device );
    m_generateMipmapMinValueVertexShader->loadAndInitialize( "Shaders/GenerateMipmapMinValueShader/GenerateMipmapMinValue_vs.cso", device );
    m_generateMipmapMinValueFragmentShader->loadAndInitialize( "Shaders/GenerateMipmapMinValueShader/GenerateMipmapMinValue_ps.cso", device );
}
