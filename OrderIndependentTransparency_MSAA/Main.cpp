#include <memory>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <cmath>
#include <utility>
#include <string>

#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <SDL.h>
#include <SDL_syswm.h>



namespace DX {

	class ComException : public std::exception {
	public:
		ComException(HRESULT hr) : m_Result(hr) {}

		const char* what() const override {
			static char s_str[64] = {};
			sprintf_s(s_str, "Failure with HRESULT of %08X" ,static_cast<uint32_t>(m_Result));
			return s_str;
		}
	private:
		HRESULT m_Result;
	};

	inline auto ThrowIfFailed(HRESULT hr) -> void {
		if (FAILED(hr))	
			throw ComException(hr);
	}

	inline auto CompileShader(std::wstring const& fileName, std::string const& entryPoint, std::string const& target, std::vector<std::pair<std::string, std::string>> const& defines) -> Microsoft::WRL::ComPtr<ID3DBlob> {
		Microsoft::WRL::ComPtr<ID3DBlob> pCodeBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

		uint32_t shaderFlags = 0;
#ifdef _DEBUG
		shaderFlags |= D3DCOMPILE_DEBUG;
		shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		shaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
#endif

		std::vector<D3D_SHADER_MACRO> d3dDefines;
		for(auto const& e: defines)
			d3dDefines.push_back({ e.first.c_str(), e.second.c_str() });

		d3dDefines.push_back({ nullptr, nullptr });

		if (FAILED(D3DCompileFromFile(fileName.c_str(), std::data(d3dDefines), D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target.c_str(), shaderFlags, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
		{
			std::printf(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
				throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
		}
		
		
		return pCodeBlob;
	}

	template<typename T>
	auto CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D11Device> pDevice) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {

		Microsoft::WRL::ComPtr<ID3D11Buffer> pBuffer;
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(T);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		ThrowIfFailed(pDevice->CreateBuffer(&desc, nullptr, pBuffer.GetAddressOf()));
		return pBuffer;
	}

	template<typename T>
	auto CreateStructuredBuffer(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, uint32_t numElements, bool isCPUWritable, bool isGPUWritable, const T* pInitialData = nullptr) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {

		Microsoft::WRL::ComPtr<ID3D11Buffer> pBuffer;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(T) * numElements;
		if ((!isCPUWritable) && (!isGPUWritable)) {
			desc.CPUAccessFlags = 0;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
		}
		else if (isCPUWritable && (!isGPUWritable)) {
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_DYNAMIC;
		}
		else if ((!isCPUWritable) && isGPUWritable) {

			desc.CPUAccessFlags = 0;
			desc.BindFlags = (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
			desc.Usage = D3D11_USAGE_DEFAULT;
		}
		else {
			assert((!(isCPUWritable && isGPUWritable)));
		}

		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(T);

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = pInitialData;
		ThrowIfFailed(pDevice->CreateBuffer((&desc), (pInitialData) ? (&data) : nullptr, pBuffer.GetAddressOf()));
		return pBuffer;
	}

	class MSAAResolver{
	public:
		auto Apply(Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTVSrc, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTVDsv, DXGI_FORMAT format) const -> void {
			Microsoft::WRL::ComPtr<ID3D11Resource> pTexture;
			pRTVDsv->GetResource(pTexture.GetAddressOf());

			Microsoft::WRL::ComPtr<ID3D11Resource> pTexture_MSAA;
			pRTVSrc->GetResource(pTexture_MSAA.GetAddressOf());

			pDeviceContext->ResolveSubresource(pTexture.Get(), 0, pTexture_MSAA.Get(), 0, format);
		}

		auto Apply(Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDSVSrc, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDSVDsv, DXGI_FORMAT format) const-> void {
			static_assert(true, "No implementation");
		}

	};

	class GraphicsPSO { 
	public:
		auto Apply(Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
			pDeviceContext->IASetPrimitiveTopology(PrimitiveTopology);
			pDeviceContext->IASetInputLayout(pInputLayout.Get());
			pDeviceContext->VSSetShader(pVS.Get(), nullptr, 0);
			pDeviceContext->PSSetShader(pPS.Get(), nullptr, 0);
			pDeviceContext->RSSetState(pRasterState.Get());
			pDeviceContext->OMSetDepthStencilState(pDepthStencilState.Get(), 0);
			pDeviceContext->OMSetBlendState(pBlendState.Get(), nullptr, BlendMask);		
		}
	public:
		Microsoft::WRL::ComPtr<ID3D11InputLayout>       pInputLayout = nullptr;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>      pVS = nullptr;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>       pPS = nullptr;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState>   pRasterState = nullptr;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDepthStencilState = nullptr;
		Microsoft::WRL::ComPtr<ID3D11BlendState>        pBlendState  = nullptr;
		uint32_t                                        BlendMask = 0xFFFFFFFF;
		D3D11_PRIMITIVE_TOPOLOGY                        PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};

	class ComputePSO {
	public:
		auto Apply(Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
			pDeviceContext->CSSetShader(pCS.Get(), nullptr, 0);
		}
	public:
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> pCS = nullptr;
	};

}




#undef main
int main(int argc, char* argv)
{





	auto const WINDOW_TITLE  = "OrderIndependentTransparency MSAA";
	auto const WINDOW_WIDTH  = 1920;
	auto const WINDOW_HEIGHT = 1280;
	auto const MSAA_SAMPLES    = 4;
	auto const FRAGMENT_COUNT  = 32;
	auto const OIT_LAYER_COUNT = 8;

	SDL_Init(SDL_INIT_EVERYTHING);
	std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> pWindow(
		SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE),
		SDL_DestroyWindow);

	SDL_SysWMinfo windowInfo{};
	SDL_GetWindowWMInfo(pWindow.get(), &windowInfo);

	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext;

	DXGI_FORMAT colorBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	{
		int32_t width  = 0;
		int32_t height = 0;
		SDL_GetWindowSize(pWindow.get(), &width, &height);

		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferCount = 2;
		desc.BufferDesc.Width = width;
		desc.BufferDesc.Height = height;
		desc.BufferDesc.Format = colorBufferFormat;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferUsage =  DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
		desc.OutputWindow = windowInfo.info.win.window;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Windowed = true;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		D3D_FEATURE_LEVEL pFutureLevel[] = { D3D_FEATURE_LEVEL_11_0 };
		uint32_t createFlag = 0;

#ifdef _DEBUG
		createFlag |= D3D11_CREATE_DEVICE_DEBUG;
		createFlag |= D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;
#endif

		DX::ThrowIfFailed(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlag, pFutureLevel, _countof(pFutureLevel), D3D11_SDK_VERSION, &desc,
			pSwapChain.GetAddressOf(),
			pDevice.GetAddressOf(), nullptr,
			nullptr));

		pDevice->GetImmediateContext(pDeviceContext.GetAddressOf());
	}



	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    pRTVSwapChain;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    pRTV_MSAA;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>    pDSV_MSAA;

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVTextureHeadOIT;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  pSRVTextureHeadOIT;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVBufferLinkedListOIT;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  pSRVBufferLinkedListOIT;

	auto const ResizeRenderTargets = [&](uint32_t width, uint32_t height)-> void {

		pRTVSwapChain.Reset();
		pUAVSwapChain.Reset();
		DX::ThrowIfFailed(pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

	
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
		{
			DX::ThrowIfFailed(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(pBackBuffer.GetAddressOf())));
			DX::ThrowIfFailed(pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, pRTVSwapChain.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(pDevice->CreateUnorderedAccessView(pBackBuffer.Get(), nullptr, pUAVSwapChain.ReleaseAndGetAddressOf()));
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBufferMSAA;
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.ArraySize = 1;
			desc.MipLevels = 1;
			desc.Width = width;
			desc.Height = height;
			desc.Format = colorBufferFormat;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;;
			desc.SampleDesc.Count = MSAA_SAMPLES;
			desc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
			desc.Usage = D3D11_USAGE_DEFAULT;
			DX::ThrowIfFailed(pDevice->CreateTexture2D(&desc, nullptr, pBackBufferMSAA.GetAddressOf()));
			DX::ThrowIfFailed(pDevice->CreateRenderTargetView(pBackBufferMSAA.Get(), nullptr, pRTV_MSAA.ReleaseAndGetAddressOf()));
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pDepthBufferMSAA;
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.ArraySize = 1;
			desc.MipLevels = 1;
			desc.Width = width;
			desc.Height = height;
			desc.Format = depthBufferFormat;
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			desc.SampleDesc.Count = MSAA_SAMPLES;
			desc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
			desc.Usage = D3D11_USAGE_DEFAULT;
			DX::ThrowIfFailed(pDevice->CreateTexture2D(&desc, nullptr, pDepthBufferMSAA.GetAddressOf()));
			DX::ThrowIfFailed(pDevice->CreateDepthStencilView(pDepthBufferMSAA.Get(), nullptr, pDSV_MSAA.ReleaseAndGetAddressOf()));
		}
		
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureOIT;
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.ArraySize = 1;
			desc.MipLevels = 1;
			desc.Width = width;
			desc.Height = height;
			desc.Format = DXGI_FORMAT_R32_UINT;
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			DX::ThrowIfFailed(pDevice->CreateTexture2D(&desc, nullptr, pTextureOIT.GetAddressOf()));
			DX::ThrowIfFailed(pDevice->CreateUnorderedAccessView(pTextureOIT.Get(), nullptr, pUAVTextureHeadOIT.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(pDevice->CreateShaderResourceView(pTextureOIT.Get(), nullptr, pSRVTextureHeadOIT.ReleaseAndGetAddressOf()));
		}

		struct ListNode {
			uint32_t  Next;
			uint32_t  Color;
			uint32_t  Depth;
			uint32_t  Coverage;
		};

		Microsoft::WRL::ComPtr<ID3D11Buffer> pBufferOIT = DX::CreateStructuredBuffer<ListNode>(pDevice, width * height * OIT_LAYER_COUNT, false, true);;
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
			desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
			desc.Buffer.NumElements = width * height * OIT_LAYER_COUNT;
			DX::ThrowIfFailed(pDevice->CreateUnorderedAccessView(pBufferOIT.Get(), &desc, pUAVBufferLinkedListOIT.ReleaseAndGetAddressOf()));
		}

		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.NumElements = width * height * OIT_LAYER_COUNT;
			DX::ThrowIfFailed(pDevice->CreateShaderResourceView(pBufferOIT.Get(), &desc, pSRVBufferLinkedListOIT.ReleaseAndGetAddressOf()));
		}

		

	};
	ResizeRenderTargets(WINDOW_WIDTH, WINDOW_HEIGHT);

	auto pMSAAResolver           = std::make_unique<DX::MSAAResolver>();
	auto pPSOGeometryOpaque      = std::make_unique<DX::GraphicsPSO>();
	auto pPSOGeometryTransparent = std::make_unique<DX::GraphicsPSO>();
	auto pPSOGeometryResolve     = std::make_unique<DX::ComputePSO>();


	//Create PSO opaque
	{	
		Microsoft::WRL::ComPtr<ID3D11VertexShader> pVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>  pPS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> pRasterState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDepthStencilState;
		Microsoft::WRL::ComPtr<ID3D11BlendState> pBlendState;


		auto const pBlobVS = DX::CompileShader(L"Shaders/OpaqueGeometry.hlsl", "VSMain", "vs_5_0", {});
		auto const pBlobPS = DX::CompileShader(L"Shaders/OpaqueGeometry.hlsl", "PSMain", "ps_5_0", {});

		DX::ThrowIfFailed(pDevice->CreateVertexShader(pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), nullptr, pVS.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(pDevice->CreatePixelShader(pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), nullptr, pPS.ReleaseAndGetAddressOf()));

		{	
			D3D11_RASTERIZER_DESC desc = {};
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_BACK;
			desc.FrontCounterClockwise = true;
			desc.DepthClipEnable = true;
			DX::ThrowIfFailed(pDevice->CreateRasterizerState(&desc, pRasterState.GetAddressOf()));
		}

		{		
			D3D11_DEPTH_STENCIL_DESC desc = {};
			desc.DepthEnable = true;
			desc.StencilEnable = false;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_LESS;
			DX::ThrowIfFailed(pDevice->CreateDepthStencilState(&desc, pDepthStencilState.GetAddressOf()));
		}

		{		
			D3D11_BLEND_DESC desc = {};
			desc.AlphaToCoverageEnable = false;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			DX::ThrowIfFailed(pDevice->CreateBlendState(&desc, pBlendState.GetAddressOf()));
		}

		pPSOGeometryOpaque->pInputLayout = nullptr;
		pPSOGeometryOpaque->pVS = pVS;
		pPSOGeometryOpaque->pPS = pPS;
		pPSOGeometryOpaque->pRasterState = pRasterState;
		pPSOGeometryOpaque->pDepthStencilState = pDepthStencilState;
		pPSOGeometryOpaque->pBlendState = pBlendState;
	}

	//Create PSO transparent 
	{
		
		Microsoft::WRL::ComPtr<ID3D11VertexShader> pVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>  pPS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> pRasterState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDepthStencilState;
		Microsoft::WRL::ComPtr<ID3D11BlendState> pBlendState;

		auto const pBlobVS = DX::CompileShader(L"Shaders/TransparentGeometry.hlsl", "VSMain", "vs_5_0", {});
		auto const pBlobPS = DX::CompileShader(L"Shaders/TransparentGeometry.hlsl", "PSMain", "ps_5_0", {});

		DX::ThrowIfFailed(pDevice->CreateVertexShader(pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), nullptr, pVS.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(pDevice->CreatePixelShader(pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), nullptr, pPS.ReleaseAndGetAddressOf()));

		{
			D3D11_RASTERIZER_DESC desc = {};
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_NONE;
			desc.FrontCounterClockwise = true;
			desc.DepthClipEnable = true;
			desc.MultisampleEnable = true;
			DX::ThrowIfFailed(pDevice->CreateRasterizerState(&desc, pRasterState.GetAddressOf()));
		}

		{
			D3D11_DEPTH_STENCIL_DESC desc = {};
			desc.DepthEnable = true;
			desc.StencilEnable = false;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = D3D11_COMPARISON_LESS;
			DX::ThrowIfFailed(pDevice->CreateDepthStencilState(&desc, pDepthStencilState.GetAddressOf()));
		}

		{
			D3D11_BLEND_DESC desc = {};
			desc.AlphaToCoverageEnable = false;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].RenderTargetWriteMask = 0;
			DX::ThrowIfFailed(pDevice->CreateBlendState(&desc, pBlendState.GetAddressOf()));
		}

		pPSOGeometryTransparent->pInputLayout = nullptr;
		pPSOGeometryTransparent->pVS = pVS;
		pPSOGeometryTransparent->pPS = pPS;
		pPSOGeometryTransparent->pRasterState = pRasterState;
		pPSOGeometryTransparent->pDepthStencilState = pDepthStencilState;
		pPSOGeometryTransparent->pBlendState = pBlendState;
	}

	//Create PSO resolve transparent and opaque
	{
		std::vector<std::pair<std::string, std::string>> defines;
		defines.push_back({ "FRAGMENT_COUNT",     std::to_string(FRAGMENT_COUNT) });
		defines.push_back({ "MSAA_SAMPLE_COUNT",  std::to_string(MSAA_SAMPLES)   });

		Microsoft::WRL::ComPtr<ID3D11ComputeShader> pCS;
		auto const pBlobCS = DX::CompileShader(L"Shaders/ResolveGeometry.hlsl", "CSMain", "cs_5_0", defines);
		DX::ThrowIfFailed(pDevice->CreateComputeShader(pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, pCS.ReleaseAndGetAddressOf()));
		pPSOGeometryResolve->pCS = pCS;
	}


	auto isRun = true;
	while (isRun) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {	 
				case SDL_WINDOWEVENT:
					switch (event.window.event) {
						case SDL_WINDOWEVENT_RESIZED:
							ResizeRenderTargets(event.window.data1, event.window.data2);
							break;
						default:
							break;
					}			
					break;
				case SDL_QUIT:
					isRun = false;
					break;
				default:
					break;
			}
		}

		int32_t width = 0;
		int32_t height = 0;
		SDL_GetWindowSize(pWindow.get(), &width, &height);


		auto const clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		auto const viewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		auto const scissor  = CD3D11_RECT(0, 0, width, height);
		auto const threadGroupsX = static_cast<uint32_t>(std::ceil(width / 8.0f));
		auto const threadGroupsY = static_cast<uint32_t>(std::ceil(height / 8.0f));


		pDeviceContext->ClearRenderTargetView(pRTV_MSAA.Get(), std::data(clearColor));
		pDeviceContext->ClearDepthStencilView(pDSV_MSAA.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
		pDeviceContext->ClearUnorderedAccessViewUint(pUAVTextureHeadOIT.Get(), std::data({ 0xFFFFFFFF }));

		pDeviceContext->RSSetViewports(1, &viewport);
		pDeviceContext->RSSetScissorRects(1, &scissor);

		{
			ID3D11RenderTargetView* ppRTVClear[] = { nullptr };
			ID3D11DepthStencilView* pDSVClear = nullptr;

			pPSOGeometryOpaque->Apply(pDeviceContext);
			pDeviceContext->OMSetRenderTargets(1, pRTV_MSAA.GetAddressOf(), pDSV_MSAA.Get());
			pDeviceContext->DrawInstanced(3, 5, 0, 0);
			pDeviceContext->OMSetRenderTargets(_countof(ppRTVClear), ppRTVClear, pDSVClear);
		}
	
		{
			
			ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr, nullptr };
			ID3D11DepthStencilView*    pDSVClear    = nullptr;

			pPSOGeometryTransparent->Apply(pDeviceContext);
			pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, pDSV_MSAA.Get(), 0, 2, std::data({ pUAVTextureHeadOIT.Get(),pUAVBufferLinkedListOIT.Get() }), std::data({ 0x0u, 0x0u }));
			pDeviceContext->DrawInstanced(3, 5, 0, 0);
			pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, pDSVClear, 0, _countof(ppUAVClear), ppUAVClear, nullptr);
		}

		{
			ID3D11UnorderedAccessView* ppUAVClear[]  = { nullptr };
			ID3D11ShaderResourceView*  ppSRVClear[] = { nullptr, nullptr };
		
			pMSAAResolver->Apply(pDeviceContext, pRTV_MSAA, pRTVSwapChain, colorBufferFormat);		
			pPSOGeometryResolve->Apply(pDeviceContext);
			pDeviceContext->CSSetShaderResources(0, 2, std::data({ pSRVTextureHeadOIT.Get(), pSRVBufferLinkedListOIT.Get() }));
			pDeviceContext->CSSetUnorderedAccessViews(0, 1, pUAVSwapChain.GetAddressOf(), nullptr);
			pDeviceContext->Dispatch(threadGroupsX, threadGroupsY, 1);
			pDeviceContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
			pDeviceContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
		
		}

		DX::ThrowIfFailed(pSwapChain->Present(0, 0));
	}

	SDL_Quit();
}
