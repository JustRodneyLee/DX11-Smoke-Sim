//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include "GameLog.hpp"
#include "VoxelGrid.hpp"
#include "imgui-1.91.5/imgui.h"
#include "imgui-1.91.5/backends/imgui_impl_win32.h"
#include "imgui-1.91.5/backends/imgui_impl_dx11.h"

#include <string>
#include <deque>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <filesystem>

#include "d3dcompiler.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// Variables
const int winWidth = 1000;
const int winHeight = 800;

std::deque<float> frameTimes;
int fps;
int maxFrames = 144;
float alpha = 0.1f; // Smoothing factor (0.0f to 1.0f)

const int simWinWidth = 200;
const int simWinHeight = 250;
const int margin = 20;

atomic<bool> loadingFile = false;
atomic<float> loadProgress = 0.0f;
atomic<int> simLoadedFrames = 0;
string simPath(255, '\0');
int simFrame;
int simTotalFrames;
int simX, simY, simZ;
vector<VoxelGrid<float>> simFrameData;
bool simLoaded;
bool simPlaying;

Game::Game() noexcept :
    m_window(nullptr),
    m_outputWidth(winWidth),
    m_outputHeight(winHeight),
    m_featureLevel(D3D_FEATURE_LEVEL_9_1)
{
}

ID3D11VertexShader* vertexShader = nullptr;
ID3D11PixelShader* pixelShader = nullptr;

ID3D11Buffer* indexBuffer;
ID3D11Buffer* vertexBuffer;
ID3D11InputLayout* inputLayout;

ID3D11Texture3D* tex3D = nullptr;
ID3D11ShaderResourceView* srv = nullptr;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
};

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_window = window;
    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateDevice();

    CreateResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:    
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "PS", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &psBlob, nullptr);
    m_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

    D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "VS", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vsBlob, nullptr);
    m_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);

    Vertex vertices[] =
    {
        {XMFLOAT3(-1.0f, -1.0f, 0.5f), XMFLOAT2(0.0f, 1.0f)}, // Bottom left
        {XMFLOAT3(1.0f, -1.0f, 0.5f), XMFLOAT2(1.0f, 1.0f)}, // Bottom right
        {XMFLOAT3(-1.0f, 1.0f, 0.5f), XMFLOAT2(0.0f, 0.0f)}, // Top left
        {XMFLOAT3(1.0f, 1.0f, 0.5f), XMFLOAT2(1.0f, 0.0f)}, // Top right
    };

    uint16_t indices[] = {
        // Clockwise order
        0, 2, 1, // First triangle
        2, 3, 1  // Second triangle
    };

    // Vertex Buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    m_d3dDevice->CreateBuffer(&bd, &InitData, &vertexBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_d3dContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    //Index buffer
    bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    InitData = {};
    InitData.pSysMem = indices;

    m_d3dDevice->CreateBuffer(&bd, &InitData, &indexBuffer);
    m_d3dContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Input Layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    m_d3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    m_d3dContext->IASetInputLayout(inputLayout);
    
    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();

    D3D11_DEPTH_STENCIL_DESC sd = {};
    sd.DepthEnable = FALSE;
    sd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    sd.DepthFunc = D3D11_COMPARISON_ALWAYS;

    sd.StencilEnable = FALSE;

    ID3D11DepthStencilState* noDepthStencilState = nullptr;
    m_d3dDevice->CreateDepthStencilState(&sd, &noDepthStencilState);

    m_d3dContext->OMSetDepthStencilState(noDepthStencilState, 0);

    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);    

    simPath = "Simulations\\staticframe\\info.sim";
}

// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());


    // TODO: Add your game logic here.
    if (simPlaying) {
        simFrame++;
        if (simFrame >= simTotalFrames) {
            simPlaying = false;
            simFrame = 0;
        }
    }
    
    // Store the elapsed time for this frame
    frameTimes.push_back(elapsedTime);

    // Remove the oldest frame if we've exceeded maxFrames
    if (frameTimes.size() > maxFrames)
    {
        frameTimes.pop_front();
    }

    // Calculate the average frame time
    float averageFrameTime = 0.0f;
    for (float time : frameTimes)
    {
        averageFrameTime += time;
    }
    averageFrameTime /= frameTimes.size();

    // Calculate FPS from the average frame time
    fps = static_cast<int>(1.0f / averageFrameTime);
}

void Game::LoadSimulation(const std::string& path) {
    simLoaded = false;
    loadProgress = 0.0f;
    loadingFile = true;
    simTotalFrames = 0;
    simFrameData.clear();

    std::filesystem::path dirPath = std::filesystem::path(path).parent_path();

    std::ifstream infoFile(path);
    if (!infoFile.is_open()) {
        loadingFile = false;
        return; // File is not valid
    }

    infoFile >> simTotalFrames >> simX >> simY >> simZ;
    infoFile.close();

    if (simTotalFrames <= 0) {
        loadingFile = false;
        return; // No data to load
    }

    for (int i = 0; i < simTotalFrames; i++) {
        VoxelGrid<float> gridData(simX, simY, simZ);
        std::filesystem::path framePath = dirPath / ("frame" + std::to_string(i));

        std::ifstream frameFile(framePath);
        if (frameFile.is_open()) {
            for (int k = 0; k < simZ; ++k)
                for (int j = 0; j < simY; ++j)
                    for (int i = 0; i < simX; ++i) {
                        double density;
                        float fdensity = 0.0f;
                        frameFile >> density;
                        if (density > 0)
                            fdensity = static_cast<float>(density);
                        gridData.At(i, j, k) = fdensity;
                    }
            simFrameData.push_back(gridData);
        }
        frameFile.close();

        simLoadedFrames = i;  // Update the number of loaded frames
        loadProgress = static_cast<float>(simLoadedFrames) / simTotalFrames;  // Update progress
    }

    loadingFile = false;
    // Configure Texture 3D
    D3D11_TEXTURE3D_DESC td = {};
    td.Width = simX;
    td.Height = simY;
    td.Depth = simZ;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    td.MiscFlags = 0;

    m_d3dDevice->CreateTexture3D(&td, nullptr, &tex3D);

    CD3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvd.Texture3D.MostDetailedMip = 0;
    srvd.Texture3D.MipLevels = 1;

    m_d3dDevice->CreateShaderResourceView(tex3D, &srvd, &srv);

    m_d3dContext->PSSetShaderResources(0, 1, &srv);
}

// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    // Draw Smoke    
    if (simLoaded) {
        D3D11_MAPPED_SUBRESOURCE res = {};
        DX::ThrowIfFailed(m_d3dContext->Map(tex3D, 0, D3D11_MAP_WRITE_DISCARD, 0, &res));
        // Copy density data into the mapped resource
        float* dest = reinterpret_cast<float*>(res.pData);
        float* src = simFrameData[simFrame].Data();
        size_t slicePitch = simX * simY;

        for (int z = 0; z < simZ; ++z) {
            // Copy each slice
            memcpy(dest + z * slicePitch, src + z * slicePitch, slicePitch * sizeof(float));
        }

        // Unmap the texture to update it on the GPU
        m_d3dContext->Unmap(tex3D, 0);

        m_d3dContext->VSSetShader(vertexShader, nullptr, 0);
        m_d3dContext->PSSetShader(pixelShader, nullptr, 0);
        m_d3dContext->DrawIndexed(6, 0, 0);
    }

    // ImGUI UI Render

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags winFlags = 0;
    winFlags |= ImGuiWindowFlags_NoMove;
    winFlags |= ImGuiWindowFlags_NoBackground;
    winFlags |= ImGuiWindowFlags_NoResize;
    winFlags |= ImGuiWindowFlags_NoTitleBar;
    winFlags |= ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Frame Counter", 0, winFlags)) {
        string fpsText = "FPS " + std::to_string(fps);
        ImGui::Text(fpsText.c_str());

        for (auto& s : gameLog) {
            ImGui::Text(s.c_str());
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(winWidth - simWinWidth - margin, margin), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(simWinWidth, simWinHeight), ImGuiCond_Once);

    winFlags = 0;
    winFlags |= ImGuiWindowFlags_NoMove;
    winFlags |= ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("Simulator", 0, winFlags)) {
        ImGui::Text("File Path:");
        ImGui::InputText("##FilePath", simPath.data(), simPath.capacity(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AllowTabInput);

        ImGui::BeginDisabled(loadingFile);
        if (ImGui::Button("Load Simulation")) {
            if (!loadingFile) {
                thread(&Game::LoadSimulation, this, simPath).detach();
            }
        }
        ImGui::EndDisabled();

        ImGui::PushTextWrapPos(simWinWidth - margin);
        if (loadingFile) {
            int loadedFrames = simLoadedFrames.load();
            ImGui::Text("Loading frames...");
            ImGui::ProgressBar(loadProgress, ImVec2(-1, 0));
            ImGui::Text("Frames Loaded: %d / %d", loadedFrames, simTotalFrames);
        }
        else if (!simFrameData.empty()) {
            simLoaded = true;
            ImGui::Text("Frame loading complete!");
            ImGui::Text("Total Frames Loaded: %d", simTotalFrames);
        }
        else if (simTotalFrames == 0) {
            ImGui::Text("No frames to load or invalid file.");
        }
        ImGui::PopTextWrapPos();

        if (simLoaded) {
            if (!simPlaying) {
                if (ImGui::Button("Play Simulation")) {
                    simPlaying = true;
                }
            }
            else {
                if (ImGui::Button("Stop Simulation")) {
                    simPlaying = false;
                }                
            }
            if (ImGui::Button("Reset Simulation")) {
                simPlaying = false;
                simFrame = 0;
            }
            ImGui::Text("Frame %d/%d", simFrame, simTotalFrames);
        }
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    // Clear the views.
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), Colors::CornflowerBlue);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3dContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    // Set the viewport.
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight), 0.f, 1.f };
    m_d3dContext->RSSetViewports(1, &viewport);
}

// Presents the back buffer contents to the screen.
void Game::Present()
{
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = m_swapChain->Present(1, 0);

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OnDeviceLost();
    }
    else
    {
        DX::ThrowIfFailed(hr);
    }
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_window)
        return;

    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = winWidth;
    height = winHeight;
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
    UINT creationFlags = 0;

#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    static const D3D_FEATURE_LEVEL featureLevels [] =
    {
        // TODO: Modify for supported Direct3D feature levels
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create the DX11 API device object, and get a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DX::ThrowIfFailed(D3D11CreateDevice(
        nullptr,                            // specify nullptr to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        device.ReleaseAndGetAddressOf(),    // returns the Direct3D device created
        &m_featureLevel,                    // returns feature level of device created
        context.ReleaseAndGetAddressOf()    // returns the device immediate context
        ));

#ifndef NDEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(device.As(&d3dDebug)))
    {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D11_MESSAGE_ID hide [] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed.
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));

    // TODO: Initialize device dependent objects here (independent of window size).
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateResources()
{
    // Clear the previous window size specific context.
    m_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_d3dContext->Flush();

    const UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    const UINT backBufferHeight = static_cast<UINT>(m_outputHeight);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    const DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    constexpr UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a SwapChain from a Win32 window.
        DX::ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            m_swapChain.ReleaseAndGetAddressOf()
            ));

        // This template does not support exclusive fullscreen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
        DX::ThrowIfFailed(dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    ComPtr<ID3D11Texture2D> backBuffer;
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Allocate a 2-D surface as the depth/stencil buffer and
    // create a DepthStencil view on this surface to use on bind.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1, D3D11_BIND_DEPTH_STENCIL);

    ComPtr<ID3D11Texture2D> depthStencil;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, depthStencil.GetAddressOf()));

    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(depthStencil.Get(), nullptr, m_depthStencilView.ReleaseAndGetAddressOf()));

    // TODO: Initialize windows-size dependent objects here.
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.

    m_depthStencilView.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();

    CreateDevice();

    CreateResources();
}
