#include "ExampleActor.h"

#include "ImGuiLogger.h"
#include "SML\util\Logging.h"

void AExampleActor::DoStuff() {
	ImGui::ImGuiLogger::LogInfo("ExampleActor");
}