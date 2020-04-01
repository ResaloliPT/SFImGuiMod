// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#include "ImGuiPrivatePCH.h"

#include "ImGuiModuleManager1.h"

#include "ImGuiInteroperability.h"
#include "Utilities/WorldContextIndex.h"

#include <Modules/ModuleManager.h>

#include <imgui.h>


// High enough z-order guarantees that ImGui output is rendered on top of the game UI.
constexpr int32 IMGUI_WIDGET_Z_ORDER = 10000;


FImGuiModuleManager1::FImGuiModuleManager1()
	: Commands(Properties)
	, Settings(Properties, Commands)
	, ImGuiDemo(Properties)
{
	// Register in context manager to get information whenever a new context proxy is created.
	ContextManager.OnContextProxyCreated().AddRaw(this, &FImGuiModuleManager1::OnContextProxyCreated);

	// Typically we will use viewport created events to add widget to new game viewports.
	ViewportCreatedHandle = UGameViewportClient::OnViewportCreated().AddRaw(this, &FImGuiModuleManager1::OnViewportCreated);

	// Try to register tick delegate (it may fail if Slate application isn't yet ready).
	RegisterTick();

	// If we failed to register, create an initializer that will do it later.
	if (!IsTickRegistered())
	{
		CreateTickInitializer();
	}

	// We need to add widgets to active game viewports as they won't generate on-created events. This is especially
	// important during hot-reloading.
	AddWidgetsToActiveViewports();
}

FImGuiModuleManager1::~FImGuiModuleManager1()
{
	// We are no longer interested with adding widgets to viewports.
	if (ViewportCreatedHandle.IsValid())
	{
		UGameViewportClient::OnViewportCreated().Remove(ViewportCreatedHandle);
		ViewportCreatedHandle.Reset();
	}

	// Remove still active widgets (important during hot-reloading).
	for (auto& Widget : Widgets)
	{
		auto SharedWidget = Widget.Pin();
		if (SharedWidget.IsValid())
		{
			auto& WidgetGameViewport = SharedWidget->GetGameViewport();
			if (WidgetGameViewport.IsValid())
			{
				WidgetGameViewport->RemoveViewportWidgetContent(SharedWidget.ToSharedRef());
			}
		}
	}

	// Deactivate this manager.
	ReleaseTickInitializer();
	UnregisterTick();
}

void FImGuiModuleManager1::LoadTextures()
{
	checkf(FSlateApplication::IsInitialized(), TEXT("Slate should be initialized before we can create textures."));

	if (!bTexturesLoaded)
	{
		bTexturesLoaded = true;

		TextureManager.InitializeErrorTexture(FColor::Magenta);

		// Create an empty texture at index 0. We will use it for ImGui outputs with null texture id.
		TextureManager.CreatePlainTexture(FName{ "ImGuiModule_Plain" }, 2, 2, FColor::White);

		// Create a font atlas texture.
		ImFontAtlas& Fonts = ContextManager.GetFontAtlas();

		unsigned char* Pixels;
		int Width, Height, Bpp;
		Fonts.GetTexDataAsRGBA32(&Pixels, &Width, &Height, &Bpp);

		TextureIndex FontsTexureIndex = TextureManager.CreateTexture(FName{ "ImGuiModule_FontAtlas" }, Width, Height, Bpp, Pixels);

		// Set font texture index in ImGui.
		Fonts.TexID = ImGuiInterops::ToImTextureID(FontsTexureIndex);
	}
}

void FImGuiModuleManager1::RegisterTick()
{
	// Slate Post-Tick is a good moment to end and advance ImGui frame as it minimises a tearing.
	if (!TickDelegateHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		TickDelegateHandle = FSlateApplication::Get().OnPostTick().AddRaw(this, &FImGuiModuleManager1::Tick);
	}
}

void FImGuiModuleManager1::UnregisterTick()
{
	if (TickDelegateHandle.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnPostTick().Remove(TickDelegateHandle);
		}
		TickDelegateHandle.Reset();
	}
}

void FImGuiModuleManager1::CreateTickInitializer()
{
	if (!TickInitializerHandle.IsValid())
	{
		// Try to register tick delegate until we finally succeed.

		TickInitializerHandle = FModuleManager::Get().OnModulesChanged().AddLambda([this](FName Name, EModuleChangeReason Reason)
		{
			if (Reason == EModuleChangeReason::ModuleLoaded)
			{
				RegisterTick();
			}

			if (IsTickRegistered())
			{
				ReleaseTickInitializer();
			}
		});
	}
}

void FImGuiModuleManager1::ReleaseTickInitializer()
{
	if (TickInitializerHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(TickInitializerHandle);
		TickInitializerHandle.Reset();
	}
}

void FImGuiModuleManager1::Tick(float DeltaSeconds)
{
	if (IsInGameThread())
	{
		// Update context manager to advance all ImGui contexts to the next frame.
		ContextManager.Tick(DeltaSeconds);

		// Inform that we finished updating ImGui, so other subsystems can react.
		PostImGuiUpdateEvent.Broadcast();
	}
}

void FImGuiModuleManager1::OnViewportCreated()
{
	checkf(FSlateApplication::IsInitialized(), TEXT("We expect Slate to be initialized when game viewport is created."));

	// Create widget to viewport responsible for this event.
	AddWidgetToViewport(GEngine->GameViewport);
}

void FImGuiModuleManager1::AddWidgetToViewport(UGameViewportClient* GameViewport)
{
	checkf(GameViewport, TEXT("Null game viewport."));
	checkf(FSlateApplication::IsInitialized(), TEXT("Slate should be initialized before we can add widget to game viewports."));

	// Make sure that we have a context for this viewport's world and get its index.
	int32 ContextIndex;
	auto& ContextProxy = ContextManager.GetWorldContextProxy(*GameViewport->GetWorld(), ContextIndex);

	// Make sure that textures are loaded before the first Slate widget is created.
	LoadTextures();

	// Create and initialize the widget.
	TSharedPtr<SImGuiLayout> SharedWidget;
	SAssignNew(SharedWidget, SImGuiLayout).ModuleManager(this).GameViewport(GameViewport).ContextIndex(ContextIndex);

	GameViewport->AddViewportWidgetContent(SharedWidget.ToSharedRef(), IMGUI_WIDGET_Z_ORDER);

	// We transfer widget ownerships to viewports but we keep weak references in case we need to manually detach active
	// widgets during module shutdown (important during hot-reloading).
	if (TWeakPtr<SImGuiLayout>* Slot = Widgets.FindByPredicate([](auto& Widget) { return !Widget.IsValid(); }))
	{
		*Slot = SharedWidget;
	}
	else
	{
		Widgets.Emplace(SharedWidget);
	}
}

void FImGuiModuleManager1::AddWidgetsToActiveViewports()
{
	if (FSlateApplication::IsInitialized() && GEngine)
	{
		// Loop as long as we have a valid viewport or until we detect a cycle.
		UGameViewportClient* GameViewport = GEngine->GameViewport;
		while (GameViewport)
		{
			AddWidgetToViewport(GameViewport);

			GameViewport = GEngine->GetNextPIEViewport(GameViewport);
			if (GameViewport == GEngine->GameViewport)
			{
				break;
			}
		}
	}
}

void FImGuiModuleManager1::OnContextProxyCreated(int32 ContextIndex, FImGuiContextProxy& ContextProxy)
{
	ContextProxy.OnDraw().AddLambda([this, ContextIndex]() { ImGuiDemo.DrawControls(ContextIndex); });
}
