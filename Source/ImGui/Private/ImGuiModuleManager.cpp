
#include "ImGuiModuleManager.h"

FImGuiModuleManager::FImGuiModuleManager()
{
	ContextManager.OnContextProxyCreated().AddRaw(this, &FImGuiModuleManager::OnContextProxyCreated);
}

FImGuiModuleManager::~FImGuiModuleManager()
{
}

void FImGuiModuleManager::OnContextProxyCreated(int32 ContextIndex, FImGuiContextProxy& ContextProxy)
{
	ContextProxy.OnDraw().AddLambda([this, ContextIndex]() { /*TODO: ImGuiDemo.DrawControls(ContextIndex);*/ });
}