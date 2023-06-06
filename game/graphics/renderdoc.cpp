#include "game/graphics/renderdoc.h"

#include <dlfcn.h>

#include "third-party/renderdoc/renderdoc_app.h"

RenderDoc::RenderDoc() {
  RENDERDOC_API_1_1_2* rdoc_api = nullptr;
  // At init, on windows
  // if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
  //  pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod,
  //  "RENDERDOC_GetAPI"); int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2,
  //  (void**)&rdoc_api); assert(ret == 1);
  //}

  // At init, on linux/android.
  // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
  if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
    // assert(ret == 1);
  }

  // rdoc_api->DiscardFrameCapture
}