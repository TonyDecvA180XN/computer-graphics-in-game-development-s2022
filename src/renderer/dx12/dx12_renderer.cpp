#include "dx12_renderer.h"

#include "utils/com_error_handler.h"
#include "utils/window.h"

#include <DirectXColors.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>

void cg::renderer::dx12_renderer::init()
{
	UINT debugFlags = 0;
#if defined(_DEBUG)
	// Enable the D3D12 debug layer.
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
	debugFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	ComPtr<IDXGIFactory4> mdxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(debugFlags, IID_PPV_ARGS(&mdxgiFactory)));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr, // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device));

	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)));
	}

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
									  IID_PPV_ARGS(&fence)));

	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	UINT mDsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	UINT mCbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#ifdef _DEBUG
	UINT adapterIdx = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(adapterIdx, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++adapterIdx;
	}

	for (adapterIdx = 0; adapterIdx < adapterList.size(); ++adapterIdx)
	{
		UINT adapterOutputIdx = 0;
		IDXGIOutput* output = nullptr;
		while (adapterList[adapterIdx]->EnumOutputs(adapterOutputIdx, &output) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);

			std::wstring text = L"***Output: ";
			text += desc.DeviceName;
			text += L"\n";
			OutputDebugString(text.c_str());

			UINT count = 0;
			UINT flags = 0;

			output->Release();
			output = nullptr;

			++adapterOutputIdx;
		}
		adapterList[adapterIdx]->Release();
		adapterList[adapterIdx] = nullptr;
	}
#endif

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&command_queue)));

	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(command_allocator.GetAddressOf())));
	// TODO: command allocator

	ThrowIfFailed(device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		command_allocator.Get(), // Associated command allocator
		nullptr, // Initial PipelineStateObject
		IID_PPV_ARGS(command_list.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	ThrowIfFailed(command_list->Close());

	// Release the previous swapchain we will be recreating.
	swap_chain.Reset();

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferDesc.Width = settings->width;
	swapChainDesc.BufferDesc.Height = settings->height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = frame_number;
	swapChainDesc.OutputWindow = utils::window::hwnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		command_queue.Get(),
		&swapChainDesc,
		swap_chain.GetAddressOf()));

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = frame_number;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(rtv_heap.GetAddressOf()));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(dsv_heap.GetAddressOf()));

	// Flush before changing any resources.
	wait_for_gpu();

	command_list->Reset(command_allocator.Get(), nullptr);

	// Release the previous resources we will be recreating.
	for (int i = 0; i < frame_number; ++i)
	{
		render_targets[i].Reset();
	}
	ds_buffer.Reset();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < frame_number; i++)
	{
		swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, rtv_descriptor_size);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = settings->width;
	depthStencilDesc.Height = settings->height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(ds_buffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(ds_buffer.Get(), &dsvDesc, dsv_heap->GetCPUDescriptorHandleForHeapStart());

	// Transition the resource from its initial state to be used as a depth buffer.
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ds_buffer.Get(),
																		   D3D12_RESOURCE_STATE_COMMON,
																		   D3D12_RESOURCE_STATE_DEPTH_WRITE));

	ThrowIfFailed(command_list->Close());
	ID3D12CommandList* commandLists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, commandLists);

	// Wait until initialization is complete.
	wait_for_gpu();

	// Update the viewport transform to cover the client area.
	view_port.TopLeftX = 0;
	view_port.TopLeftY = 0;
	view_port.Width = static_cast<float>(settings->width);
	view_port.Height = static_cast<float>(settings->height);
	view_port.MinDepth = 0.0f;
	view_port.MaxDepth = 1.0f;

	scissor_rect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(settings->width), static_cast<LONG>(settings->height) };

	ThrowIfFailed(command_list->Reset(command_allocator.Get(), nullptr));

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbvHeapDesc,
											   IID_PPV_ARGS(&cbv_heap)));

	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data 
	// at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	size_t elementByteSize = sizeof(DirectX::XMFLOAT4X4);
	elementByteSize = (elementByteSize + 255) & ~255;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constant_buffer)));

	ThrowIfFailed(constant_buffer->Map(0, nullptr, reinterpret_cast<void**>(&constant_buffer_data_begin)));

	// We do not need to unmap until we are done with the resource.  However, we must not write to
	// the resource while it is in use by the GPU (so we must use synchronization techniques).

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = constant_buffer->GetGPUVirtualAddress();
	// Offset to the ith object constant buffer in the buffer.
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * elementByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = static_cast<UINT>(elementByteSize);

	device->CreateConstantBufferView(
		&cbvDesc,
		cbv_heap->GetCPUDescriptorHandleForHeapStart());

	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
											D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
											  serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

	if (errorBlob != nullptr)
	{
		OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	}

	ThrowIfFailed(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&root_signature)));

	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> vsByteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	ThrowIfFailed(D3DCompileFromFile(L"shaders\\shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
									 "VSMain", "vs_5_0", compileFlags, 0, &vsByteCode, &errors));

	if (errors != nullptr)
	{
		OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
	}

	ComPtr<ID3DBlob> psByteCode = nullptr;
	ThrowIfFailed(D3DCompileFromFile(L"shaders\\shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
									 "PSMain", "ps_5_0", compileFlags, 0, &psByteCode, &errors));

	if (errors != nullptr)
	{
		OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},

	};

	load_assets();

	std::vector<d3d_vertex> vertices;
	std::vector<UINT> indices;
	UINT index_offset = 0;
	for (size_t shape_idx = 0; shape_idx != model->get_index_buffers().size(); ++shape_idx)
	{
		for (size_t i = 0; i != model->get_vertex_buffers()[shape_idx]->get_number_of_elements(); ++i)
		{
			vertex& v = model->get_vertex_buffers()[shape_idx]->item(i);
			d3d_vertex vert = {
				DirectX::XMFLOAT4(v.position.x, v.position.y, v.position.z, 1.0f),
				DirectX::XMFLOAT4(v.normal.x, v.normal.y, v.normal.z, 0.0f),
				DirectX::XMFLOAT4(v.ambient.x, v.ambient.y, v.ambient.z, 1.0f),
				DirectX::XMFLOAT4(v.diffuse.x, v.diffuse.y, v.diffuse.z, 1.0f),
				DirectX::XMFLOAT4(v.emissive.x, v.emissive.y, v.emissive.z, 1.0f)
			};
			vertices.emplace_back(vert);
		}
		for (size_t i = 0; i != model->get_index_buffers()[shape_idx]->get_number_of_elements(); ++i)
		{
			UINT& ind = model->get_index_buffers()[shape_idx]->item(i);
			indices.push_back(ind + index_offset);
		}
		index_offset += static_cast<UINT>(model->get_vertex_buffers()[shape_idx]->get_number_of_elements());
	}

	const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(d3d_vertex);
	const UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(UINT);

	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &VertexBufferCPU));
	CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &IndexBufferCPU));
	CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
	//command_list.Get(), vertices.data(), vbByteSize, VertexBufferUploader);

	//IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
	//command_list.Get(), indices.data(), ibByteSize, IndexBufferUploader);

	{
		ComPtr<ID3D12Resource> defaultBuffer;

		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vbByteSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vbByteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(VertexBufferUploader.GetAddressOf())));

		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = vertices.data();
		subResourceData.RowPitch = vbByteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																			   D3D12_RESOURCE_STATE_COMMON,
																			   D3D12_RESOURCE_STATE_COPY_DEST));
		UpdateSubresources<1>(command_list.Get(), defaultBuffer.Get(), VertexBufferUploader.Get(), 0, 0, 1,
							  &subResourceData);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																			   D3D12_RESOURCE_STATE_COPY_DEST,
																			   D3D12_RESOURCE_STATE_GENERIC_READ));

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.

		VertexBufferGPU = defaultBuffer;
	}
	{
		ComPtr<ID3D12Resource> defaultBuffer;

		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(ibByteSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(ibByteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(IndexBufferUploader.GetAddressOf())));

		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = indices.data();
		subResourceData.RowPitch = ibByteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																			   D3D12_RESOURCE_STATE_COMMON,
																			   D3D12_RESOURCE_STATE_COPY_DEST));
		UpdateSubresources<1>(command_list.Get(), defaultBuffer.Get(), IndexBufferUploader.Get(), 0, 0, 1,
							  &subResourceData);
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																			   D3D12_RESOURCE_STATE_COPY_DEST,
																			   D3D12_RESOURCE_STATE_GENERIC_READ));

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.

		IndexBufferGPU = defaultBuffer;
	}

	VertexByteStride = sizeof(d3d_vertex);
	VertexBufferByteSize = vbByteSize;
	IndexFormat = DXGI_FORMAT_R32_UINT;
	IndexBufferByteSize = ibByteSize;

	//SubmeshGeometry submesh;
	IndexCount = (UINT)indices.size();
	//submesh.StartIndexLocation = 0;
	//submesh.BaseVertexLocation = 0;

	//mBoxGeo->DrawArgs["box"] = submesh;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = root_signature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(vsByteCode->GetBufferPointer()),
		vsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(psByteCode->GetBufferPointer()),
		psByteCode->GetBufferSize()
	};

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // We load OpenGL model, so its indices are in RHS, but I am lazy
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline_state)));

	ThrowIfFailed(command_list->Close());

	command_queue->ExecuteCommandLists(1, commandLists);

	// Wait until initialization is complete.
	wait_for_gpu();

	currentRenderTargetIdx = 0;
}

void cg::renderer::dx12_renderer::destroy()
{
	wait_for_gpu();
}

void cg::renderer::dx12_renderer::update()
{
	using namespace DirectX;
	const XMMATRIX world = model->get_world_matrix();
	const XMMATRIX view = camera->get_view_matrix();
	const XMMATRIX projection = camera->get_projection_matrix();

	const XMMATRIX wvp = XMMatrixMultiply(XMMatrixMultiply(world, view), projection);
	XMStoreFloat4x4(&world_view_projection, XMMatrixTranspose(wvp));

	CopyMemory(constant_buffer_data_begin, &world_view_projection, sizeof(XMFLOAT4X4));
}

void cg::renderer::dx12_renderer::render()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	command_allocator->Reset();

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	command_list->Reset(command_allocator.Get(), pipeline_state.Get());

	command_list->RSSetViewports(1, &view_port);
	command_list->RSSetScissorRects(1, &scissor_rect);

	// Indicate a state transition on the resource usage.
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
									  render_targets[currentRenderTargetIdx].Get(),
									  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	command_list->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
											rtv_heap->GetCPUDescriptorHandleForHeapStart(),
											currentRenderTargetIdx,
											rtv_descriptor_size), DirectX::Colors::Aqua, 0, nullptr);
	command_list->ClearDepthStencilView(dsv_heap->GetCPUDescriptorHandleForHeapStart(),
										D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	command_list->OMSetRenderTargets(1, &CD3DX12_CPU_DESCRIPTOR_HANDLE(
										 rtv_heap->GetCPUDescriptorHandleForHeapStart(),
										 currentRenderTargetIdx,
										 rtv_descriptor_size), true, &dsv_heap->GetCPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* descriptorHeaps[] = { cbv_heap.Get() };
	command_list->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	command_list->SetGraphicsRootSignature(root_signature.Get());

	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = VertexByteStride;
	vbv.SizeInBytes = VertexBufferByteSize;

	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = IndexFormat;
	ibv.SizeInBytes = IndexBufferByteSize;

	command_list->IASetVertexBuffers(0, 1, &vbv);
	command_list->IASetIndexBuffer(&ibv);
	command_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	command_list->SetGraphicsRootDescriptorTable(0, cbv_heap->GetGPUDescriptorHandleForHeapStart());

	command_list->DrawIndexedInstanced(
		IndexCount,
		1, 0, 0, 0);

	// Indicate a state transition on the resource usage.
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
									  render_targets[currentRenderTargetIdx].Get(),
									  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(command_list->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(swap_chain->Present(1, 0));
	//mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	wait_for_gpu();

	// Switch to the next render target
	currentRenderTargetIdx = ++currentRenderTargetIdx % frame_number;
}

void cg::renderer::dx12_renderer::load_pipeline()
{
	THROW_ERROR("Not implemented yet")
}

void cg::renderer::dx12_renderer::load_assets()
{
	// Make camera
	camera = std::make_shared<world::camera>();
	camera->set_position(DirectX::XMLoadFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(settings->camera_position.data())));
	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_theta(settings->camera_theta);
	camera->set_phi(settings->camera_phi);
	camera->set_z_near(settings->camera_z_near);
	camera->set_z_far(settings->camera_z_far);

	// Load model from file
	model = std::make_shared<world::model>();
	model->load_obj(settings->model_path);
}

void cg::renderer::dx12_renderer::populate_command_list()
{
	THROW_ERROR("Not implemented yet")
}

void cg::renderer::dx12_renderer::move_to_next_frame()
{
	THROW_ERROR("Not implemented yet")
}

void cg::renderer::dx12_renderer::wait_for_gpu()
{
	// Advance the fence value to mark commands up to this fence point.
	++frame_index;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(command_queue->Signal(fence.Get(), frame_index));

	// Wait until the GPU has completed commands up to this fence point.
	if (fence->GetCompletedValue() < frame_index)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(fence->SetEventOnCompletion(frame_index, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
