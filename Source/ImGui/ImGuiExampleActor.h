#pragma once

#include "CoreMinimal.h"
#include "CoreUObject.h"
#include "GameFramework/Actor.h"
#include "ImGuiExampleActor.generated.h"

UCLASS()
class IMGUI_API AImGuiExampleActor : public AActor {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void DoStuff();
};