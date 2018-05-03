// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "ThumbnailHelpers.h"
#include "PrefabAsset.h"

/**
* Preview scene for rendering prefab thumbnail
*/
class FPrefabThumbnailScene : public FThumbnailPreviewScene
{
public:

	FPrefabThumbnailScene();

	static bool IsValidComponentForVisualization(UActorComponent* Component);

	void SetPrefab(class UPrefabAsset* Prefab);
	void PrefabChanged(class UPrefabAsset* Prefab);

protected:

	//~ Begin FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;
	//~ End FThumbnailPreviewScene implementation

	virtual USceneThumbnailInfo* GetSceneThumbnailInfo(const float TargetDistance) const;

	void SpawnPreviewActor();

	FBoxSphereBounds GetPreviewActorBounds(const TArray<AActor*> InPreviewActors) const;

	FVector GetPreviewActorCenter(const TArray<AActor*> InPreviewActors) const;

private:

	void ClearStaleActors();

private:

	int32 NumStartingActors;

	//TWeakObjectPtr<class AActor> PreviewActor;
	TWeakObjectPtr<class UPrefabAsset> CurrentPrefab;
	FText CachedPrefabContent;

	FBoxSphereBounds PreviewActorsBound;
	FVector PreviewActorsCenter;
};

template <int32 MaxNumScenes>
class FPrefabInstanceThumbnailScene
{
public:
	FPrefabInstanceThumbnailScene();

	TSharedPtr<FPrefabThumbnailScene> FindThumbnailScene(const FString& InPrefabPath) const;
	TSharedRef<FPrefabThumbnailScene> EnsureThumbnailScene(const FString& InPrefabPath);
	void Clear();

private:
	TMap<FString, TSharedPtr<FPrefabThumbnailScene>> InstancedThumbnailScenes;
};

typedef FPrefabInstanceThumbnailScene<400> FPrefabInstanceThumbnailSceneType;

