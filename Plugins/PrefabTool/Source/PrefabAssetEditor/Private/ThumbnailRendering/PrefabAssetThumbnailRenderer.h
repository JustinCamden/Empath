// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "PrefabThumbnailScene.h"
#include "PrefabAssetThumbnailRenderer.generated.h"

/**
* Asset thumbnail renderer for Prefab
*/
UCLASS(config=Editor,MinimalAPI)
class UPrefabAssetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation

	void PrefabChanged(class UPrefabAsset* Prefab);

private:
	FPrefabInstanceThumbnailSceneType ThumbnailScenes;
};
