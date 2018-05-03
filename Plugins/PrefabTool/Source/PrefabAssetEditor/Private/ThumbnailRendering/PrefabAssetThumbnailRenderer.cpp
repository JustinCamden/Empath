// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabAssetThumbnailRenderer.h"
#include "PrefabToolEditorHelpers.h"
#include "PrefabToolHelpers.h"

#include "EngineModule.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"


//////////////////////////////////////////////////////////////////////////////////////
// FPrefabInstanceThumbnailScene
//

template <int32 MaxNumScenes>
FPrefabInstanceThumbnailScene<MaxNumScenes>::FPrefabInstanceThumbnailScene()
{
	InstancedThumbnailScenes.Reserve(MaxNumScenes);
}

template <int32 MaxNumScenes>
TSharedPtr<FPrefabThumbnailScene> FPrefabInstanceThumbnailScene<MaxNumScenes>::FindThumbnailScene(const FString& InPrefabPath) const
{
	return InstancedThumbnailScenes.FindRef(InPrefabPath);
}

template <int32 MaxNumScenes>
TSharedRef<FPrefabThumbnailScene> FPrefabInstanceThumbnailScene<MaxNumScenes>::EnsureThumbnailScene(const FString& InPrefabPath)
{
	TSharedPtr<FPrefabThumbnailScene> ExistingThumbnailScene = InstancedThumbnailScenes.FindRef(InPrefabPath);
	if (!ExistingThumbnailScene.IsValid())
	{
		if (InstancedThumbnailScenes.Num() >= MaxNumScenes)
		{
			InstancedThumbnailScenes.Reset();
		}

		ExistingThumbnailScene = MakeShareable(new FPrefabThumbnailScene());
		InstancedThumbnailScenes.Add(InPrefabPath, ExistingThumbnailScene);
	}

	return ExistingThumbnailScene.ToSharedRef();
}


template <int32 MaxNumScenes>
void FPrefabInstanceThumbnailScene<MaxNumScenes>::Clear()
{
	InstancedThumbnailScenes.Reset();
}

template class FPrefabInstanceThumbnailScene<400>;

//////////////////////////////////////////////////////////////////////////////////////
// UPrefabAssetThumbnailRenderer
//

UPrefabAssetThumbnailRenderer::UPrefabAssetThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPrefabAssetThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UPrefabAsset* Prefab = Cast<UPrefabAsset>(Object);

	if (Prefab && !Prefab->PrefabContent.IsEmpty())
	{
		return true;
	}
	return false;
}

void UPrefabAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UPrefabAsset* Prefab = Cast<UPrefabAsset>(Object);

	if (Prefab != nullptr && !Prefab->IsPendingKill())
	{
		//UE_LOG(LogTemp, Display, TEXT("[UPrefabAssetThumbnailRenderer::Draw]Draw %s"), *Prefab->GetPathName());
		TSharedRef<FPrefabThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(Prefab->GetPathName());
		ThumbnailScene->SetPrefab(Prefab);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		RenderViewFamily(Canvas, &ViewFamily);
	}
}

void UPrefabAssetThumbnailRenderer::BeginDestroy()
{
	ThumbnailScenes.Clear();

	Super::BeginDestroy();
}

void UPrefabAssetThumbnailRenderer::PrefabChanged(class UPrefabAsset* Prefab)
{
	if (Prefab && !Prefab->IsPendingKill())
	{
		TSharedRef<FPrefabThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(Prefab->GetPathName());
		ThumbnailScene->PrefabChanged(Prefab);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// FPrefabThumbnailScene
//

FPrefabThumbnailScene::FPrefabThumbnailScene()
	: FThumbnailPreviewScene()
	, NumStartingActors(0)
	, CurrentPrefab(nullptr)
{
	NumStartingActors = GetWorld()->GetCurrentLevel()->Actors.Num();
	//UE_LOG(LogTemp, Display, TEXT("[FPrefabThumbnailScene::FThumbnailPreviewScene] %d"), NumStartingActors);
}

void FPrefabThumbnailScene::SpawnPreviewActor()
{
	//UE_LOG(LogTemp, Display, TEXT("[FPrefabThumbnailScene::SpawnPreviewActor] %s"), *GetWorld()->GetCurrentLevel()->GetFName().ToString());

	ClearStaleActors();

	if (CurrentPrefab.Get())
	{
		TArray<AActor*> PreviewActors;
		FPrefabToolEditorUtil::SpawnPrefabInstances(CurrentPrefab.Get(), GetWorld(), PreviewActors, nullptr, RF_Transient);
		//FPrefabToolEditorUtil::RebuildAlteredBSP(PreviewActors); // For BSP support
		PreviewActorsBound = GetPreviewActorBounds(PreviewActors);
		PreviewActorsCenter = PreviewActorsBound.Origin;
	}
}

FBoxSphereBounds FPrefabThumbnailScene::GetPreviewActorBounds(const TArray<AActor*> InPreviewActors) const
{
	FBoxSphereBounds Bounds(ForceInitToZero);
	bool bFirstBound = true;
	for (AActor* PreviewActor : InPreviewActors)
	{
		if (!PreviewActor->IsPendingKillPending() && PreviewActor->GetRootComponent())
		{
			TArray<USceneComponent*> PreviewComponents;
			PreviewActor->GetRootComponent()->GetChildrenComponents(/*bIncludeAllDescendants=*/ true, PreviewComponents);
			PreviewComponents.Add(PreviewActor->GetRootComponent());

			for (USceneComponent* PreviewComponent : PreviewComponents)
			{
				if (IsValidComponentForVisualization(PreviewComponent))
				{
					if (bFirstBound)
					{
						Bounds = PreviewComponent->Bounds;
						bFirstBound = false;
					}
					else
					{
						Bounds = Bounds + PreviewComponent->Bounds;
					}
					
				}
			}
		}
	}

	return Bounds;
}

FVector FPrefabThumbnailScene::GetPreviewActorCenter(const TArray<AActor*> InPreviewActors) const
{
	FBox ActorsBox(EForceInit::ForceInitToZero);
	for (AActor* Actor : InPreviewActors)
	{
		if (!Actor->IsPendingKillPending())
		{
			ActorsBox += Actor->GetActorLocation();
		}
	}
	return ActorsBox.GetCenter();
}

void FPrefabThumbnailScene::ClearStaleActors()
{
	ULevel* Level = GetWorld()->GetCurrentLevel();

	for (int32 i = NumStartingActors; i < Level->Actors.Num(); ++i)
	{
		if (Level->Actors[i])
		{
			Level->Actors[i]->Destroy();
		}
	}
}

bool FPrefabThumbnailScene::IsValidComponentForVisualization(UActorComponent* Component)
{
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
	if (PrimComp) // && PrimComp->IsVisible())// && !PrimComp->bHiddenInGame)
	{
		UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component);
		if (StaticMeshComp && StaticMeshComp->GetStaticMesh())
		{
			return true;
		}

		USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Component);
		if (SkelMeshComp && SkelMeshComp->SkeletalMesh)
		{
			return true;
		}

		// Todo: enable Brush Component Preview when BSP is fully supported in Prefab
		// UBrushComponent* BrushComp = Cast<UBrushComponent>(Component);
		// if (BrushComp && BrushComp->Brush)
		// {
		// return true;
		// }
		return false;
	}

	return false;
}

void FPrefabThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	
	const float HalfMeshSize = PreviewActorsBound.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(PreviewActorsBound);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo(TargetDistance);
	check(ThumbnailInfo);

	//UE_LOG(LogTemp, Display, TEXT("Bounds.Origin:%s - Center:%s"), *Bounds.Origin.ToString(), *Center.ToString());
	OutOrigin = -1 * PreviewActorsCenter;
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

void FPrefabThumbnailScene::SetPrefab(UPrefabAsset* Prefab)
{
	CurrentPrefab = Prefab;
	if (Prefab && !Prefab->IsPendingKill() && !CachedPrefabContent.EqualTo(Prefab->PrefabContent))
	{
		CachedPrefabContent = Prefab->PrefabContent;
		SpawnPreviewActor();
	}
}

void FPrefabThumbnailScene::PrefabChanged(class UPrefabAsset* Prefab)
{
	bool bSpawnPreviewActor = false;

	if (CurrentPrefab.Get() == Prefab && !CachedPrefabContent.EqualTo(Prefab->PrefabContent))
	{
		CachedPrefabContent = Prefab->PrefabContent;
		bSpawnPreviewActor = true;
	}

	if (bSpawnPreviewActor)
	{
		SpawnPreviewActor();
	}
}

USceneThumbnailInfo* FPrefabThumbnailScene::GetSceneThumbnailInfo(const float TargetDistance) const
{
	UPrefabAsset* Prefab = CurrentPrefab.Get();
	check(Prefab);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(Prefab->ThumbnailInfo);
	if (ThumbnailInfo)
	{
		if (TargetDistance + ThumbnailInfo->OrbitZoom < 0)
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	return ThumbnailInfo;
}