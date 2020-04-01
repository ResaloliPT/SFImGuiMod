// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#include "ImGuiPrivatePCH.h"

#include "ImGuiInputHandler.h"

#include "ImGuiContextProxy.h"
#include "ImGuiInputState.h"
#include "ImGuiModuleManager1.h"
#include "ImGuiModuleSettings.h"

#include <Engine/Console.h>
#include <Input/Events.h>


DEFINE_LOG_CATEGORY(LogImGuiInputHandler);

namespace
{
	FReply ToReply(bool bConsume)
	{
		return bConsume ? FReply::Handled() : FReply::Unhandled();
	}
}

FReply UImGuiInputHandler::OnKeyChar(const struct FCharacterEvent& CharacterEvent)
{
	InputState->AddCharacter(CharacterEvent.GetCharacter());
	return ToReply(!ModuleManager->GetProperties().IsKeyboardInputShared());
}

FReply UImGuiInputHandler::OnKeyDown(const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey().IsGamepadKey())
	{
		bool bConsume = false;
		if (InputState->IsGamepadNavigationEnabled())
		{
			InputState->SetGamepadNavigationKey(KeyEvent, true);
			bConsume = !ModuleManager->GetProperties().IsGamepadInputShared();
		}

		return ToReply(bConsume);
	}
	else
	{
		// Ignore console events, so we don't block it from opening.
		if (IsConsoleEvent(KeyEvent))
		{
			return ToReply(false);
		}

		const bool bConsume = !ModuleManager->GetProperties().IsKeyboardInputShared();

		// With shared input we can leave command bindings for DebugExec to handle, otherwise we need to do it here.
		if (bConsume && IsToggleInputEvent(KeyEvent))
		{
			ModuleManager->GetProperties().ToggleInput();
		}

		InputState->SetKeyDown(KeyEvent, true);
		CopyModifierKeys(KeyEvent);

		return ToReply(bConsume);
	}
}

FReply UImGuiInputHandler::OnKeyUp(const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey().IsGamepadKey())
	{
		bool bConsume = false;
		if (InputState->IsGamepadNavigationEnabled())
		{
			InputState->SetGamepadNavigationKey(KeyEvent, false);
			bConsume = !ModuleManager->GetProperties().IsGamepadInputShared();
		}

		return ToReply(bConsume);
	}
	else
	{
		InputState->SetKeyDown(KeyEvent, false);
		CopyModifierKeys(KeyEvent);

		return ToReply(!ModuleManager->GetProperties().IsKeyboardInputShared());
	}
}

FReply UImGuiInputHandler::OnAnalogValueChanged(const FAnalogInputEvent& AnalogInputEvent)
{
	bool bConsume = false;

	if (AnalogInputEvent.GetKey().IsGamepadKey() && InputState->IsGamepadNavigationEnabled())
	{
		InputState->SetGamepadNavigationAxis(AnalogInputEvent, AnalogInputEvent.GetAnalogValue());
		bConsume = !ModuleManager->GetProperties().IsGamepadInputShared();
	}

	return ToReply(bConsume);
}

FReply UImGuiInputHandler::OnMouseButtonDown(const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent())
	{
		return ToReply(false);
	}

	InputState->SetMouseDown(MouseEvent, true);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnMouseButtonDoubleClick(const FPointerEvent& MouseEvent)
{
	InputState->SetMouseDown(MouseEvent, true);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnMouseButtonUp(const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent())
	{
		return ToReply(false);
	}

	InputState->SetMouseDown(MouseEvent, false);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnMouseWheel(const FPointerEvent& MouseEvent)
{
	InputState->AddMouseWheelDelta(MouseEvent.GetWheelDelta());
	return ToReply(true);
}

FReply UImGuiInputHandler::OnMouseMove(const FVector2D& MousePosition, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent())
	{
		return ToReply(false);
	}

	return OnMouseMove(MousePosition);
}

FReply UImGuiInputHandler::OnMouseMove(const FVector2D& MousePosition)
{
	InputState->SetMousePosition(MousePosition);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnTouchStarted(const FVector2D& CursorPosition, const FPointerEvent& TouchEvent)
{
	InputState->SetTouchDown(true);
	InputState->SetTouchPosition(CursorPosition);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnTouchMoved(const FVector2D& CursorPosition, const FPointerEvent& TouchEvent)
{
	InputState->SetTouchPosition(CursorPosition);
	return ToReply(true);
}

FReply UImGuiInputHandler::OnTouchEnded(const FVector2D& CursorPosition, const FPointerEvent& TouchEvent)
{
	InputState->SetTouchDown(false);
	return ToReply(true);
}

void UImGuiInputHandler::OnKeyboardInputEnabled()
{
	bKeyboardInputEnabled = true;
}

void UImGuiInputHandler::OnKeyboardInputDisabled()
{
	if (bKeyboardInputEnabled)
	{
		bKeyboardInputEnabled = false;
		InputState->ResetKeyboard();
	}
}

void UImGuiInputHandler::OnGamepadInputEnabled()
{
	bGamepadInputEnabled = true;
}

void UImGuiInputHandler::OnGamepadInputDisabled()
{
	if (bGamepadInputEnabled)
	{
		bGamepadInputEnabled = false;
		InputState->ResetGamepadNavigation();
	}
}

void UImGuiInputHandler::OnMouseInputEnabled()
{
	if (!bMouseInputEnabled)
	{
		bMouseInputEnabled = true;
		UpdateInputStatePointer();
	}
}

void UImGuiInputHandler::OnMouseInputDisabled()
{
	if (bMouseInputEnabled)
	{
		bMouseInputEnabled = false;
		InputState->ResetMouse();
		UpdateInputStatePointer();
	}
}

void UImGuiInputHandler::CopyModifierKeys(const FInputEvent& InputEvent)
{
	InputState->SetControlDown(InputEvent.IsControlDown());
	InputState->SetShiftDown(InputEvent.IsShiftDown());
	InputState->SetAltDown(InputEvent.IsAltDown());
}

bool UImGuiInputHandler::IsConsoleEvent(const FKeyEvent& KeyEvent) const
{
	// Checking modifiers is based on console implementation.
	const bool bModifierDown = KeyEvent.IsControlDown() || KeyEvent.IsShiftDown() || KeyEvent.IsAltDown() || KeyEvent.IsCommandDown();
	return !bModifierDown && GetDefault<UInputSettings>()->ConsoleKeys.Contains(KeyEvent.GetKey());
}


namespace
{
	bool IsMatching(ECheckBoxState CheckBoxState, bool bValue)
	{
		return (CheckBoxState == ECheckBoxState::Undetermined) || ((CheckBoxState == ECheckBoxState::Checked) == bValue);
	}

	bool IsMatchingEvent(const FKeyEvent& KeyEvent, const FImGuiKeyInfo& KeyInfo)
	{
		return (KeyInfo.Key == KeyEvent.GetKey())
			&& IsMatching(KeyInfo.Shift, KeyEvent.IsShiftDown())
			&& IsMatching(KeyInfo.Ctrl, KeyEvent.IsControlDown())
			&& IsMatching(KeyInfo.Alt, KeyEvent.IsAltDown())
			&& IsMatching(KeyInfo.Cmd, KeyEvent.IsCommandDown());
	}
}

bool UImGuiInputHandler::IsToggleInputEvent(const FKeyEvent& KeyEvent) const
{
	return false;//IsMatchingEvent(KeyEvent, ModuleManager->GetSettings().GetToggleInputKey());
}

bool UImGuiInputHandler::HasImGuiActiveItem() const
{
	FImGuiContextProxy* ContextProxy = ModuleManager->GetContextManager().GetContextProxy(ContextIndex);
	return ContextProxy && ContextProxy->HasActiveItem();
}

void UImGuiInputHandler::UpdateInputStatePointer()
{
	//InputState->SetMousePointer(bMouseInputEnabled && ModuleManager->GetSettings().UseSoftwareCursor());
}

void UImGuiInputHandler::OnSoftwareCursorChanged(bool)
{
	UpdateInputStatePointer();
}

void UImGuiInputHandler::OnPostImGuiUpdate()
{
	InputState->ClearUpdateState();

	// TODO Replace with delegates after adding property change events.
	InputState->SetKeyboardNavigationEnabled(ModuleManager->GetProperties().IsKeyboardNavigationEnabled());
	InputState->SetGamepadNavigationEnabled(ModuleManager->GetProperties().IsGamepadNavigationEnabled());

	const auto& PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	InputState->SetGamepad(PlatformApplication.IsValid() && PlatformApplication->IsGamepadAttached());
}

void UImGuiInputHandler::Initialize(FImGuiModuleManager1* InModuleManager, UGameViewportClient* InGameViewport, int32 InContextIndex)
{
	ModuleManager = InModuleManager;
	GameViewport = InGameViewport;
	ContextIndex = InContextIndex;

	auto* ContextProxy = ModuleManager->GetContextManager().GetContextProxy(ContextIndex);
	checkf(ContextProxy, TEXT("Missing context during initialization of input handler: ContextIndex = %d"), ContextIndex);
	InputState = &ContextProxy->GetInputState();

	// Register to get post-update notifications, so we can clean frame updates.
	ModuleManager->OnPostImGuiUpdate().AddUObject(this, &UImGuiInputHandler::OnPostImGuiUpdate);

	/*auto& Settings = ModuleManager->GetSettings();
	if (!Settings.OnUseSoftwareCursorChanged.IsBoundToObject(this))
	{
		Settings.OnUseSoftwareCursorChanged.AddUObject(this, &UImGuiInputHandler::OnSoftwareCursorChanged);
	}*/
}

void UImGuiInputHandler::BeginDestroy()
{
	Super::BeginDestroy();

	if (ModuleManager)
	{
		//ModuleManager->GetSettings().OnUseSoftwareCursorChanged.RemoveAll(this);
	}
}

