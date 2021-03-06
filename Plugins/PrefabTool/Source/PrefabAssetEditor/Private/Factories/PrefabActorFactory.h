// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "PrefabActorFactory.generated.h"

/**
* PrefabActorFactory responsible for spawning prefab instances
*/
UCLASS(hidecategories=Object)
class UPrefabActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin UActorFactory
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual bool PreSpawnActor(UObject* Asset, FTransform& InOutLocation) override;
	//virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name);
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	//virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const override;
	//~ End UActorFactory

	bool bStampMode;

private:

	void SpawnPrefabInstances(class APrefabActor* PrefabActor, class UPrefabAsset* Prefab, UWorld* InWorld, const FTransform& Transform, TArray<AActor*>& OutSpawnActors, TMap<FName, AActor*>* OutPrefabMapPtr, EObjectFlags InObjectFlags);

};
