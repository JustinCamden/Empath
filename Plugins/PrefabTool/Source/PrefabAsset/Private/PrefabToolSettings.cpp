// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabToolSettings.h"

//////////////////////////////////////////////////////////////////////////
// UPrefabToolEditorSettings

UPrefabToolSettings::UPrefabToolSettings()
	// General
	: bReplaceActorsWithCreatedPrefab(true)
	, bNestedPrefabSupport(true)
	, bUpdateAllPrefabActorsWhenOpenMap(true)
	, bCheckPrefabChangeBeforeUpdateAllPrefabActorsWhenOpenMap(true)
	, bUpdateAllPrefabActorsWhenApply(true)
	, bEnableApplyFromDisconnectedPrefabActor(false)

	// Visualizer
	, bEnablePrefabComponentVisualizer(true)

	// Experimental
	, bLockPrefabSelectionByDefault(true)
	, bDisableLockPrefabSelectionFeature(false)
	, bForceApplyPerInstanceVertexColor(false)
	, bBSPBrushSupport(false)

	// Generate Blueprint
	, bSupportGenerateBlueprint(true)
	, bHarvestComponentsWhenGeneratingBlueprint(true)
	, bForceMobilityToMovableWhenGeneratingBlueprint(true)
	, bBakeBlueprintWhenGeneratingBlueprint(false)

	//, bCreateGroupWhenAddPrefabToLevel(false)
	, bEnablePrefabTextEditor(false)
	, bDebugMode(false)
{
	if (!IsRunningCommandlet())
	{
		//ConnectedPrefabIconTextureName = FSoftObjectPath("/PrefabTool/Icons/S_PrefabConnected.S_PrefabConnected");
	}
}
