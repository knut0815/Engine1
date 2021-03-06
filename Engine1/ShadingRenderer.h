#pragma once

#include <wrl.h>
#include <memory>

#include "Texture2DTypes.h"

#include "uchar4.h"
#include "float2.h"

struct ID3D11Device3;
struct ID3D11DeviceContext3;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;

namespace Engine1
{
    class DX11RendererCore;
    class ShadingComputeShader0;
    class ShadingComputeShader;
    class ShadingComputeShader2;
    class ShadingNoShadowsComputeShader;
    class ShadingNoShadowsComputeShader2;
    class Light;
    class Camera;

    class ShadingRenderer
    {
        public:

        ShadingRenderer( DX11RendererCore& rendererCore );
        ~ShadingRenderer();

        void initialize( int imageWidth, int imageHeight, Microsoft::WRL::ComPtr< ID3D11Device3 > device, 
                         Microsoft::WRL::ComPtr< ID3D11DeviceContext3 > deviceContext );

        void performEmissiveShading( 
            std::shared_ptr< RenderTargetTexture2D< float4 > > colorRenderTarget,
            const std::shared_ptr< Texture2D< uchar4 > > emissiveTexture 
        );

        // With shadows.
        void performShading( 
            const Camera& camera,
            std::shared_ptr< RenderTargetTexture2D< float4 > > colorRenderTarget,
            const std::shared_ptr< Texture2D< float4 > > positionTexture,
            const std::shared_ptr< Texture2D< uchar4 > > albedoTexture,
            const std::shared_ptr< Texture2D< unsigned char > > metalnessTexture,
            const std::shared_ptr< Texture2D< unsigned char > > roughnessTexture,
            const std::shared_ptr< Texture2D< float4 > > normalTexture,
            const std::shared_ptr< Texture2D< unsigned char > > shadowTexture,
            const Light& light 
        );

        // Without shadows.
        void performShadingNoShadows( 
            const Camera& camera,
            std::shared_ptr< RenderTargetTexture2D< float4 > > colorRenderTarget,
            const std::shared_ptr< Texture2D< float4 > > positionTexture,
            const std::shared_ptr< Texture2D< uchar4 > > albedoTexture,
            const std::shared_ptr< Texture2D< unsigned char > > metalnessTexture,
            const std::shared_ptr< Texture2D< unsigned char > > roughnessTexture,
            const std::shared_ptr< Texture2D< float4 > > normalTexture,
            const std::shared_ptr< Texture2D< unsigned char > > ambientOcclusionTexture,
            const std::vector< std::shared_ptr< Light > > lights 
        );

        // With shadows.
        void performShading( 
            std::shared_ptr< RenderTargetTexture2D< float4 > > colorRenderTarget,
            const std::shared_ptr< Texture2D< float4 > > rayOriginTexture,
            const std::shared_ptr< Texture2D< float4 > > rayHitPositionTexture,
            const std::shared_ptr< Texture2D< uchar4 > > rayHitAlbedoTexture,
            const std::shared_ptr< Texture2D< unsigned char > > rayHitMetalnessTexture,
            const std::shared_ptr< Texture2D< unsigned char > > rayHitRoughnessTexture,
            const std::shared_ptr< Texture2D< float4 > > rayHitNormalTexture,
            const std::shared_ptr< Texture2D< unsigned char > > shadowTexture,
            const Light& light 
        );

        // Without shadows.
        void performShadingNoShadows( 
            std::shared_ptr< RenderTargetTexture2D< float4 > > colorRenderTarget,
            const std::shared_ptr< Texture2D< float4 > > rayOriginTexture,
            const std::shared_ptr< Texture2D< float4 > > rayHitPositionTexture,
            const std::shared_ptr< Texture2D< uchar4 > > rayHitAlbedoTexture,
            const std::shared_ptr< Texture2D< unsigned char > > rayHitMetalnessTexture,
            const std::shared_ptr< Texture2D< unsigned char > > rayHitRoughnessTexture,
            const std::shared_ptr< Texture2D< float4 > > rayHitNormalTexture,
            const std::vector< std::shared_ptr< Light > > lights 
        );

        private:

        DX11RendererCore& m_rendererCore;

        Microsoft::WRL::ComPtr< ID3D11Device3 >        m_device;
        Microsoft::WRL::ComPtr< ID3D11DeviceContext3 > m_deviceContext;

        bool m_initialized;

        // Render targets.
        int m_imageWidth, m_imageHeight;

        // Shaders.
        std::shared_ptr< ShadingComputeShader0 > m_shadingComputeShader0;
        std::shared_ptr< ShadingComputeShader >  m_shadingComputeShader;
        std::shared_ptr< ShadingComputeShader2 > m_shadingComputeShader2;
        std::shared_ptr< ShadingNoShadowsComputeShader > m_shadingNoShadowsComputeShader;
        std::shared_ptr< ShadingNoShadowsComputeShader2 > m_shadingNoShadowsComputeShader2;

        void loadAndCompileShaders( Microsoft::WRL::ComPtr< ID3D11Device3 >& device );

        // Copying is not allowed.
        ShadingRenderer( const ShadingRenderer& )           = delete;
        ShadingRenderer& operator=(const ShadingRenderer& ) = delete;
    };
}

