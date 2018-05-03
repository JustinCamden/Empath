// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabAssetEditCommands.h"

#define LOCTEXT_NAMESPACE "PrefabAssetEditCommands"

PRAGMA_DISABLE_OPTIMIZATION
void FPrefabAssetEditCommands::RegisterCommands()
{
	UI_COMMAND(AttachToPrefabActor, "Attach to Prefab Actor", "Attach to Prefab Actor", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::T));
	UI_COMMAND(ToggleHidePrefabActor, "Toggle Parent Prefab Actor Hide Status", "Toggle Parent Prefab Actors Hide Status", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::G));
	UI_COMMAND(TogglePrefabActorLockSelection, "Toggle Parent Prefab Actor Hide Status", "Toggle Parent Prefab Actors Lock Selection", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::L));
	UI_COMMAND(ApplyAndLockPrefabActor, "Apply And Lock Prefab Actor", "Apply And Lock Prefab Actor Selection", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::A));
	UI_COMMAND(SelectParentPrefabActor, "Select Parent Prefab Actor", "Select Parent Prefab Actor", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::S));
	UI_COMMAND(CollapseToParentPrefabActor, "Collapse to Parent Prefab Actor", "Collapse Scene Outliner to Parent Prefab Actor", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::E));
	UI_COMMAND(SelectChildrenOfSelectedPrefabActor, "Select Children of Selected Prefab Actor", "Select Children of Selected Prefab Actor", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SelectChildrenOfParentPrefabActor, "Select Children of Parent Prefab Actor", "Select Children of Parent Prefab Actor", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(SetPrefabPivotToCurrentWidgetLocation, "Set Prefab Pivot to Current Widget Location", "Set Prefab Pivot to Current Widget Location", EUserInterfaceActionType::Button, FInputChord());
}
PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
