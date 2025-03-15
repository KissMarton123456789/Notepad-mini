#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>


int createWindow(WNDCLASSEXW wc, HWND hwnd);
void handleUserInteraction(bool& done);
void refreshFrame();
void renderFrame(ImVec4 clear_color);
void finishRendering(HWND hwnd, WNDCLASSEXW wc);


static const int APP_NUM_FRAMES_IN_FLIGHT = 2;
static const int APP_NUM_BACK_BUFFERS = 2;
static const int APP_SRV_HEAP_SIZE = 64;

struct FrameContext
{
  ID3D12CommandAllocator* CommandAllocator;
  UINT64                      FenceValue;
};

struct ExampleDescriptorHeapAllocator
{
  ID3D12DescriptorHeap* Heap = nullptr;
  D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
  D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
  D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
  UINT                        HeapHandleIncrement;
  ImVector<int>               FreeIndices;

  void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
  {
    IM_ASSERT(Heap == nullptr && FreeIndices.empty());
    Heap = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    HeapType = desc.Type;
    HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
    HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
    HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
    FreeIndices.reserve((int)desc.NumDescriptors);
    for (int n = desc.NumDescriptors; n > 0; n--)
      FreeIndices.push_back(n - 1);
  }
  void Destroy()
  {
    Heap = nullptr;
    FreeIndices.clear();
  }
  void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
  {
    IM_ASSERT(FreeIndices.Size > 0);
    int idx = FreeIndices.back();
    FreeIndices.pop_back();
    out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
    out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
  }
  void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
  {
    int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
    int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
    IM_ASSERT(cpu_idx == gpu_idx);
    FreeIndices.push_back(cpu_idx);
  }
};

static FrameContext                 g_frameContext[APP_NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

static ID3D12Device* g_pd3dDevice = nullptr;
static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
static ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ID3D12CommandQueue* g_pd3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList* g_pd3dCommandList = nullptr;
static ID3D12Fence* g_fence = nullptr;
static HANDLE                       g_fenceEvent = nullptr;
static UINT64                       g_fenceLastSignaledValue = 0;
static IDXGISwapChain3* g_pSwapChain = nullptr;
static bool                         g_SwapChainOccluded = false;
static HANDLE                       g_hSwapChainWaitableObject = nullptr;
static ID3D12Resource* g_mainRenderTargetResource[APP_NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[APP_NUM_BACK_BUFFERS] = {};

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);