#include "ImGuiModule.h"
#include "ExampleActor.h"
#include "../SML/mod/hooking.h"
#include "FGGameMode.h"
#include <fstream>

void FImGuiModule::StartupModule() {
	SUBSCRIBE_METHOD("?InitGameState@AFGGameMode@@UEAAXXZ", AFGGameMode::InitGameState, [](auto& scope, AFGGameMode* gameMode) {
		AExampleActor* actor = gameMode->GetWorld()->SpawnActor<AExampleActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		actor->DoStuff();
	});
}

IMPLEMENT_GAME_MODULE(FImGuiModule, ImGui);
