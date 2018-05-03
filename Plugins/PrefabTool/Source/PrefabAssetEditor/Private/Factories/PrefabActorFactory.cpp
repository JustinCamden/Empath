// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabActorFactory.h"
#include "PrefabActor.h"
#include "PrefabComponent.h"
#include "PrefabToolHelpers.h"
#include "PrefabAsset.h"
#include "PrefabToolSettings.h"

#include "AssetData.h"

#define LOCTEXT_NAMESPACE "PrefabActorFactory"

UPrefabActorFactory::UPrefabActorFactory( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("PrefabDisplayName", "Prefab");
	NewActorClass = APrefabActor::StaticClass();
	bShowInEditorQuickMenu = false;
	bUseSurfaceOrientation = true;
	bStampMode = false;
}

bool UPrefabActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid() && AssetData.GetClass()->IsChildOf(UPrefabAsset::StaticClass()))
	{
		return true;
	}

	return false;
}

bool UPrefabActorFactory::PreSpawnActor(UObject* Asset, FTransform& InOutLocation)
{
	UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);

	if (Prefab == NULL)
	{
		return false;
	}

	return true;
}

void UPrefabActorFactory::PostSpawnActor(UObject* Asset, AActor* InNewActor)
{
	Super::PostSpawnActor(Asset, InNewActor);

	UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);

	APrefabActor* PrefabActor = CastChecked<APrefabActor>(InNewActor);
	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	check(PrefabComponent);

	PrefabComponent->UnregisterComponent();

	PrefabComponent->SetPrefab(Prefab);
	PrefabComponent->bVisualizeComponent = true;

	PrefabComponent->RegisterComponent();

	TArray<AActor*> PrefabInstances;
	TMap<FName, AActor*> PrefabInstancesMap;
	EObjectFlags ObjectFlag = PrefabActor->HasAnyFlags(RF_Transient) ? RF_Transient : RF_Transactional;
	SpawnPrefabInstances(PrefabActor, Prefab, PrefabActor->GetLevel()->OwningWorld, PrefabComponent->GetComponentTransform(), PrefabInstances, &PrefabInstancesMap, ObjectFlag);

	const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>(); 
	
	if (bStampMode)
	{
		bool bForceChildrenPrefabDisconnected = true; // PrefabToolSettings->ShouldLockPrefabSelectionByDefault()
		PrefabActor->GetPrefabComponent()->SetConnected(false, bForceChildrenPrefabDisconnected);
	}
	
	if (PrefabToolSettings->ShouldLockPrefabSelectionByDefault())
	{
		PrefabActor->SetLockSelection(true);
	}
	else
	{
		PrefabActor->SetLockSelection(false);
	}
}

void UPrefabActorFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);
		APrefabActor* PrefabActor = CastChecked<APrefabActor>(CDO);
		UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();

		PrefabComponent->SetPrefab(Prefab);
	}
}

UObject* UPrefabActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	check(ActorInstance->IsA(NewActorClass));
	APrefabActor* PrefabActor = CastChecked<APrefabActor>(ActorInstance);
	check(PrefabActor->GetPrefabComponent());
	return PrefabActor->GetPrefabComponent()->GetPrefab();
}

void UPrefabActorFactory::SpawnPrefabInstances(APrefabActor* PrefabActor, UPrefabAsset* Prefab, UWorld* InWorld, const FTransform& Transform, TArray<AActor*>& OutSpawnActors, TMap<FName, AActor*>* OutPrefabMapPtr, EObjectFlags InObjectFlags)
{
	const bool bPrevewing = !!(InObjectFlags & RF_Transient);
	//PREFABTOOL_LOG(Display, TEXT("UPrefabActorFactory::SpawnActor: PrefabActor [Actor] %s [ObjectFlag] %d [bPrevewing] %d"), *PrefabActor->GetActorLabel(), (int32)InObjectFlags, bPrevewing);

	if (!bPrevewing) // Alternative check: FActorEditorUtils::IsAPreviewOrInactiveActor()
	{
		FPrefabToolEditorUtil::RevertPrefabActor(PrefabActor);
	}
}

#undef LOCTEXT_NAMESPACE
