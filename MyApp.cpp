#include "graphics.h"

int main()
{
  WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"MyExample", nullptr };
  ::RegisterClassExW(&wc);
  HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"MyExample", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

  if (createWindow(wc, hwnd) == 1)
    return 1;

  ImVec4 clear_color = ImVec4(0.65f, 0.55f, 0.60f, 1.00f);

  bool done = false;
  while (!done) 
  {
    handleUserInteraction(done);
    
    if (done)
      break;

    if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
      ::Sleep(10);
      continue;
    }
    g_SwapChainOccluded = false;
  
    refreshFrame();

    // YOUR CODE:
    ImGui::SetNextWindowSize(ImVec2(700, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::Begin("MyApp");
    ImGui::End();
    //-----------
    renderFrame(clear_color);
  }
  finishRendering(hwnd, wc);
  return 0;
}