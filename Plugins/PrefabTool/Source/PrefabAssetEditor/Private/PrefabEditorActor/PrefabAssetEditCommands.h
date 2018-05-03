// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "PrefabAssetEditorStyle.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "PrefabAssetEditCommands"

/**
* Edit Commands for Prefab Actor
*/
class FPrefabAssetEditCommands : public TCommands<FPrefabAssetEditCommands>
{

public:
	FPrefabAssetEditCommands() : TCommands<FPrefabAssetEditCommands>
		(TEXT("PrefabAssetEditor"), LOCTEXT("PrefabAssetEditor", "Prefab Asset Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo> AttachToPrefabActor;
	TSharedPtr<FUICommandInfo> ToggleHidePrefabActor;
	TSharedPtr<FUICommandInfo> TogglePrefabActorLockSelection;
	TSharedPtr<FUICommandInfo> ApplyAndLockPrefabActor;
	TSharedPtr<FUICommandInfo> SelectParentPrefabActor;
	TSharedPtr<FUICommandInfo> CollapseToParentPrefabActor;
	TSharedPtr<FUICommandInfo> SelectChildrenOfSelectedPrefabActor;
	TSharedPtr<FUICommandInfo> SelectChildrenOfParentPrefabActor;

	TSharedPtr<FUICommandInfo> SetPrefabPivotToCurrentWidgetLocation;

	virtual void RegisterCommands() override;

public:
};

#undef LOCTEXT_NAMESPACE