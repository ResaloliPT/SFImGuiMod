#pragma once

#include "ImGuiContextManager.h"

class FImGuiModuleManager
{
	friend class FImGuiModule;

public:
	
private:
	FImGuiModuleManager();
	~FImGuiModuleManager();

	FImGuiModuleManager(const FImGuiModuleManager&) = delete;
	FImGuiModuleManager& operator=(const FImGuiModuleManager&) = delete;

	FImGuiModuleManager(FImGuiModuleManager&&) = delete;
	FImGuiModuleManager& operator=(FImGuiModuleManager&&) = delete;

	void OnContextProxyCreated(int32 ContextIndex, FImGuiContextProxy& ContextProxy);

	FImGuiContextManager ContextManager;
};