#include "ImGuiPrivatePCH.h"

#include "ImGuiModuleManager1.h"
#include "ImGuiModuleManager.h"

#include "ImGuiDelegatesContainer.h"
#include "ImGuiTextureHandle.h"
#include "TextureManager.h"
#include "Utilities/WorldContext.h"
#include "Utilities/WorldContextIndex.h"
#include "ImGuiModule.h"
#include "ImGuiExampleActor.h"
#include "SML/mod/hooking.h"
#include "SML/util/Logging.h"
#include "FGGameMode.h"
#include <fstream>
#include "stdio.h"
#include "ImGuiLogger.h"

#include <Interfaces/IPluginManager.h>

#define IMGUI_REDIRECT_OBSOLETE_DELEGATES 1

#define LOCTEXT_NAMESPACE "FImGuiModule"

using namespace std;
using namespace ImGui;

struct EDelegateCategory
{
	enum
	{
		// Default per-context draw events.
		Default,

		// Multi-context draw event defined in context manager.
		MultiContext
	};
};

static FImGuiModuleManager1* ImGuiModuleManager1 = nullptr;

static FImGuiModuleManager* ImGuiModuleManager = nullptr;

#if IMGUI_WITH_OBSOLETE_DELEGATES

FImGuiDelegateHandle FImGuiModule::AddWorldImGuiDelegate(const FImGuiDelegate& Delegate)
{
#if IMGUI_REDIRECT_OBSOLETE_DELEGATES
	const int32 ContextIndex = Utilities::GetWorldContextIndex((UWorld*)GWorld);
	return { FImGuiDelegatesContainer::Get().OnWorldDebug(ContextIndex).Add(Delegate), EDelegateCategory::Default, ContextIndex };
#else
	checkf(ImGuiModuleManager, TEXT("Null pointer to internal module implementation. Is module available?"));
	const int32 Index = Utilities::STANDALONE_GAME_CONTEXT_INDEX;
	FImGuiContextProxy& Proxy = ImGuiModuleManager->GetContextManager().GetWorldContextProxy();

	return{ Proxy.OnDraw().Add(Delegate), EDelegateCategory::Default, Index };
#endif // IMGUI_REDIRECT_OBSOLETE_DELEGATES
}

FImGuiDelegateHandle FImGuiModule::AddMultiContextImGuiDelegate(const FImGuiDelegate& Delegate)
{
#if IMGUI_REDIRECT_OBSOLETE_DELEGATES
	return { FImGuiDelegatesContainer::Get().OnMultiContextDebug().Add(Delegate), EDelegateCategory::MultiContext };
#else
	checkf(ImGuiModuleManager, TEXT("Null pointer to internal module implementation. Is module available?"));

	return { ImGuiModuleManager->GetContextManager().OnDrawMultiContext().Add(Delegate), EDelegateCategory::MultiContext };
#endif
}

void FImGuiModule::RemoveImGuiDelegate(const FImGuiDelegateHandle& Handle)
{
#if IMGUI_REDIRECT_OBSOLETE_DELEGATES
	if (Handle.Category == EDelegateCategory::MultiContext)
	{
		FImGuiDelegatesContainer::Get().OnMultiContextDebug().Remove(Handle.Handle);
	}
	else
	{
		FImGuiDelegatesContainer::Get().OnWorldDebug(Handle.Index).Remove(Handle.Handle);
	}
#else
	if (ImGuiModuleManager)
	{
		if (Handle.Category == EDelegateCategory::MultiContext)
		{
			ImGuiModuleManager->GetContextManager().OnDrawMultiContext().Remove(Handle.Handle);
		}
		else if (auto* Proxy = ImGuiModuleManager->GetContextManager().GetContextProxy(Handle.Index))
		{
			Proxy->OnDraw().Remove(Handle.Handle);
		}
	}
#endif
}

#endif // IMGUI_WITH_OBSOLETE_DELEGATES

FImGuiTextureHandle FImGuiModule::FindTextureHandle(const FName& Name)
{
	const TextureIndex Index = ImGuiModuleManager1->GetTextureManager().FindTextureIndex(Name);
	return (Index != INDEX_NONE) ? FImGuiTextureHandle{ Name, ImGuiInterops::ToImTextureID(Index) } : FImGuiTextureHandle{};
}

FImGuiTextureHandle FImGuiModule::RegisterTexture(const FName& Name, class UTexture2D* Texture, bool bMakeUnique)
{
	const TextureIndex Index = ImGuiModuleManager1->GetTextureManager().CreateTextureResources(Name, Texture, bMakeUnique);
	return FImGuiTextureHandle{ Name, ImGuiInterops::ToImTextureID(Index) };
}

void FImGuiModule::ReleaseTexture(const FImGuiTextureHandle& Handle)
{
	if (Handle.IsValid())
	{
		ImGuiModuleManager1->GetTextureManager().ReleaseTextureResources(ImGuiInterops::ToTextureIndex(Handle.GetTextureId()));
	}
}

void FImGuiModule::StartupModule()
{
	// Create managers that implements module logic.

	checkf(!ImGuiModuleManager, TEXT("Instance of the ImGui Module Manager already exists. Instance should be created only during module startup."));
	//ImGuiModuleManager = new FImGuiModuleManager();

	SUBSCRIBE_METHOD("?InitGameState@AFGGameMode@@UEAAXXZ", AFGGameMode::InitGameState, [](auto& scope, AFGGameMode* gameMode) {
		AImGuiExampleActor* actor = gameMode->GetWorld()->SpawnActor<AImGuiExampleActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		actor->DoStuff();
	});

	ImGui::ImGuiLogger::LogInfo("Loaded!");
}

void FImGuiModule::ShutdownModule()
{

	checkf(ImGuiModuleManager, TEXT("Null ImGui Module Manager. Module manager instance should be deleted during module shutdown."));
	delete ImGuiModuleManager;
	ImGuiModuleManager = nullptr;
}

FImGuiModuleProperties& FImGuiModule::GetProperties()
{
	return ImGuiModuleManager1->GetProperties();
}

const FImGuiModuleProperties& FImGuiModule::GetProperties() const
{
	return ImGuiModuleManager1->GetProperties();
}

bool FImGuiModule::IsInputMode() const
{
	return ImGuiModuleManager1 && ImGuiModuleManager1->GetProperties().IsInputEnabled();
}

void FImGuiModule::SetInputMode(bool bEnabled)
{
	if (ImGuiModuleManager1)
	{
		ImGuiModuleManager1->GetProperties().SetInputEnabled(bEnabled);
	}
}

void FImGuiModule::ToggleInputMode()
{
	if (ImGuiModuleManager)
	{
		ImGuiModuleManager1->GetProperties().ToggleInput();
	}
}

bool FImGuiModule::IsShowingDemo() const
{
	return ImGuiModuleManager1 && ImGuiModuleManager1->GetProperties().ShowDemo();
}

void FImGuiModule::SetShowDemo(bool bShow)
{
	if (ImGuiModuleManager1)
	{
		ImGuiModuleManager1->GetProperties().SetShowDemo(bShow);
	}
}

void FImGuiModule::ToggleShowDemo()
{
	if (ImGuiModuleManager1)
	{
		ImGuiModuleManager1->GetProperties().ToggleDemo();
	}
}


//----------------------------------------------------------------------------------------------------
// Runtime loader
//----------------------------------------------------------------------------------------------------
//TODO: Check if needed
#if !WITH_EDITOR && RUNTIME_LOADER_ENABLED

class FImGuiModuleLoader
{
	FImGuiModuleLoader()
	{
		if (!Load())
		{
			FModuleManager::Get().OnModulesChanged().AddRaw(this, &FImGuiModuleLoader::LoadAndRelease);
		}
	}

	// For different engine versions.
	static FORCEINLINE bool IsValid(const TSharedPtr<IModuleInterface>& Ptr) { return Ptr.IsValid(); }
	static FORCEINLINE bool IsValid(const IModuleInterface* const Ptr) { return Ptr != nullptr; }

	bool Load()
	{
		return true;// IsValid(FModuleManager::Get().LoadModule(ModuleName));
	}

	void LoadAndRelease(FName Name, EModuleChangeReason Reason)
	{
		// Avoid handling own load event.
		if (Name != ModuleName)
		{
			// Try loading until success and then release.
			if (Load())
			{
				FModuleManager::Get().OnModulesChanged().RemoveAll(this);
			}
		}
	}

	static FName ModuleName;

	static FImGuiModuleLoader Instance;
};

FName FImGuiModuleLoader::ModuleName = "ImGui";

// In monolithic builds this will start loading process.
FImGuiModuleLoader FImGuiModuleLoader::Instance;

#endif // !WITH_EDITOR && RUNTIME_LOADER_ENABLED


//----------------------------------------------------------------------------------------------------
// Partial implementations of other classes that needs access to ImGuiModuleManager
//----------------------------------------------------------------------------------------------------

bool FImGuiTextureHandle::HasValidEntry() const
{
	const TextureIndex Index = ImGuiInterops::ToTextureIndex(TextureId);
	return Index != INDEX_NONE && ImGuiModuleManager1 && ImGuiModuleManager1->GetTextureManager().GetTextureName(Index) == Name;
}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FImGuiModule, ImGui)
