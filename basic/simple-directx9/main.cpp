#pragma comment( lib, "d3d9.lib" )
#pragma comment( lib, "comdlg32.lib" )
#pragma comment( lib, "comctl32.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <commdlg.h>
#include <commctrl.h>
#include <tchar.h>
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

static const int WINDOW_SIZE_W = 1920;
static const int WINDOW_SIZE_H = 1080;
static const UINT ID_BUTTON_OPEN_MODEL = 1001;
static const UINT ID_EDIT_LIGHT_POWER = 1002;
static const UINT ID_SLIDER_LIGHT_POWER = 1003;
static const UINT ID_EDIT_BASE_COLOR_R = 1004;
static const UINT ID_SLIDER_BASE_COLOR_R = 1005;
static const UINT ID_EDIT_BASE_COLOR_G = 1006;
static const UINT ID_SLIDER_BASE_COLOR_G = 1007;
static const UINT ID_EDIT_BASE_COLOR_B = 1008;
static const UINT ID_SLIDER_BASE_COLOR_B = 1009;
static const UINT ID_CHECK_SRGB_TO_LINEAR = 1010;
static const UINT ID_CHECK_LINEAR_TO_SRGB = 1011;
static const UINT ID_EDIT_ROUGHNESS = 1012;
static const UINT ID_SLIDER_ROUGHNESS = 1013;
static const UINT ID_EDIT_METALLIC = 1014;
static const UINT ID_SLIDER_METALLIC = 1015;
static const UINT ID_EDIT_ENV_REFLECTION_INTENSITY = 1016;
static const UINT ID_SLIDER_ENV_REFLECTION_INTENSITY = 1017;

struct MeshMaterial
{
    D3DMATERIAL9 material;
    LPDIRECT3DTEXTURE9 texture;
    bool hasTexture;

    MeshMaterial()
        : texture(NULL)
        , hasTexture(false)
    {
        ZeroMemory(&material, sizeof(material));
    }
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ControlDialogProc(HWND, UINT, WPARAM, LPARAM);

// D3D
LPDIRECT3D9         g_pD3D = NULL;
LPDIRECT3DDEVICE9   g_pd3dDevice = NULL;
LPD3DXEFFECT        g_pEffect = NULL;
LPD3DXFONT          g_pFont = NULL;

// Mesh
LPD3DXMESH          g_pMesh = NULL;
DWORD               g_dwNumMaterials = 0;
std::vector<MeshMaterial> g_materials;
std::wstring        g_loadedMeshPath;
D3DXVECTOR3         g_modelCenter(0.0f, 0.0f, 0.0f);
float               g_modelRadius = 1.0f;
LPD3DXMESH          g_pBackgroundMesh = NULL;

// Env Cube
LPDIRECT3DCUBETEXTURE9  g_pEnvCube = NULL;

// App
HINSTANCE           g_hInstance = NULL;
HWND                g_hWnd = NULL;
HWND                g_hControlDialog = NULL;
bool                g_bClose = false;
bool                g_isCursorVisible = true;

// Camera
D3DXVECTOR3         g_cameraPosition(0.0f, 1.5f, -6.0f);
float               g_cameraYaw = 0.0f;
float               g_cameraPitch = 0.0f;
float               g_cameraMoveSpeed = 6.0f;
float               g_mouseSensitivity = 0.0035f;
LARGE_INTEGER       g_perfFrequency = {};
LARGE_INTEGER       g_prevFrameCounter = {};
const D3DXVECTOR4   g_lightDirectionW(0.35f, 0.85f, -0.40f, 0.0f);
const D3DXVECTOR4   g_lightColor(1.0f, 1.0f, 1.0f, 1.0f);
float               g_lightPower = 3.14159f;
float               g_pbrBaseColorR = 1.0f;
float               g_pbrBaseColorG = 1.0f;
float               g_pbrBaseColorB = 1.0f;
bool                g_isUpdatingLightPowerUi = false;
bool                g_isUpdatingBaseColorUi = false;
bool                g_enableSrgbToLinear = true;
bool                g_enableLinearToSrgb = true;
float               g_pbrRoughness = 0.5f;
bool                g_isUpdatingRoughnessUi = false;
float               g_pbrMetallic = 0.0f;
bool                g_isUpdatingMetallicUi = false;
float               g_envReflectionIntensity = 1.0f;
bool                g_isUpdatingEnvReflectionUi = false;

static std::wstring GetDirectoryPath(const std::wstring& path)
{
    const std::wstring::size_type pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return L".";
    }

    return path.substr(0, pos);
}

static std::wstring JoinPath(const std::wstring& dir, const std::wstring& fileName)
{
    if (dir.empty())
    {
        return fileName;
    }

    const wchar_t last = dir[dir.size() - 1];
    if (last == L'\\' || last == L'/')
    {
        return dir + fileName;
    }

    return dir + L"\\" + fileName;
}

static std::wstring NormalizePathKey(const std::wstring& path)
{
    std::wstring normalized = path;
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        if (normalized[i] == L'/')
        {
            normalized[i] = L'\\';
        }
        else
        {
            normalized[i] = static_cast<wchar_t>(towlower(normalized[i]));
        }
    }
    return normalized;
}

static std::wstring AnsiToWide(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        return std::wstring();
    }

    const int length = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    if (length <= 0)
    {
        return std::wstring();
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &buffer[0], length);
    return std::wstring(&buffer[0]);
}

static float ClampLightPower(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 10.0f)
    {
        return 10.0f;
    }
    return value;
}

static float ClampBaseColorValue(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static float ClampRoughness(float value)
{
    if (value < 0.04f)
    {
        return 0.04f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static float ClampMetallic(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static float ClampEnvReflectionIntensity(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 3.0f)
    {
        return 3.0f;
    }
    return value;
}

static std::wstring FormatLightPowerText(float value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.5f", value);
    return std::wstring(buffer);
}

static std::wstring FormatBaseColorText(float value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.3f", value);
    return std::wstring(buffer);
}

static std::wstring FormatRoughnessText(float value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.3f", value);
    return std::wstring(buffer);
}

static std::wstring FormatMetallicText(float value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.3f", value);
    return std::wstring(buffer);
}

static std::wstring FormatEnvReflectionIntensityText(float value)
{
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.3f", value);
    return std::wstring(buffer);
}

static LONG LightPowerToSliderPosition(float value)
{
    return static_cast<LONG>(ClampLightPower(value) * 100.0f + 0.5f);
}

static float SliderPositionToLightPower(LONG sliderPosition)
{
    return ClampLightPower(static_cast<float>(sliderPosition) / 100.0f);
}

static LONG BaseColorToSliderPosition(float value)
{
    return static_cast<LONG>(ClampBaseColorValue(value) * 1000.0f + 0.5f);
}

static float SliderPositionToBaseColor(LONG sliderPosition)
{
    return ClampBaseColorValue(static_cast<float>(sliderPosition) / 1000.0f);
}

static LONG RoughnessToSliderPosition(float value)
{
    const float clamped = ClampRoughness(value);
    return static_cast<LONG>(((clamped - 0.04f) / (1.0f - 0.04f)) * 1000.0f + 0.5f);
}

static float SliderPositionToRoughness(LONG sliderPosition)
{
    const float t = static_cast<float>(sliderPosition) / 1000.0f;
    return ClampRoughness(0.04f + t * (1.0f - 0.04f));
}

static LONG MetallicToSliderPosition(float value)
{
    return static_cast<LONG>(ClampMetallic(value) * 1000.0f + 0.5f);
}

static float SliderPositionToMetallic(LONG sliderPosition)
{
    return ClampMetallic(static_cast<float>(sliderPosition) / 1000.0f);
}

static LONG EnvReflectionIntensityToSliderPosition(float value)
{
    return static_cast<LONG>(ClampEnvReflectionIntensity(value) * (1000.0f / 3.0f) + 0.5f);
}

static float SliderPositionToEnvReflectionIntensity(LONG sliderPosition)
{
    return ClampEnvReflectionIntensity(static_cast<float>(sliderPosition) * (3.0f / 1000.0f));
}

static void SyncLightPowerUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_LIGHT_POWER);
    if (hEdit == NULL)
    {
        return;
    }

    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_LIGHT_POWER);

    g_isUpdatingLightPowerUi = true;
    const std::wstring text = FormatLightPowerText(g_lightPower);
    SetWindowTextW(hEdit, text.c_str());
    if (hSlider != NULL)
    {
        SendMessageW(hSlider, TBM_SETPOS, TRUE, LightPowerToSliderPosition(g_lightPower));
    }
    g_isUpdatingLightPowerUi = false;
}

static void SyncBaseColorUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    struct BaseColorControl
    {
        UINT editId;
        UINT sliderId;
        float value;
    };

    const BaseColorControl controls[] =
    {
        { ID_EDIT_BASE_COLOR_R, ID_SLIDER_BASE_COLOR_R, g_pbrBaseColorR },
        { ID_EDIT_BASE_COLOR_G, ID_SLIDER_BASE_COLOR_G, g_pbrBaseColorG },
        { ID_EDIT_BASE_COLOR_B, ID_SLIDER_BASE_COLOR_B, g_pbrBaseColorB },
    };

    g_isUpdatingBaseColorUi = true;

    for (size_t i = 0; i < _countof(controls); ++i)
    {
        HWND hEdit = GetDlgItem(hWnd, controls[i].editId);
        if (hEdit != NULL)
        {
            const std::wstring text = FormatBaseColorText(controls[i].value);
            SetWindowTextW(hEdit, text.c_str());
        }

        HWND hSlider = GetDlgItem(hWnd, controls[i].sliderId);
        if (hSlider != NULL)
        {
            SendMessageW(hSlider, TBM_SETPOS, TRUE, BaseColorToSliderPosition(controls[i].value));
        }
    }

    g_isUpdatingBaseColorUi = false;
}

static void SyncGammaUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    HWND hSrgbToLinear = GetDlgItem(hWnd, ID_CHECK_SRGB_TO_LINEAR);
    if (hSrgbToLinear != NULL)
    {
        SendMessageW(hSrgbToLinear, BM_SETCHECK, g_enableSrgbToLinear ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    HWND hLinearToSrgb = GetDlgItem(hWnd, ID_CHECK_LINEAR_TO_SRGB);
    if (hLinearToSrgb != NULL)
    {
        SendMessageW(hLinearToSrgb, BM_SETCHECK, g_enableLinearToSrgb ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

static void SyncRoughnessUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_ROUGHNESS);
    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_ROUGHNESS);

    g_isUpdatingRoughnessUi = true;

    if (hEdit != NULL)
    {
        const std::wstring text = FormatRoughnessText(g_pbrRoughness);
        SetWindowTextW(hEdit, text.c_str());
    }

    if (hSlider != NULL)
    {
        SendMessageW(hSlider, TBM_SETPOS, TRUE, RoughnessToSliderPosition(g_pbrRoughness));
    }

    g_isUpdatingRoughnessUi = false;
}

static void SyncMetallicUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_METALLIC);
    if (hEdit == NULL)
    {
        return;
    }

    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_METALLIC);

    g_isUpdatingMetallicUi = true;

    const std::wstring text = FormatMetallicText(g_pbrMetallic);
    SetWindowTextW(hEdit, text.c_str());

    if (hSlider != NULL)
    {
        SendMessageW(hSlider, TBM_SETPOS, TRUE, MetallicToSliderPosition(g_pbrMetallic));
    }

    g_isUpdatingMetallicUi = false;
}

static void SyncEnvReflectionUi(HWND hWnd)
{
    if (hWnd == NULL)
    {
        return;
    }

    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_ENV_REFLECTION_INTENSITY);
    if (hEdit == NULL)
    {
        return;
    }

    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_ENV_REFLECTION_INTENSITY);

    g_isUpdatingEnvReflectionUi = true;

    const std::wstring text = FormatEnvReflectionIntensityText(g_envReflectionIntensity);
    SetWindowTextW(hEdit, text.c_str());

    if (hSlider != NULL)
    {
        SendMessageW(hSlider, TBM_SETPOS, TRUE, EnvReflectionIntensityToSliderPosition(g_envReflectionIntensity));
    }

    g_isUpdatingEnvReflectionUi = false;
}

static void ApplyLightPowerFromUi(HWND hWnd)
{
    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_LIGHT_POWER);
    if (hEdit == NULL)
    {
        return;
    }

    wchar_t buffer[64];
    GetWindowTextW(hEdit, buffer, _countof(buffer));
    if (buffer[0] == L'\0')
    {
        return;
    }

    wchar_t* endPtr = NULL;
    const double parsed = wcstod(buffer, &endPtr);
    if (endPtr == buffer)
    {
        return;
    }

    g_lightPower = ClampLightPower(static_cast<float>(parsed));
    SyncLightPowerUi(hWnd);
}

static void ApplyLightPowerFromSlider(HWND hWnd)
{
    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_LIGHT_POWER);
    if (hSlider == NULL)
    {
        return;
    }

    const LONG sliderPosition = static_cast<LONG>(SendMessageW(hSlider, TBM_GETPOS, 0, 0));
    g_lightPower = SliderPositionToLightPower(sliderPosition);
    SyncLightPowerUi(hWnd);
}

static float* GetBaseColorValueFromEditId(UINT editId)
{
    switch (editId)
    {
    case ID_EDIT_BASE_COLOR_R:
        return &g_pbrBaseColorR;
    case ID_EDIT_BASE_COLOR_G:
        return &g_pbrBaseColorG;
    case ID_EDIT_BASE_COLOR_B:
        return &g_pbrBaseColorB;
    default:
        return NULL;
    }
}

static float* GetBaseColorValueFromSliderId(UINT sliderId)
{
    switch (sliderId)
    {
    case ID_SLIDER_BASE_COLOR_R:
        return &g_pbrBaseColorR;
    case ID_SLIDER_BASE_COLOR_G:
        return &g_pbrBaseColorG;
    case ID_SLIDER_BASE_COLOR_B:
        return &g_pbrBaseColorB;
    default:
        return NULL;
    }
}

static void ApplyBaseColorFromUi(HWND hWnd, UINT editId)
{
    float* value = GetBaseColorValueFromEditId(editId);
    if (value == NULL)
    {
        return;
    }

    HWND hEdit = GetDlgItem(hWnd, editId);
    if (hEdit == NULL)
    {
        return;
    }

    wchar_t buffer[64];
    GetWindowTextW(hEdit, buffer, _countof(buffer));
    if (buffer[0] == L'\0')
    {
        return;
    }

    wchar_t* endPtr = NULL;
    const double parsed = wcstod(buffer, &endPtr);
    if (endPtr == buffer)
    {
        return;
    }

    *value = ClampBaseColorValue(static_cast<float>(parsed));
    SyncBaseColorUi(hWnd);
}

static void ApplyBaseColorFromSlider(HWND hWnd, UINT sliderId)
{
    float* value = GetBaseColorValueFromSliderId(sliderId);
    if (value == NULL)
    {
        return;
    }

    HWND hSlider = GetDlgItem(hWnd, sliderId);
    if (hSlider == NULL)
    {
        return;
    }

    const LONG sliderPosition = static_cast<LONG>(SendMessageW(hSlider, TBM_GETPOS, 0, 0));
    *value = SliderPositionToBaseColor(sliderPosition);
    SyncBaseColorUi(hWnd);
}

static void ApplyRoughnessFromUi(HWND hWnd)
{
    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_ROUGHNESS);
    if (hEdit == NULL)
    {
        return;
    }

    wchar_t buffer[64];
    GetWindowTextW(hEdit, buffer, _countof(buffer));
    if (buffer[0] == L'\0')
    {
        return;
    }

    wchar_t* endPtr = NULL;
    const double parsed = wcstod(buffer, &endPtr);
    if (endPtr == buffer)
    {
        return;
    }

    g_pbrRoughness = ClampRoughness(static_cast<float>(parsed));
    SyncRoughnessUi(hWnd);
}

static void ApplyRoughnessFromSlider(HWND hWnd)
{
    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_ROUGHNESS);
    if (hSlider == NULL)
    {
        return;
    }

    const LONG sliderPosition = static_cast<LONG>(SendMessageW(hSlider, TBM_GETPOS, 0, 0));
    g_pbrRoughness = SliderPositionToRoughness(sliderPosition);
    SyncRoughnessUi(hWnd);
}

static void ApplyMetallicFromUi(HWND hWnd)
{
    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_METALLIC);
    if (hEdit == NULL)
    {
        return;
    }

    wchar_t buffer[64];
    GetWindowTextW(hEdit, buffer, _countof(buffer));
    if (buffer[0] == L'\0')
    {
        return;
    }

    wchar_t* endPtr = NULL;
    const double parsed = wcstod(buffer, &endPtr);
    if (endPtr == buffer)
    {
        return;
    }

    g_pbrMetallic = ClampMetallic(static_cast<float>(parsed));
    SyncMetallicUi(hWnd);
}

static void ApplyMetallicFromSlider(HWND hWnd)
{
    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_METALLIC);
    if (hSlider == NULL)
    {
        return;
    }

    const LONG sliderPosition = static_cast<LONG>(SendMessageW(hSlider, TBM_GETPOS, 0, 0));
    g_pbrMetallic = SliderPositionToMetallic(sliderPosition);
    SyncMetallicUi(hWnd);
}

static void ApplyEnvReflectionFromUi(HWND hWnd)
{
    HWND hEdit = GetDlgItem(hWnd, ID_EDIT_ENV_REFLECTION_INTENSITY);
    if (hEdit == NULL)
    {
        return;
    }

    wchar_t buffer[64];
    GetWindowTextW(hEdit, buffer, _countof(buffer));
    if (buffer[0] == L'\0')
    {
        return;
    }

    wchar_t* endPtr = NULL;
    const double parsed = wcstod(buffer, &endPtr);
    if (endPtr == buffer)
    {
        return;
    }

    g_envReflectionIntensity = ClampEnvReflectionIntensity(static_cast<float>(parsed));
    SyncEnvReflectionUi(hWnd);
}

static void ApplyEnvReflectionFromSlider(HWND hWnd)
{
    HWND hSlider = GetDlgItem(hWnd, ID_SLIDER_ENV_REFLECTION_INTENSITY);
    if (hSlider == NULL)
    {
        return;
    }

    const LONG sliderPosition = static_cast<LONG>(SendMessageW(hSlider, TBM_GETPOS, 0, 0));
    g_envReflectionIntensity = SliderPositionToEnvReflectionIntensity(sliderPosition);
    SyncEnvReflectionUi(hWnd);
}

static void ReleaseMeshResources()
{
    for (size_t i = 0; i < g_materials.size(); ++i)
    {
        SAFE_RELEASE(g_materials[i].texture);
    }
    g_materials.clear();
    g_dwNumMaterials = 0;
    SAFE_RELEASE(g_pMesh);
}

static void ReleaseBackgroundResources()
{
    SAFE_RELEASE(g_pBackgroundMesh);
}

static void SetCursorVisible(bool visible)
{
    if (visible == g_isCursorVisible)
    {
        return;
    }

    if (visible)
    {
        while (ShowCursor(TRUE) < 0)
        {
        }
        ReleaseCapture();
        ClipCursor(NULL);
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
        {
        }
        SetCapture(g_hWnd);
    }

    g_isCursorVisible = visible;
}

static void CenterCursorInClient()
{
    if (g_hWnd == NULL)
    {
        return;
    }

    RECT rc;
    GetClientRect(g_hWnd, &rc);

    POINT pt;
    pt.x = (rc.left + rc.right) / 2;
    pt.y = (rc.top + rc.bottom) / 2;
    ClientToScreen(g_hWnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

static void ToggleCursorVisible()
{
    SetCursorVisible(!g_isCursorVisible);
    if (!g_isCursorVisible)
    {
        CenterCursorInClient();
    }
}

static void ResetCameraForModel()
{
    const float safeRadius = (g_modelRadius > 0.001f) ? g_modelRadius : 1.0f;
    const float cameraDistance = (safeRadius > 2.0f) ? safeRadius * 1.25f : 2.0f;
    g_cameraPosition = D3DXVECTOR3(0.0f, safeRadius * 0.20f, -cameraDistance);
    g_cameraYaw = 0.0f;
    g_cameraPitch = 0.0f;

    if (!g_isCursorVisible)
    {
        CenterCursorInClient();
    }
}

static void UpdateWindowTitle()
{
    std::wstring title = L"PBR Study - ";
    title += g_loadedMeshPath.empty() ? L"(no mesh)" : g_loadedMeshPath;
    SetWindowTextW(g_hWnd, title.c_str());
}

static bool LoadMeshFromFile(const std::wstring& filePath)
{
    LPD3DXBUFFER adjacencyBuffer = NULL;
    LPD3DXBUFFER materialBuffer = NULL;
    LPD3DXMESH newMesh = NULL;
    DWORD materialCount = 0;

    HRESULT hr = D3DXLoadMeshFromXW(filePath.c_str(),
                                    D3DXMESH_MANAGED,
                                    g_pd3dDevice,
                                    &adjacencyBuffer,
                                    &materialBuffer,
                                    NULL,
                                    &materialCount,
                                    &newMesh);
    if (FAILED(hr))
    {
        SAFE_RELEASE(adjacencyBuffer);
        SAFE_RELEASE(materialBuffer);
        SAFE_RELEASE(newMesh);
        return false;
    }

    if (adjacencyBuffer != NULL)
    {
        DWORD* adjacency = static_cast<DWORD*>(adjacencyBuffer->GetBufferPointer());
        D3DXComputeNormals(newMesh, adjacency);
    }

    std::vector<MeshMaterial> newMaterials;
    if (materialBuffer != NULL && materialCount > 0)
    {
        const D3DXMATERIAL* d3dxMaterials = static_cast<const D3DXMATERIAL*>(materialBuffer->GetBufferPointer());
        const std::wstring baseDirectory = GetDirectoryPath(filePath);
        std::map<std::wstring, LPDIRECT3DTEXTURE9> textureCache;
        newMaterials.resize(materialCount);

        for (DWORD i = 0; i < materialCount; ++i)
        {
            newMaterials[i].material = d3dxMaterials[i].MatD3D;
            newMaterials[i].material.Ambient = newMaterials[i].material.Diffuse;

            if (d3dxMaterials[i].pTextureFilename != NULL && d3dxMaterials[i].pTextureFilename[0] != '\0')
            {
                const std::wstring textureName = AnsiToWide(d3dxMaterials[i].pTextureFilename);
                const std::wstring texturePath = JoinPath(baseDirectory, textureName);
                const std::wstring primaryKey = NormalizePathKey(texturePath);
                const std::wstring fallbackKey = NormalizePathKey(textureName);
                LPDIRECT3DTEXTURE9 texture = NULL;

                std::map<std::wstring, LPDIRECT3DTEXTURE9>::const_iterator cacheIt = textureCache.find(primaryKey);
                if (cacheIt == textureCache.end())
                {
                    cacheIt = textureCache.find(fallbackKey);
                }

                if (cacheIt != textureCache.end())
                {
                    texture = cacheIt->second;
                    texture->AddRef();
                }
                else
                {
                    hr = D3DXCreateTextureFromFileW(g_pd3dDevice, texturePath.c_str(), &texture);
                    if (SUCCEEDED(hr))
                    {
                        textureCache[primaryKey] = texture;
                        textureCache[fallbackKey] = texture;
                    }
                    else
                    {
                        hr = D3DXCreateTextureFromFileW(g_pd3dDevice, textureName.c_str(), &texture);
                        if (SUCCEEDED(hr))
                        {
                            textureCache[fallbackKey] = texture;
                        }
                    }
                }

                if (texture != NULL)
                {
                    newMaterials[i].texture = texture;
                    newMaterials[i].hasTexture = true;
                }
            }
        }
    }
    else
    {
        MeshMaterial material;
        material.material.Diffuse.r = 0.8f;
        material.material.Diffuse.g = 0.8f;
        material.material.Diffuse.b = 0.8f;
        material.material.Diffuse.a = 1.0f;
        material.material.Ambient = material.material.Diffuse;
        newMaterials.push_back(material);
        materialCount = 1;
    }

    BYTE* vertices = NULL;
    hr = newMesh->LockVertexBuffer(D3DLOCK_READONLY, reinterpret_cast<void**>(&vertices));
    if (SUCCEEDED(hr))
    {
        D3DXComputeBoundingSphere(reinterpret_cast<const D3DXVECTOR3*>(vertices),
                                  newMesh->GetNumVertices(),
                                  newMesh->GetNumBytesPerVertex(),
                                  &g_modelCenter,
                                  &g_modelRadius);
        newMesh->UnlockVertexBuffer();
    }
    else
    {
        g_modelCenter = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        g_modelRadius = 1.0f;
    }

    ReleaseMeshResources();
    g_pMesh = newMesh;
    g_dwNumMaterials = materialCount;
    g_materials.swap(newMaterials);
    g_loadedMeshPath = filePath;
    ResetCameraForModel();
    UpdateWindowTitle();

    SAFE_RELEASE(adjacencyBuffer);
    SAFE_RELEASE(materialBuffer);
    return true;
}

static bool LoadBackgroundMesh(const std::wstring& filePath)
{
    LPD3DXBUFFER adjacencyBuffer = NULL;
    LPD3DXBUFFER materialBuffer = NULL;
    LPD3DXMESH newMesh = NULL;
    DWORD materialCount = 0;

    HRESULT hr = D3DXLoadMeshFromXW(filePath.c_str(),
                                    D3DXMESH_MANAGED,
                                    g_pd3dDevice,
                                    &adjacencyBuffer,
                                    &materialBuffer,
                                    NULL,
                                    &materialCount,
                                    &newMesh);

    SAFE_RELEASE(adjacencyBuffer);
    SAFE_RELEASE(materialBuffer);

    if (FAILED(hr))
    {
        SAFE_RELEASE(newMesh);
        return false;
    }

    ReleaseBackgroundResources();
    g_pBackgroundMesh = newMesh;
    return true;
}

static bool LoadEffectAndAssets()
{
    HRESULT hr = D3DXCreateEffectFromFileW(g_pd3dDevice,
                                           L"simple.fx",
                                           NULL,
                                           NULL,
                                           D3DXSHADER_DEBUG,
                                           NULL,
                                           &g_pEffect,
                                           NULL);
    if (FAILED(hr))
    {
        return false;
    }

    hr = D3DXCreateCubeTextureFromFileW(g_pd3dDevice,
                                        L"Texture1.dds",
                                        &g_pEnvCube);
    if (FAILED(hr))
    {
        return false;
    }

    hr = D3DXCreateFontW(g_pd3dDevice,
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
    if (FAILED(hr))
    {
        return false;
    }

    if (!LoadBackgroundMesh(L"cubeBack.x"))
    {
        return false;
    }

    return LoadMeshFromFile(L"sphere.x");
}

static void Cleanup()
{
    SetCursorVisible(true);

    if (g_hControlDialog != NULL)
    {
        DestroyWindow(g_hControlDialog);
        g_hControlDialog = NULL;
    }

    ReleaseMeshResources();
    ReleaseBackgroundResources();
    SAFE_RELEASE(g_pEnvCube);
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

static void ShowModelDialog()
{
    if (g_hControlDialog == NULL)
    {
        const int dialogWidth = 620;
        const int dialogHeight = 760;
        RECT rcMain;
        GetWindowRect(g_hWnd, &rcMain);

        g_hControlDialog = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
                                           L"ModelControlDialog",
                                           L"Model Loader",
                                           WS_CAPTION | WS_SYSMENU | WS_POPUP,
                                           rcMain.left + 60,
                                           rcMain.top + 60,
                                           dialogWidth,
                                           dialogHeight,
                                           g_hWnd,
                                           NULL,
                                           g_hInstance,
                                           NULL);
        assert(g_hControlDialog != NULL);
    }

    SetCursorVisible(true);
    ShowWindow(g_hControlDialog, SW_SHOWNORMAL);
    UpdateWindow(g_hControlDialog);
    SyncLightPowerUi(g_hControlDialog);
    SyncBaseColorUi(g_hControlDialog);
    SyncGammaUi(g_hControlDialog);
    SyncRoughnessUi(g_hControlDialog);
    SyncMetallicUi(g_hControlDialog);
    SyncEnvReflectionUi(g_hControlDialog);
    SetForegroundWindow(g_hControlDialog);
}

static void OpenModelFileDialog()
{
    wchar_t filePath[MAX_PATH] = L"";

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (g_hControlDialog != NULL) ? g_hControlDialog : g_hWnd;
    ofn.lpstrFilter = L"X Files (*.x)\0*.x\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = _countof(filePath);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Xファイルを選択";

    if (!GetOpenFileNameW(&ofn))
    {
        return;
    }

    if (!LoadMeshFromFile(filePath))
    {
        MessageBoxW(g_hWnd,
                    L"Xファイルの読み込みに失敗しました。",
                    L"読み込みエラー",
                    MB_ICONERROR | MB_OK);
    }
}

static void UpdateCamera(float deltaSeconds)
{
    if (deltaSeconds <= 0.0f)
    {
        return;
    }

    if (GetActiveWindow() != g_hWnd)
    {
        return;
    }

    D3DXVECTOR3 forward(cosf(g_cameraPitch) * sinf(g_cameraYaw),
                        sinf(g_cameraPitch),
                        cosf(g_cameraPitch) * cosf(g_cameraYaw));
    D3DXVec3Normalize(&forward, &forward);

    const D3DXVECTOR3 worldUp(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &worldUp, &forward);
    D3DXVec3Normalize(&right, &right);

    D3DXVECTOR3 up;
    D3DXVec3Cross(&up, &forward, &right);
    D3DXVec3Normalize(&up, &up);

    const float moveAmount = g_cameraMoveSpeed * deltaSeconds;
    if (GetAsyncKeyState('W') & 0x8000)
    {
        g_cameraPosition += forward * moveAmount;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        g_cameraPosition -= forward * moveAmount;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        g_cameraPosition += right * moveAmount;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        g_cameraPosition -= right * moveAmount;
    }
    if (GetAsyncKeyState('E') & 0x8000)
    {
        g_cameraPosition.y += moveAmount;
    }
    if (GetAsyncKeyState('Q') & 0x8000)
    {
        g_cameraPosition.y -= moveAmount;
    }

    if (!g_isCursorVisible)
    {
        RECT rc;
        GetClientRect(g_hWnd, &rc);

        POINT center;
        center.x = (rc.left + rc.right) / 2;
        center.y = (rc.top + rc.bottom) / 2;
        ClientToScreen(g_hWnd, &center);

        POINT cursorPos;
        GetCursorPos(&cursorPos);

        const LONG deltaX = cursorPos.x - center.x;
        const LONG deltaY = cursorPos.y - center.y;

        g_cameraYaw += static_cast<float>(deltaX) * g_mouseSensitivity;
        g_cameraPitch -= static_cast<float>(deltaY) * g_mouseSensitivity;

        const float pitchLimit = D3DXToRadian(89.0f);
        if (g_cameraPitch > pitchLimit)
        {
            g_cameraPitch = pitchLimit;
        }
        if (g_cameraPitch < -pitchLimit)
        {
            g_cameraPitch = -pitchLimit;
        }

        CenterCursorInClient();
    }
}

static void Render()
{
    D3DXMATRIX mW, mV, mP, mWVP;
    D3DXMatrixTranslation(&mW, -g_modelCenter.x, -g_modelCenter.y, -g_modelCenter.z);

    D3DXVECTOR3 eye = g_cameraPosition;
    D3DXVECTOR4 cameraPositionW(eye.x, eye.y, eye.z, 1.0f);
    D3DXVECTOR4 pbrBaseColor(g_pbrBaseColorR, g_pbrBaseColorG, g_pbrBaseColorB, 1.0f);
    g_pEffect->SetVector("g_cameraPositionW", &cameraPositionW);
    g_pEffect->SetVector("g_lightDirectionW", &g_lightDirectionW);
    g_pEffect->SetVector("g_lightColor", &g_lightColor);
    g_pEffect->SetFloat("g_lightPower", g_lightPower);
    g_pEffect->SetFloat("g_pbrRoughness", g_pbrRoughness);
    g_pEffect->SetFloat("g_pbrMetallic", g_pbrMetallic);
    g_pEffect->SetFloat("g_envReflectionIntensity", g_envReflectionIntensity);
    g_pEffect->SetVector("g_pbrBaseColorFactor", &pbrBaseColor);
    g_pEffect->SetBool("g_enableSrgbToLinear", g_enableSrgbToLinear);
    g_pEffect->SetBool("g_enableLinearToSrgb", g_enableLinearToSrgb);

    D3DXVECTOR3 forward(cosf(g_cameraPitch) * sinf(g_cameraYaw),
                        sinf(g_cameraPitch),
                        cosf(g_cameraPitch) * cosf(g_cameraYaw));
    D3DXVec3Normalize(&forward, &forward);

    D3DXVECTOR3 worldUp(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &worldUp, &forward);
    D3DXVec3Normalize(&right, &right);

    D3DXVECTOR3 up;
    D3DXVec3Cross(&up, &forward, &right);
    D3DXVec3Normalize(&up, &up);

    const D3DXVECTOR3 at = eye + forward;
    D3DXMatrixLookAtLH(&mV, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&mP,
                               D3DXToRadian(45.0f),
                               static_cast<float>(WINDOW_SIZE_W) / static_cast<float>(WINDOW_SIZE_H),
                               0.1f,
                               1000.0f);

    mWVP = mW * mV * mP;

    g_pd3dDevice->Clear(0,
                        NULL,
                        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(80, 80, 100),
                        1.0f,
                        0);

    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        TextDraw(g_pFont, L"WASD:移動  E/Q:上下  Esc:カーソル表示切替  F1:モデルダイアログ", 10, 10, D3DCOLOR_XRGB(255, 255, 255));
        TextDraw(g_pFont, g_isCursorVisible ? L"マウスルック: OFF" : L"マウスルック: ON", 10, 34, D3DCOLOR_XRGB(255, 255, 180));
        TextDraw(g_pFont, L"表示モード: PBR Direct Light (Diffuse + Cook-Torrance Specular)", 10, 58, D3DCOLOR_XRGB(180, 255, 220));
        TextDraw(g_pFont, g_loadedMeshPath.c_str(), 10, 82, D3DCOLOR_XRGB(220, 240, 255));
        wchar_t pbrInfo[128];
        swprintf_s(pbrInfo,
                   L"PBR BaseColor Factor: %.2f, %.2f, %.2f",
                   g_pbrBaseColorR,
                   g_pbrBaseColorG,
                   g_pbrBaseColorB);
        TextDraw(g_pFont, pbrInfo, 10, 106, D3DCOLOR_XRGB(220, 255, 220));
        TextDraw(g_pFont, L"albedo = BaseColorFactor * MaterialDiffuse * TextureColor", 10, 130, D3DCOLOR_XRGB(220, 235, 255));
        wchar_t roughnessInfo[128];
        swprintf_s(roughnessInfo, L"PBR Roughness: %.2f", g_pbrRoughness);
        TextDraw(g_pFont, roughnessInfo, 10, 154, D3DCOLOR_XRGB(255, 230, 200));
        wchar_t metallicInfo[128];
        swprintf_s(metallicInfo, L"PBR Metallic: %.2f", g_pbrMetallic);
        TextDraw(g_pFont, metallicInfo, 10, 178, D3DCOLOR_XRGB(255, 230, 200));
        wchar_t envReflectionInfo[128];
        swprintf_s(envReflectionInfo, L"Env Reflection Intensity: %.2f", g_envReflectionIntensity);
        TextDraw(g_pFont, envReflectionInfo, 10, 202, D3DCOLOR_XRGB(255, 230, 200));
        TextDraw(g_pFont, L"Env Reflection: Simple CubeMap", 10, 226, D3DCOLOR_XRGB(255, 230, 200));
        TextDraw(g_pFont, g_enableSrgbToLinear ? L"sRGB To Linear: ON" : L"sRGB To Linear: OFF", 10, 250, D3DCOLOR_XRGB(255, 230, 200));
        TextDraw(g_pFont, g_enableLinearToSrgb ? L"Linear To sRGB: ON" : L"Linear To sRGB: OFF", 10, 274, D3DCOLOR_XRGB(255, 230, 200));

        g_pEffect->SetMatrix("g_matWorldViewProj", &mWVP);
        g_pEffect->SetMatrix("g_matWorld", &mW);
        g_pEffect->SetTexture("EnvMap", g_pEnvCube);

        if (g_pBackgroundMesh != NULL)
        {
            D3DXMATRIX skyScale;
            D3DXMATRIX skyWorld;
            D3DXMATRIX skyWvp;

            D3DXMatrixScaling(&skyScale, 8.0f, 8.0f, 8.0f);
            skyWorld = skyScale;
            skyWvp = skyWorld * mV * mP;

            g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
            g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

            g_pEffect->SetMatrix("g_matWorldViewProj", &skyWvp);
            g_pEffect->SetMatrix("g_matWorld", &skyWorld);
            g_pEffect->SetTechnique("SkyboxTechnique");

            UINT skyPassCount = 0;
            if (SUCCEEDED(g_pEffect->Begin(&skyPassCount, 0)))
            {
                for (UINT skyPassIndex = 0; skyPassIndex < skyPassCount; ++skyPassIndex)
                {
                    if (SUCCEEDED(g_pEffect->BeginPass(skyPassIndex)))
                    {
                        g_pBackgroundMesh->DrawSubset(0);
                        g_pEffect->EndPass();
                    }
                }
                g_pEffect->End();
            }

            g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
            g_pEffect->SetMatrix("g_matWorldViewProj", &mWVP);
            g_pEffect->SetMatrix("g_matWorld", &mW);
        }

        g_pEffect->SetTechnique("PbrDirectLightTechnique");

        UINT passCount = 0;
        if (SUCCEEDED(g_pEffect->Begin(&passCount, 0)))
        {
            for (UINT passIndex = 0; passIndex < passCount; ++passIndex)
            {
                if (SUCCEEDED(g_pEffect->BeginPass(passIndex)))
                {
                    for (DWORD materialIndex = 0; materialIndex < g_dwNumMaterials; ++materialIndex)
                    {
                        const MeshMaterial& material = g_materials[materialIndex];
                        D3DXVECTOR4 diffuse(material.material.Diffuse.r,
                                            material.material.Diffuse.g,
                                            material.material.Diffuse.b,
                                            material.material.Diffuse.a);
                        g_pEffect->SetVector("g_materialDiffuse", &diffuse);
                        g_pEffect->SetBool("g_hasDiffuseTexture", material.hasTexture);
                        g_pEffect->SetTexture("DiffuseMap", material.texture);
                        g_pEffect->CommitChanges();
                        g_pMesh->DrawSubset(materialIndex);
                    }

                    g_pEffect->EndPass();
                }
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

    if (!LoadEffectAndAssets())
    {
        return false;
    }

    QueryPerformanceFrequency(&g_perfFrequency);
    QueryPerformanceCounter(&g_prevFrameCounter);
    UpdateWindowTitle();
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
    g_hInstance = hInstance;

    WNDCLASSEXW mainWindowClass;
    ZeroMemory(&mainWindowClass, sizeof(mainWindowClass));
    mainWindowClass.cbSize = sizeof(mainWindowClass);
    mainWindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    mainWindowClass.lpfnWndProc = WndProc;
    mainWindowClass.hInstance = hInstance;
    mainWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    mainWindowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    mainWindowClass.lpszClassName = L"Window1";

    WNDCLASSEXW dialogWindowClass;
    ZeroMemory(&dialogWindowClass, sizeof(dialogWindowClass));
    dialogWindowClass.cbSize = sizeof(dialogWindowClass);
    dialogWindowClass.style = CS_HREDRAW | CS_VREDRAW;
    dialogWindowClass.lpfnWndProc = ControlDialogProc;
    dialogWindowClass.hInstance = hInstance;
    dialogWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    dialogWindowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    dialogWindowClass.lpszClassName = L"ModelControlDialog";

    INITCOMMONCONTROLSEX commonControls;
    ZeroMemory(&commonControls, sizeof(commonControls));
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&commonControls);

    ATOM mainAtom = RegisterClassExW(&mainWindowClass);
    ATOM dialogAtom = RegisterClassExW(&dialogWindowClass);
    assert(mainAtom != 0);
    assert(dialogAtom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, WINDOW_SIZE_W, WINDOW_SIZE_H);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"Window1",
                              L"PBR Study",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              rect.right - rect.left,
                              rect.bottom - rect.top,
                              NULL,
                              NULL,
                              hInstance,
                              NULL);

    assert(hWnd != NULL);
    g_hWnd = hWnd;

    ShowWindow(hWnd, nShowCmd);
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
            LARGE_INTEGER currentCounter;
            QueryPerformanceCounter(&currentCounter);
            const float deltaSeconds = static_cast<float>(currentCounter.QuadPart - g_prevFrameCounter.QuadPart) / static_cast<float>(g_perfFrequency.QuadPart);
            g_prevFrameCounter = currentCounter;

            UpdateCamera(deltaSeconds);
            Sleep(1);
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
    case WM_ACTIVATEAPP:
    {
        if (wParam == FALSE)
        {
            SetCursorVisible(true);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        const bool isFirstPress = (lParam & 0x40000000) == 0;
        if (isFirstPress)
        {
            if (wParam == VK_ESCAPE)
            {
                ToggleCursorVisible();
                return 0;
            }
            if (wParam == VK_F1)
            {
                ShowModelDialog();
                return 0;
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK ControlDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC",
                      L"F1で開くモデルダイアログです。",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      16,
                      500,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"BUTTON",
                      L"Xファイルを開く",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      16,
                      44,
                      180,
                      32,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BUTTON_OPEN_MODEL)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"複数マテリアル付きの .x にも対応します。",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      82,
                      500,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"Light Power  Intensity : 0.0 - 10.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      108,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      128,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSlider = CreateWindowW(TRACKBAR_CLASSW,
                                     L"",
                                     WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                     52,
                                     124,
                                     340,
                                     32,
                                     hWnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_LIGHT_POWER)),
                                     g_hInstance,
                                     NULL);
        if (hSlider != NULL)
        {
            SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSlider, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSlider, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSlider, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"10.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      128,
                      36,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"3.14159",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      124,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_LIGHT_POWER)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Diffuse表示で使用します。",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      162,
                      320,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Base Color R : 0.0 - 1.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      188,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      208,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderBaseColorR = CreateWindowW(TRACKBAR_CLASSW,
                                               L"",
                                               WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                               52,
                                               204,
                                               340,
                                               32,
                                               hWnd,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_BASE_COLOR_R)),
                                               g_hInstance,
                                               NULL);
        if (hSliderBaseColorR != NULL)
        {
            SendMessageW(hSliderBaseColorR, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderBaseColorR, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderBaseColorR, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderBaseColorR, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"1.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      208,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"1.000",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      204,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_BASE_COLOR_R)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Base Color G : 0.0 - 1.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      250,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      270,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderBaseColorG = CreateWindowW(TRACKBAR_CLASSW,
                                               L"",
                                               WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                               52,
                                               266,
                                               340,
                                               32,
                                               hWnd,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_BASE_COLOR_G)),
                                               g_hInstance,
                                               NULL);
        if (hSliderBaseColorG != NULL)
        {
            SendMessageW(hSliderBaseColorG, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderBaseColorG, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderBaseColorG, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderBaseColorG, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"1.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      270,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"1.000",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      266,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_BASE_COLOR_G)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Base Color B : 0.0 - 1.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      312,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      332,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderBaseColorB = CreateWindowW(TRACKBAR_CLASSW,
                                               L"",
                                               WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                               52,
                                               328,
                                               340,
                                               32,
                                               hWnd,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_BASE_COLOR_B)),
                                               g_hInstance,
                                               NULL);
        if (hSliderBaseColorB != NULL)
        {
            SendMessageW(hSliderBaseColorB, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderBaseColorB, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderBaseColorB, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderBaseColorB, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"1.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      332,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"1.000",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      328,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_BASE_COLOR_B)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Roughness : 0.04 - 1.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      374,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.04",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      394,
                      36,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderRoughness = CreateWindowW(TRACKBAR_CLASSW,
                                              L"",
                                              WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                              52,
                                              390,
                                              340,
                                              32,
                                              hWnd,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_ROUGHNESS)),
                                              g_hInstance,
                                              NULL);
        if (hSliderRoughness != NULL)
        {
            SendMessageW(hSliderRoughness, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderRoughness, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderRoughness, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderRoughness, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"1.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      394,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"0.500",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      390,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_ROUGHNESS)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"PBR Metallic : 0.0 - 1.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      436,
                      300,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      456,
                      36,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderMetallic = CreateWindowW(TRACKBAR_CLASSW,
                                             L"",
                                             WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                             52,
                                             452,
                                             340,
                                             32,
                                             hWnd,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_METALLIC)),
                                             g_hInstance,
                                             NULL);
        if (hSliderMetallic != NULL)
        {
            SendMessageW(hSliderMetallic, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderMetallic, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderMetallic, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderMetallic, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"1.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      456,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"0.000",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      452,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_METALLIC)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"Env Reflection Intensity : 0.0 - 3.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      498,
                      320,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"STATIC",
                      L"0.0",
                      WS_CHILD | WS_VISIBLE,
                      16,
                      518,
                      36,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        HWND hSliderEnvReflection = CreateWindowW(TRACKBAR_CLASSW,
                                                  L"",
                                                  WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                                  52,
                                                  514,
                                                  340,
                                                  32,
                                                  hWnd,
                                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_ENV_REFLECTION_INTENSITY)),
                                                  g_hInstance,
                                                  NULL);
        if (hSliderEnvReflection != NULL)
        {
            SendMessageW(hSliderEnvReflection, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
            SendMessageW(hSliderEnvReflection, TBM_SETTICFREQ, 100, 0);
            SendMessageW(hSliderEnvReflection, TBM_SETPAGESIZE, 0, 50);
            SendMessageW(hSliderEnvReflection, TBM_SETLINESIZE, 0, 1);
        }

        CreateWindowW(L"STATIC",
                      L"3.0",
                      WS_CHILD | WS_VISIBLE,
                      398,
                      518,
                      30,
                      20,
                      hWnd,
                      NULL,
                      g_hInstance,
                      NULL);

        CreateWindowW(L"EDIT",
                      L"1.000",
                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      456,
                      514,
                      84,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_ENV_REFLECTION_INTENSITY)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"BUTTON",
                      L"sRGB To Linear",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      16,
                      564,
                      180,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHECK_SRGB_TO_LINEAR)),
                      g_hInstance,
                      NULL);

        CreateWindowW(L"BUTTON",
                      L"Linear To sRGB",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      16,
                      590,
                      180,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHECK_LINEAR_TO_SRGB)),
                      g_hInstance,
                      NULL);

        SyncLightPowerUi(hWnd);
        SyncBaseColorUi(hWnd);
        SyncGammaUi(hWnd);
        SyncRoughnessUi(hWnd);
        SyncMetallicUi(hWnd);
        SyncEnvReflectionUi(hWnd);
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BUTTON_OPEN_MODEL && HIWORD(wParam) == BN_CLICKED)
        {
            OpenModelFileDialog();
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_LIGHT_POWER && HIWORD(wParam) == EN_CHANGE && !g_isUpdatingLightPowerUi)
        {
            ApplyLightPowerFromUi(hWnd);
            return 0;
        }
        if ((LOWORD(wParam) == ID_EDIT_BASE_COLOR_R ||
             LOWORD(wParam) == ID_EDIT_BASE_COLOR_G ||
             LOWORD(wParam) == ID_EDIT_BASE_COLOR_B) &&
            HIWORD(wParam) == EN_CHANGE &&
            !g_isUpdatingBaseColorUi)
        {
            ApplyBaseColorFromUi(hWnd, LOWORD(wParam));
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_ROUGHNESS &&
            HIWORD(wParam) == EN_CHANGE &&
            !g_isUpdatingRoughnessUi)
        {
            ApplyRoughnessFromUi(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_METALLIC &&
            HIWORD(wParam) == EN_CHANGE &&
            !g_isUpdatingMetallicUi)
        {
            ApplyMetallicFromUi(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_ENV_REFLECTION_INTENSITY &&
            HIWORD(wParam) == EN_CHANGE &&
            !g_isUpdatingEnvReflectionUi)
        {
            ApplyEnvReflectionFromUi(hWnd);
            return 0;
        }
        if ((LOWORD(wParam) == ID_CHECK_SRGB_TO_LINEAR || LOWORD(wParam) == ID_CHECK_LINEAR_TO_SRGB) &&
            HIWORD(wParam) == BN_CLICKED)
        {
            if (LOWORD(wParam) == ID_CHECK_SRGB_TO_LINEAR)
            {
                g_enableSrgbToLinear = (SendMessageW(reinterpret_cast<HWND>(lParam), BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            else
            {
                g_enableLinearToSrgb = (SendMessageW(reinterpret_cast<HWND>(lParam), BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            SyncGammaUi(hWnd);
            return 0;
        }
        break;
    }

    case WM_HSCROLL:
    {
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_LIGHT_POWER) && !g_isUpdatingLightPowerUi)
        {
            ApplyLightPowerFromSlider(hWnd);
            return 0;
        }
        if ((reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_BASE_COLOR_R) ||
             reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_BASE_COLOR_G) ||
             reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_BASE_COLOR_B)) &&
            !g_isUpdatingBaseColorUi)
        {
            const UINT sliderId = static_cast<UINT>(GetDlgCtrlID(reinterpret_cast<HWND>(lParam)));
            ApplyBaseColorFromSlider(hWnd, sliderId);
            return 0;
        }
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_ROUGHNESS) &&
            !g_isUpdatingRoughnessUi)
        {
            ApplyRoughnessFromSlider(hWnd);
            return 0;
        }
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_METALLIC) &&
            !g_isUpdatingMetallicUi)
        {
            ApplyMetallicFromSlider(hWnd);
            return 0;
        }
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hWnd, ID_SLIDER_ENV_REFLECTION_INTENSITY) &&
            !g_isUpdatingEnvReflectionUi)
        {
            ApplyEnvReflectionFromSlider(hWnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
    {
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }

    case WM_DESTROY:
    {
        if (g_hControlDialog == hWnd)
        {
            g_hControlDialog = NULL;
        }
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
