// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#include "ImGuiPrivatePCH.h"

#include "ImGuiContextManager.h"

#include "ImGuiDelegatesContainer.h"
#include "ImGuiImplementation.h"
#include "Utilities/ScopeGuards.h"
#include "Utilities/WorldContext.h"
#include "Utilities/WorldContextIndex.h"

#include <imgui.h>


// TODO: Refactor ImGui Context Manager, to handle different types of worlds.

namespace
{
	FORCEINLINE FString GetWorldContextName()
	{
		return TEXT("Game");
	}

	FORCEINLINE FString GetWorldContextName(const UWorld&)
	{
		return TEXT("Game");
	}

}

FImGuiContextManager::FImGuiContextManager()
{
	unsigned char* Pixels;
	int Width, Height, Bpp;
	FontAtlas.GetTexDataAsRGBA32(&Pixels, &Width, &Height, &Bpp);

	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FImGuiContextManager::OnWorldTickStart);
#if ENGINE_COMPATIBILITY_WITH_WORLD_POST_ACTOR_TICK
	FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FImGuiContextManager::OnWorldPostActorTick);
#endif
}

FImGuiContextManager::~FImGuiContextManager()
{
	// Order matters because contexts can be created during World Tick Start events.
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
#if ENGINE_COMPATIBILITY_WITH_WORLD_POST_ACTOR_TICK
	FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
#endif
}

void FImGuiContextManager::Tick(float DeltaSeconds)
{
	// In editor, worlds can get invalid. We could remove corresponding entries, but that would mean resetting ImGui
	// context every time when PIE session is restarted. Instead we freeze contexts until their worlds are re-created.

	for (auto& Pair : Contexts)
	{
		auto& ContextData = Pair.Value;
		if (ContextData.CanTick())
		{
			ContextData.ContextProxy->Tick(DeltaSeconds);
		}
		else
		{
			// Clear to make sure that we don't store objects registered for world that is no longer valid.
			FImGuiDelegatesContainer::Get().OnWorldDebug(Pair.Key).Clear();
		}
	}
}

#if ENGINE_COMPATIBILITY_LEGACY_WORLD_ACTOR_TICK
void FImGuiContextManager::OnWorldTickStart(ELevelTick TickType, float DeltaSeconds)
{
	OnWorldTickStart(GWorld, TickType, DeltaSeconds);
}
#endif

void FImGuiContextManager::OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE
		|| World->WorldType == EWorldType::Editor))
	{
		FImGuiContextProxy& ContextProxy = GetWorldContextProxy(*World);

		// Set as current, so we have right context ready when updating world objects.
		ContextProxy.SetAsCurrent();

		ContextProxy.DrawEarlyDebug();
#if !ENGINE_COMPATIBILITY_WITH_WORLD_POST_ACTOR_TICK
		ContextProxy.DrawDebug();
#endif
	}
}

#if ENGINE_COMPATIBILITY_WITH_WORLD_POST_ACTOR_TICK
void FImGuiContextManager::OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE
		|| World->WorldType == EWorldType::Editor))
	{
		GetWorldContextProxy(*World).DrawDebug();
	}
}
#endif // ENGINE_COMPATIBILITY_WITH_WORLD_POST_ACTOR_TICK

FImGuiContextManager::FContextData& FImGuiContextManager::GetStandaloneWorldContextData()
{
	FContextData* Data = Contexts.Find(Utilities::STANDALONE_GAME_CONTEXT_INDEX);

	if (UNLIKELY(!Data))
	{
		Data = &Contexts.Emplace(Utilities::STANDALONE_GAME_CONTEXT_INDEX, FContextData{ GetWorldContextName(), Utilities::STANDALONE_GAME_CONTEXT_INDEX, DrawMultiContextEvent, FontAtlas });
		ContextProxyCreatedEvent.Broadcast(Utilities::STANDALONE_GAME_CONTEXT_INDEX, *Data->ContextProxy);
	}

	return *Data;
}

FImGuiContextManager::FContextData& FImGuiContextManager::GetWorldContextData(const UWorld& World, int32* OutIndex)
{
	using namespace Utilities;

	const FWorldContext* WorldContext = GetWorldContext(World);
	const int32 Index = GetWorldContextIndex(*WorldContext);

	checkf(Index != Utilities::INVALID_CONTEXT_INDEX, TEXT("Couldn't find context index for world %s: WorldType = %d"),
		*World.GetName(), static_cast<int32>(World.WorldType));

	FContextData* Data = Contexts.Find(Index);

	if (UNLIKELY(!Data))
	{
		Data = &Contexts.Emplace(Index, FContextData{ GetWorldContextName(World), Index, DrawMultiContextEvent, FontAtlas });
		ContextProxyCreatedEvent.Broadcast(Index, *Data->ContextProxy);
	}

	if (OutIndex)
	{
		*OutIndex = Index;
	}
	return *Data;
}
