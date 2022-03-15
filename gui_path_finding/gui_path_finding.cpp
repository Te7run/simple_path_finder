#include <iostream>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <queue>
#include <random>

#pragma comment(lib, "d3d11.lib")

#define width 15
#define height 15

// algorithm ----------------

// 1 is no obstacle 0 is obstacle
int grid[width][height];
bool visited[width][height];

int prev_x[width][height];
int prev_y[width][height];

int distance = 0;

// for a_star
int g_score[width][height];
int f_score[width][height];
int h_score[width][height];

float get_random(float a, float b);
void reset();
void clear_path(const std::pair<int, int> start_node, const std::pair<int, int> end_node);
void prepare_grid(const std::pair<int, int> start_node, const std::pair<int, int> end_node);
void backtrack_path(const std::pair<int, int> start_node, const std::pair<int, int> node);
bool bfs(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node);
int h(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node);
bool a_star(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node);

// imgui ---------------------

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{

    // App window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Path finding example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Path finding example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 900, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    static auto start = std::make_pair(0, 0);
    static auto end = std::make_pair(width - 1, height - 1);

    reset();
    prepare_grid(start, end);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Rendering
        ImGuiIO& io = ImGui::GetIO();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.0f, 0.0f, 0.0f, 0.0f });
        ImGui::Begin("##Backbuffer", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);

        ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);


        if (ImGui::IsMouseClicked(0)) {
            auto mouse_position = ImGui::GetMousePos();

            int tile_x = ((int)mouse_position.x / 50);
            int tile_y = ((int)mouse_position.y / 50);
            if (tile_x >= 0 && tile_x < width && tile_y >= 0 && tile_y < height) {
                if (grid[tile_x][tile_y] == 1)
                    grid[tile_x][tile_y] = 0;
                else if(grid[tile_x][tile_y] == 0)
                    grid[tile_x][tile_y] = 1;
            }

        }

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {

                ImColor col;

                if (grid[i][j] == 1)
                    col = ImColor(0, 0, 0, 255);
                else if(grid[i][j] == 0)
                    col = ImColor(255, 0, 0, 255);
                else if(grid[i][j] == 2)
                    col = ImColor(0, 0, 255, 255);
                else
                    col = ImColor(255, 255, 255, 255);


                ImGui::GetWindowDrawList()->AddRectFilled({ (float)(i * 50), (float)(j * 50) }, { (float)(i * 50 + 50), (float)(j * 50 + 50) }, col);
            }
        }



        ImGui::GetWindowDrawList()->PushClipRectFullScreen();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        {
            ImGui::Begin("Path finder"); 


            const char* items[] = { "BFS", "A*" };
            static int current_item = NULL;
            ImGui::Combo("algorithm", &current_item, items, IM_ARRAYSIZE(items));

            if (ImGui::Button("randomize")) {
                reset();
                prepare_grid(start, end);

            }

            if (ImGui::Button("clear path")) {
                reset();
                clear_path(start, end);
            }

            if (ImGui::Button("find path")) {
                reset();
                clear_path(start, end);

                if (current_item == 0) {
                    bfs(start, end);
                }
                else if (current_item == 1)
                {
                    a_star(start, end);
                }
            }


            ImGui::SameLine();
            ImGui::Text("distance = %d", distance);
            ImGui::End();
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// functions
float get_random(float a, float b) {
    std::random_device rd;
    std::mt19937 e2(rd());
    std::uniform_real_distribution<> dist(a, b);

    return dist(e2);
}

void reset() {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            visited[i][j] = false;
            prev_x[i][j] = 0;
            prev_y[i][j] = 0;
            g_score[i][j] = 0;
            f_score[i][j] = 0;
            h_score[i][j] = 0;
            distance = 0;
        }
    }
}

void clear_path(const std::pair<int, int> start_node, const std::pair<int, int> end_node) {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            if (grid[i][j] == 2)
                grid[i][j] = 1;
        }
    }

    grid[start_node.first][start_node.second] = 3;
    grid[end_node.first][end_node.second] = 3;
}

void prepare_grid(const std::pair<int, int> start_node, const std::pair<int, int> end_node) {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {

            auto rand = get_random(0.f, 2.f);

            if (rand < 1.5f)
                grid[i][j] = 1;
            else
                grid[i][j] = 0;
        }
    }

    grid[start_node.first][start_node.second] = 3;
    grid[end_node.first][end_node.second] = 3;
}

void backtrack_path(const std::pair<int, int> start_node, const std::pair<int, int> node) {
    if (node.first == start_node.first && node.second == start_node.second) {
        printf("start node: (%d, %d), distance: %d\n", node.first, node.second, distance);
        // mark as start
        grid[node.first][node.second] = 3;
        return;
    }

    printf("(%d, %d), ", node.first, node.second);

    auto prev_pair = std::make_pair(prev_x[node.first][node.second], prev_y[node.first][node.second]);
    grid[prev_pair.first][prev_pair.second] = 2;
    distance++;
    backtrack_path(start_node, prev_pair);
}

bool bfs(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node) {
    std::queue<std::pair<int, int>> q;
    visited[start_node.first][start_node.second] = true;
    q.push(start_node);

    prev_x[start_node.first][start_node.second] = -1;
    prev_y[start_node.first][start_node.second] = -1;

    while (!q.empty()) {
        auto v = q.front();
        q.pop();



        if (v == end_node) {
            printf("destination reached\n");
            backtrack_path(start_node, v);
            return true;
        }

        auto visit_node = [&](std::pair<int, int> w) {
            if (!visited[w.first][w.second]) {
                visited[w.first][w.second] = true;
                q.push(w);

                prev_x[w.first][w.second] = v.first;
                prev_y[w.first][w.second] = v.second;
            }
        };

        // 1. edge
        if (v.first - 1 >= 0 && grid[v.first - 1][v.second]) {
            auto w = std::make_pair(v.first - 1, v.second);

            visit_node(w);
        }
        // 2.
        if (v.second - 1 >= 0 && grid[v.first][v.second - 1]) {
            auto w = std::make_pair(v.first, v.second - 1);
            visit_node(w);
        }
        // 3.
        if (v.first + 1 < width && grid[v.first + 1][v.second]) {
            auto w = std::make_pair(v.first + 1, v.second);
            visit_node(w);
        }
        // 4.
        if (v.second + 1 < width && grid[v.first][v.second + 1]) {
            auto w = std::make_pair(v.first, v.second + 1);
            visit_node(w);
        }
    }

    printf("destination in unavaiable\n");
    return false;
}

int h(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node) {
    return (int)std::sqrt(std::pow(start_node.first - end_node.first, 2) + std::pow(start_node.second - end_node.second, 2));
}

bool a_star(const std::pair<int, int>& start_node, const std::pair<int, int>& end_node) {

    auto compare = [](const std::pair<int, int>& lhs, const std::pair<int, int>& rhs) {
        return f_score[lhs.first][lhs.second] > f_score[rhs.first][rhs.second];
    };

    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, decltype(compare)> os(compare);


    os.push(start_node);
    prev_x[start_node.first][start_node.second] = 0;
    prev_y[start_node.first][start_node.second] = 0;

    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {

            g_score[i][j] = INT_MAX;
            f_score[i][j] = INT_MAX;
        }
    }
    g_score[start_node.first][start_node.second] = 0;
    f_score[start_node.first][start_node.second] = h(start_node, end_node);

    while (!os.empty()) {
        auto v = os.top();
        visited[v.first][v.second] = false;
        os.pop();

        if (v == end_node) {
            printf("destination reached\n");
            backtrack_path(start_node, v);
            return true;
        }


        auto visit_node = [&](std::pair<int, int> w) {
            auto tentative_g_score = g_score[v.first][v.second] + 1;
            if (tentative_g_score < g_score[w.first][w.second]) {
                prev_x[w.first][w.second] = v.first;
                prev_y[w.first][w.second] = v.second;
                g_score[w.first][w.second] = tentative_g_score;
                f_score[w.first][w.second] = tentative_g_score + h(w, end_node);

                if (!visited[w.first][w.second]) {
                    visited[w.first][w.second] = true;
                    os.push(w);
                }

            }
        };

        // 1. edge
        if (v.first - 1 >= 0 && grid[v.first - 1][v.second]) {
            auto w = std::make_pair(v.first - 1, v.second);
            visit_node(w);
        }
        // 2.
        if (v.second - 1 >= 0 && grid[v.first][v.second - 1]) {
            auto w = std::make_pair(v.first, v.second - 1);
            visit_node(w);
        }
        // 3.
        if (v.first + 1 < width && grid[v.first + 1][v.second]) {
            auto w = std::make_pair(v.first + 1, v.second);
            visit_node(w);
        }
        // 4.
        if (v.second + 1 < width && grid[v.first][v.second + 1]) {
            auto w = std::make_pair(v.first, v.second + 1);
            visit_node(w);
        }

    }

    printf("destination in unavaiable\n");
    return false;
}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
