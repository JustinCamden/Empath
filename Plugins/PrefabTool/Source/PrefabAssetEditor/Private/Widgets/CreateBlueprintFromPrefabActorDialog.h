// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APrefabActor;

//////////////////////////////////////////////////////////////////////////
// FCreateBlueprintFromPrefabActorDialog

class FCreateBlueprintFromPrefabActorDialog
{
public:
	/** 
	 * Static function to access constructing this window.
	 *
	 * @param bInUpdate			true if update the blueprint
	 * @param InPrefabActor		Prefab Actor to create blueprint from
	 */
	static void OpenDialog(APrefabActor* InPrefabActor);

private:
	/** 
	 * Will create the blueprint
	 *
	 * @param InAssetPath		Path of the asset to create
	 * @param bInUpdate			true if update the blueprint.
	 */
	static void OnCreateBlueprint(const FString& InAssetPath);

private:
	static TWeakObjectPtr<APrefabActor> SourceActor;
};
