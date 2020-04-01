// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#pragma once

#include <Widgets/SCompoundWidget.h>
#include "Slate.h"

class FImGuiModuleManager1;
class UGameViewportClient;

// Layout preset for ImGui Widget.
class SImGuiLayout : public SCompoundWidget
{
	typedef SCompoundWidget Super;

public:

	SLATE_BEGIN_ARGS(SImGuiLayout)
	{}
	SLATE_ARGUMENT(FImGuiModuleManager1*, ModuleManager)
	SLATE_ARGUMENT(UGameViewportClient*, GameViewport)
	SLATE_ARGUMENT(int32, ContextIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	const TWeakObjectPtr<UGameViewportClient>& GetGameViewport() const { return GameViewport; }

private:

	TWeakObjectPtr<UGameViewportClient> GameViewport;
};
