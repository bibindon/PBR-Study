#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <cassert>
#include <vector>
#include <string>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

static const int WINDOW_SIZE_W = 1600;
static const int WINDOW_SIZE_H = 900;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// D3D
LPDIRECT3D9         g_pD3D = NULL;
LPDIRECT3DDEVICE9   g_pd3dDevice = NULL;
LPD3DXEFFECT        g_pEffect = NULL;
LPD3DXFONT          g_pFont = NULL;

// Mesh
LPD3DXMESH          g_pMesh = NULL;
DWORD               g_dwNumMaterials = 0;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;

// Env Cube
LPDIRECT3DCUBETEXTURE9  g_pEnvCube = NULL;

// App
HWND  g_hWnd = NULL;
bool  g_bClose = false;

static void Cleanup()
{
    for (size_t i = 0; i < g_pTextures.size(); ++i)
    {
        SAFE_RELEASE(g_pTextures[i]);
    }
    g_pTextures.clear();

    SAFE_RELEASE(g_pEnvCube);
    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

static bool InitD3D(HWND hWnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_pD3D == NULL)
    {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));

    d3dpp.BackBufferWidth = WINDOW_SIZE_W;
    d3dpp.BackBufferHeight = WINDOW_SIZE_H;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.Windowed = TRUE;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL,
                                      hWnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp,
                                      &g_pd3dDevice);

    if (FAILED(hr))
    {
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                  D3DDEVTYPE_HAL,
                                  hWnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                  &d3dpp,
                                  &g_pd3dDevice);
        if (FAILED(hr))
        {
            return false;
        }
    }

    return true;
}

static bool LoadMeshAndEffect()
{
    // Mesh
    LPD3DXBUFFER pAdj = NULL;
    LPD3DXBUFFER pMtl = NULL;
    DWORD        numMaterials = 0;

    HRESULT hr = D3DXLoadMeshFromX(
                                   //_T("cube.x"),
                                   _T("sphere.x"),
                                   D3DXMESH_MANAGED,
                                   g_pd3dDevice,
                                   &pAdj,
                                   &pMtl,
                                   NULL,
                                   &numMaterials,
                                   &g_pMesh);

    assert(hr == S_OK);

    g_dwNumMaterials = numMaterials;

    // 隣接情報を計算して法線を再計算
    // なめらかになる
    // なめらかにしたくないときは計算してはいけない
    if (true)
    {
        // 隣接情報を使って法線を再計算（スムージング）
        DWORD* pAdjData = nullptr;
        pAdjData = (DWORD*)pAdj->GetBufferPointer();

        hr = D3DXComputeNormals(g_pMesh, pAdjData);
    }
    SAFE_RELEASE(pAdj);

    if (pMtl != NULL)
    {
        D3DXMATERIAL* pMaterials = (D3DXMATERIAL*)pMtl->GetBufferPointer();

        g_pTextures.resize(g_dwNumMaterials, NULL);

        for (DWORD i = 0; i < g_dwNumMaterials; ++i)
        {
            if (pMaterials[i].pTextureFilename != NULL &&
                strlen(pMaterials[i].pTextureFilename) > 0)
            {
                const char* s = pMaterials[i].pTextureFilename;
                D3DXCreateTextureFromFileA(g_pd3dDevice, s, &g_pTextures[i]);
            }
        }
    }

    SAFE_RELEASE(pMtl);

    hr = D3DXCreateEffectFromFile(g_pd3dDevice,
                                  L"simple.fx",
                                  NULL,
                                  NULL,
                                  D3DXSHADER_DEBUG,
                                  NULL,
                                  &g_pEffect,
                                  NULL);

    assert(hr == S_OK);

    // Env Cube
    hr = D3DXCreateCubeTextureFromFile(g_pd3dDevice,
                                       L"Texture1.dds",
                                       &g_pEnvCube);

    assert(hr == S_OK);

    // Font
    hr = D3DXCreateFont(g_pd3dDevice,
                        18,
                        0,
                        FW_NORMAL,
                        1,
                        FALSE,
                        DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        DEFAULT_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE,
                        L"BIZ UDゴシック",
                        &g_pFont);

    assert(hr == S_OK);

    return true;
}

static void TextDraw(ID3DXFont* font, const TCHAR* text, int x, int y, D3DCOLOR color)
{
    if (font == NULL)
    {
        return;
    }

    RECT rc;
    SetRect(&rc, x, y, x + 2000, y + 200);
    font->DrawText(NULL,
                   text,
                   -1,
                   &rc,
                   DT_LEFT | DT_TOP | DT_NOCLIP,
                   color);
}

static void Render()
{
    static float t = 0.0f;
    t += 0.03f;

    // 行列
    D3DXMATRIX mW, mV, mP, mWVP;
    D3DXMatrixIdentity(&mW);

    //D3DXVECTOR3 eye(4.0f * sinf(t), 4.f * sinf(t), -4.0f * cosf(t));
    D3DXVECTOR3 eye(4.0f * sinf(t), 2.f, -4.0f * cosf(t));
    D3DXVECTOR4 eye4(eye.x, eye.y, eye.z, 1.0f);
    g_pEffect->SetVector("g_eyePosW", &eye4);
    D3DXVECTOR3 at(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);

    D3DXMatrixLookAtLH(&mV, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&mP,
                               D3DXToRadian(45.0f),
                               (float)WINDOW_SIZE_W / (float)WINDOW_SIZE_H,
                               1.0f,
                               1000.0f);

    mWVP = mW * mV * mP;

    // クリア
    g_pd3dDevice->Clear(0,
                        NULL,
                        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(80, 80, 100),
                        1.0f,
                        0);

    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        // テキスト
        TextDraw(g_pFont, L"環境マッピング", 10, 10, D3DCOLOR_XRGB(255, 255, 255));

        // エフェクト定数
        g_pEffect->SetMatrix("g_matWorldViewProj", &mWVP);
        g_pEffect->SetMatrix("g_matWorld", &mW);
        g_pEffect->SetTexture("EnvMap", g_pEnvCube);

        // 描画
        g_pEffect->SetTechnique("Technique1");

        UINT nPass = 0;
        if (SUCCEEDED(g_pEffect->Begin(&nPass, 0)))
        {
            if (SUCCEEDED(g_pEffect->BeginPass(0)))
            {
                for (DWORD i = 0; i < g_dwNumMaterials; ++i)
                {
                    g_pMesh->DrawSubset(i);
                }

                g_pEffect->EndPass();
            }
            g_pEffect->End();
        }

        g_pd3dDevice->EndScene();
    }

    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

static bool InitAll(HWND hWnd)
{
    if (!InitD3D(hWnd))
    {
        return false;
    }

    if (!LoadMeshAndEffect())
    {
        return false;
    }
    return true;
}

extern int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                           _In_opt_ HINSTANCE hPrevInstance,
                           _In_ LPWSTR lpCmdLine,
                           _In_ int nShowCmd);

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nShowCmd)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"Window1";
    wc.hIconSm = NULL;

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, WINDOW_SIZE_W, WINDOW_SIZE_H);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(L"Window1",
                             L"EnvMap Sample",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right - rect.left,
                             rect.bottom - rect.top,
                             NULL,
                             NULL,
                             GetModuleHandle(NULL),
                             NULL);

    assert(hWnd != NULL);
    g_hWnd = hWnd;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    if (!InitAll(hWnd))
    {
        Cleanup();
        return 0;
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (!g_bClose)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_bClose = true;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            Sleep(16);
            Render();
        }
    }

    Cleanup();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
