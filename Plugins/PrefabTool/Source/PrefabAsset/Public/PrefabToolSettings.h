// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "PrefabToolSettings.generated.h"

/**
 * Implements the settings for prefab tool.
 */
UCLASS(config=Editor, defaultconfig)
class PREFABASSET_API UPrefabToolSettings : public UObject
{
	GENERATED_BODY()

public:

	// General

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bReplaceActorsWithCreatedPrefab;

	UPROPERTY(config, EditAnywhere, Category=General)
	bool bNestedPrefabSupport;

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bUpdateAllPrefabActorsWhenOpenMap;

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bCheckPrefabChangeBeforeUpdateAllPrefabActorsWhenOpenMap;

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bUpdateAllPrefabActorsWhenApply;

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bEnableApplyFromDisconnectedPrefabActor;

	// Visualizer

	UPROPERTY(config, EditAnywhere, Category = Visualizer)
	bool bEnablePrefabComponentVisualizer;

	// Experimental

	UPROPERTY(config, EditAnywhere, Category = Experimental)
	bool bLockPrefabSelectionByDefault;

	UPROPERTY(config, EditAnywhere, Category = Experimental)
	bool bDisableLockPrefabSelectionFeature;

	UPROPERTY(config, EditAnywhere, Category = Experimental)
	bool bForceApplyPerInstanceVertexColor; 
	
	//	UPROPERTY(config, EditAnywhere, Category = Experimental)
	bool bBSPBrushSupport;

	// Generate Blueprint

	UPROPERTY(config, EditAnywhere, Category = "Generate Blueprint")
	bool bSupportGenerateBlueprint;

	UPROPERTY(config, EditAnywhere, DisplayName = "Harvest Components First", Category = "Generate Blueprint")
	bool bHarvestComponentsWhenGeneratingBlueprint;

	UPROPERTY(config, EditAnywhere, DisplayName = "Force Mobility to Movable", Category = "Generate Blueprint")
	bool bForceMobilityToMovableWhenGeneratingBlueprint;

	//UPROPERTY(config, EditAnywhere, Category = Experimental)
	bool bBakeBlueprintWhenGeneratingBlueprint;

	UPROPERTY(config, EditAnywhere, DisplayName = "Ignore Actor Classes", Category = "Generate Blueprint")
	TArray<TSubclassOf<AActor>> IgnoreActorClassesWhenGeneratingBlueprint;

	UPROPERTY(config, EditAnywhere, DisplayName = "Ignore Actor Tags", Category = "Generate Blueprint")
	TArray<FName> IgnoreActorTagsWhenGeneratingBlueprint;

// 	/** Should create new group actor when adding prefab to level. */
// 	UPROPERTY(config, EditAnywhere, Category= PrefabToolSettings)
// 	bool bCreateGroupWhenAddPrefabToLevel;

/** Should enable debug mode. */
	UPROPERTY(config, EditAnywhere, Category = Advanced)
	bool bEnablePrefabTextEditor;

	/** Should enable debug mode. */
	UPROPERTY(config, EditAnywhere, Category = Debug)
	bool bDebugMode;

public:
	UPrefabToolSettings();

	FORCEINLINE bool ShouldEnablePrefabComponentVisualizer() const { return bEnablePrefabComponentVisualizer; }

	FORCEINLINE bool ShouldLockPrefabSelectionByDefault() const { return bLockPrefabSelectionByDefault; }

	FORCEINLINE bool ShouldDisableLockPrefabSelectionFeature() const { return bDisableLockPrefabSelectionFeature; }

	FORCEINLINE bool ShouldReplaceActorsWithCreatedPrefab() const { return bReplaceActorsWithCreatedPrefab; }

	FORCEINLINE bool ShouldEnableNestedPrefabSupport() const { return bNestedPrefabSupport; }

	FORCEINLINE bool ShouldUpdateAllPrefabActorsWhenOpenMap() const { return bUpdateAllPrefabActorsWhenOpenMap; }

	FORCEINLINE bool ShouldCheckPrefabChangeBeforeUpdateAllPrefabActorsWhenOpenMap() const { return bCheckPrefabChangeBeforeUpdateAllPrefabActorsWhenOpenMap; }

	FORCEINLINE bool ShouldUpdateAllPrefabActorsWhenApply() const { return bUpdateAllPrefabActorsWhenApply; }

	FORCEINLINE bool ShouldEnablebBlueprintGenerationSupport() const { return bSupportGenerateBlueprint; }

	FORCEINLINE bool ShouldForceApplyPerInstanceVertexColor() const { return bForceApplyPerInstanceVertexColor; }

	FORCEINLINE bool ShouldEnableBSPBrushSupport() const { return bBSPBrushSupport; }

//	bool ShouldCreateGroupWhenAddPrefabToLevel() const { return bCreateGroupWhenAddPrefabToLevel; }

	FORCEINLINE bool ShouldEnablePrefabTextEditor() const { return bEnablePrefabTextEditor; }

	FORCEINLINE bool IsDebugMode() const { return bDebugMode; }
};
