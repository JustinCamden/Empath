// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabComponent.h"
#include "PrefabActor.h"
#include "PrefabAsset.h"
#include "PrefabToolHelpers.h"

#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BillboardComponent.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "UPrefabComponent"

UPrefabComponent::UPrefabComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> ConnectedTexture(TEXT("/PrefabTool/Icons/S_PrefabConnected.S_PrefabConnected"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DisConnectedTexture(TEXT("/PrefabTool/Icons/S_PrefabDisConnected.S_PrefabDisConnected"));

		PrefabConnectedEditorTexture = ConnectedTexture.Object;
		PrefabConnectedEditorTextureScale = 1.f;
		PrefabDisConnectedEditorTexture = DisConnectedTexture.Object;
		PrefabDisConnectedEditorTextureScale = 1.f;
	}
#endif

	PrimaryComponentTick.bCanEverTick = false;

	bConnected = true;
	bLockSelection = true;
	bTransient = false;
}

// FBoxSphereBounds UPrefabComponent::CalcBounds(const FTransform& LocalToWorld) const
// {
// #if WITH_EDITOR
// 	FBox BoundingBox = FPrefabActorUtil::GetAllComponentsBoundingBox(GetOwner());
// 	return FBoxSphereBounds(BoundingBox);
// #else
// 	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
// #endif
// }

UPrefabComponent::~UPrefabComponent()
{
}

const void UPrefabComponent::GetAllAttachedChildren(AActor* Parent, TArray<AActor*>& OutChildActors) const
{
	OutChildActors.Empty();

	TArray<USceneComponent*> ChildrenComponents;
	Parent->GetRootComponent()->GetChildrenComponents(/*bIncludeAllDecendants=*/true, ChildrenComponents);

	for (USceneComponent* Child : ChildrenComponents)
	{
		if (Child && !Child->IsPendingKill() && Child->GetOwner()
			&& !(Child->GetOwner() == Parent) && !Child->GetOwner()->IsPendingKillPending())
		{
			OutChildActors.AddUnique(Child->GetOwner());
		}
	}
}

const void UPrefabComponent::GetAllPotentialChildrenInstanceActors(TArray<AActor*>& OutChildActors) const
{
	struct Local
	{
		static void GetValidChildActors(AActor* InActor, TArray<AActor*>& OutValidChildActors, UPrefabAsset* InPrefab)
		{	
			TArray<AActor*> DirectAttachedChildActors;
			InActor->GetAttachedActors(DirectAttachedChildActors);

			for (AActor* Child : DirectAttachedChildActors)
			{
				if (Child && !Child->IsPendingKillPending())
				{
					APrefabActor* PrefabActor = Cast<APrefabActor>(Child);

					// Prevent recursive nested prefab
					const bool bSamePrefabFound = false;// PrefabActor && (InPrefab == PrefabActor->GetPrefabComponent()->GetPrefab());
					const bool bValid = !bSamePrefabFound;
					if (bValid)
					{
						OutValidChildActors.AddUnique(Child);
					}

					const bool bGetChild = bValid;

					if (bGetChild)
					{
						GetValidChildActors(Child, OutValidChildActors, InPrefab);
					}
				}
			}
		}
	};

	Local::GetValidChildActors(GetOwner(), OutChildActors, GetPrefab());
}

const void UPrefabComponent::GetDirectAttachedInstanceActors(TArray<AActor*>& OutChildActors) const
{
	OutChildActors.Empty();

	struct Local
	{
		static void GetDirectAttachedActor(AActor* Actor, TArray<AActor*>& OutDirectedChildActors)
		{
			if (Actor && !Actor->IsPendingKillPending())
			{
				TArray<AActor*> DirectChildActors;
				Actor->GetAttachedActors(DirectChildActors);
				for (AActor* Child : DirectChildActors)
				{
					OutDirectedChildActors.Add(Child);
					if (!Child->IsA(APrefabActor::StaticClass()))
					{
						GetDirectAttachedActor(Child, OutDirectedChildActors);
					}
				}
			}
		}
	};

	Local::GetDirectAttachedActor(GetOwner(), OutChildActors);
}

const void UPrefabComponent::GetDirectAttachedPrefabActors(TArray<APrefabActor*>& OutChildActors) const
{
	struct Local
	{
		static void GetChildPrefabActor(AActor* InActor, TArray<APrefabActor*>& OutChildPrefabActors)
		{
			TArray<AActor*> DirectAttachedChildActors;
			InActor->GetAttachedActors(DirectAttachedChildActors);

			for (AActor* Actor : DirectAttachedChildActors)
			{
				if (Actor && !Actor->IsPendingKillPending())
				{
					if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
					{
						OutChildPrefabActors.Add(PrefabActor);
					}
					else
					{
						GetChildPrefabActor(Actor, OutChildPrefabActors);
					}
				}
			}
		}
	};

	Local::GetChildPrefabActor(GetOwner(), OutChildActors);
}

const void UPrefabComponent::GetAllAttachedPrefabActors(TArray<APrefabActor*>& OutChildActors) const
{
	OutChildActors.Empty();

	TArray<AActor*> AllAttachedChildActors;
	GetAllAttachedChildren(GetOwner(), AllAttachedChildActors);

	for (AActor* Actor : AllAttachedChildActors)
	{
		if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
		{
			OutChildActors.Add(PrefabActor);
		}
	}
}

void UPrefabComponent::DeleteInvalidAttachedChildren(bool bRecursive)
{
	if (bRecursive)
	{
		TArray<APrefabActor*> DirectAttachedChildPrefabActors;
		GetDirectAttachedPrefabActors(DirectAttachedChildPrefabActors);
		for (APrefabActor* PrefabActor : DirectAttachedChildPrefabActors)
		{
			if (PrefabActor && PrefabActor->GetPrefabComponent())
			{
				PrefabActor->GetPrefabComponent()->DeleteInvalidAttachedChildren(bRecursive);
			}
		}
	}

	TArray<AActor*> InValidActors;
	ValidatePrefabInstancesMap(true, &InValidActors);

	TArray<AActor*> AllChildActors;
	GetAllAttachedChildren(GetOwner(), AllChildActors);

	struct Local
	{
		static bool IsActorInPrefabInstancesMap(AActor* InActor, const TMap<FName, AActor*>& InPrefabInstancesMap)
		{
			return (NULL != InPrefabInstancesMap.FindKey(InActor));
		}

		static bool IsActorSpawnedByValidChildActorComponent(AActor* InActor, const TMap<FName, AActor*>& InPrefabInstancesMap)
		{
			if (InActor->IsChildActor())
			{
				if (UChildActorComponent* ParentChildActorComponent = InActor->GetParentComponent())
				{
					if (AActor* ChildActorComponentActor = ParentChildActorComponent->GetOwner())
					{
						return IsActorInPrefabInstancesMap(ChildActorComponentActor, InPrefabInstancesMap);
					}
				}
			}
			return false;
		}
	};

	for (AActor* ChildActor : AllChildActors)
	{
		const bool bInPrefabInstancesMap = Local::IsActorInPrefabInstancesMap(ChildActor, PrefabInstancesMap);
		const bool bIsSpawnedByValidChildActorComponent = Local::IsActorSpawnedByValidChildActorComponent(ChildActor, PrefabInstancesMap);

		if (!bInPrefabInstancesMap && !bIsSpawnedByValidChildActorComponent)
		{
			InValidActors.Add(ChildActor);
			TArray<AActor*> Children;
			GetAllAttachedChildren(ChildActor, Children);
			for (AActor* Child : Children)
			{
				InValidActors.AddUnique(Child);
			}
		}
	}

	for (AActor* ToDeleteActor : InValidActors)
	{
		if (ToDeleteActor && !ToDeleteActor->IsPendingKillPending())
		{
			GetWorld()->EditorDestroyActor(ToDeleteActor, false);
		}
	}
}

void UPrefabComponent::ValidatePrefabInstancesMap(bool bRecursive/* = false*/, TArray<AActor*>* OutInValidActorsPtr /*=nullptr*/)
{
	if (bRecursive)
	{
		TArray<APrefabActor*> DirectAttachedChildPrefabActors;
		GetDirectAttachedPrefabActors(DirectAttachedChildPrefabActors);
		for (APrefabActor* PrefabActor : DirectAttachedChildPrefabActors)
		{
			PrefabActor->GetPrefabComponent()->ValidatePrefabInstancesMap(bRecursive, OutInValidActorsPtr);
		}
	}

	if (PrefabInstancesMap.Num() > 0)
	{
		TMap<FName, AActor*> ValidPrefabInstancesMap; 
		
		AActor* Owner = GetOwner();
	
		for (TMap<FName, AActor*>::TConstIterator It(PrefabInstancesMap); It; ++It)
		{
			AActor* InstanceActor = It.Value();

			bool bValid = false;
			if (InstanceActor && !InstanceActor->IsPendingKillPending() 
				&& InstanceActor->IsAttachedTo(Owner)
				// Todo: verify if only valid when attach to non-connected child prefab actor?
				// && !IsAttachToConnectedChildPrefabActor(AllAttachedChildPrefabActors, InstanceActor)
				)
			{
				bValid = true;
			}

			const bool bValidatingTag = false;
			if (bValidatingTag)
			{
				struct Local
				{
					static FName GetPrefabInstanceTag(const AActor* InInstanceActor, const FString& PrefabTagPrefix)
					{
						for (const FName& Tag : InInstanceActor->Tags)
						{
							if (Tag.ToString().StartsWith(PrefabTagPrefix))
							{
								return Tag;
							}
						}
						return NAME_None;
					}
				};
				const FString PrefabTagPrefix = GetPrefab()->GetPrefabTagPrefix();
				FName PrefabInstanceTag = Local::GetPrefabInstanceTag(InstanceActor, PrefabTagPrefix);
				if (PrefabInstanceTag == NAME_None)
				{
					bValid = false;
				}
			}

			if (It.Key() == NAME_None)
			{
				bValid = false;
			}

			if (bValid)
			{
				ValidPrefabInstancesMap.Add(It.Key(), It.Value());
			}
			else if (OutInValidActorsPtr)
			{
				OutInValidActorsPtr->AddUnique(It.Value());
			}
		}

		bool bUpdated = ValidPrefabInstancesMap.Num() != PrefabInstancesMap.Num();
		if (bUpdated)
		{
			Modify();
			PrefabInstancesMap = ValidPrefabInstancesMap;
		}
	}
}

void UPrefabComponent::SetConnected(bool bInConnected, bool bRecursive /*=true*/)
{
	bConnected = bInConnected;
#if WITH_EDITOR
	UpdatePrefabSpriteTexture();
#endif

	if (bRecursive)
	{
		TArray<APrefabActor*> ChildrenPrefabActors;
		GetAllAttachedPrefabActors(ChildrenPrefabActors);
		for (APrefabActor* ChildPrefabActor : ChildrenPrefabActors)
		{
			ChildPrefabActor->GetPrefabComponent()->SetConnected(bInConnected, /*bRecursive=*/ false);
		}
	}
}

void UPrefabComponent::SetLockSelection(bool bInLocked, bool bCheckParent /*= false*/, bool bRecursive /*=false*/)
{
// 	if (bCheckParent)
// 	{
// 		if (APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(GetOwner()))
// 		{
// 			const bool bIsParentLocked = ParentPrefabActor->GetLockSelection();
// 			if (bIsParentLocked && !bInLocked)
// 			{
// 				bLockSelection = bIsParentLocked; // Make it consistent with parent, might not needed
// 				return;
// 			}
// 		}
// 	}

	bLockSelection = bInLocked;

// 	if (bRecursive)
// 	{
// 		TArray<APrefabActor*> ChildrenPrefabActors;
// 		GetAllAttachedPrefabActors(ChildrenPrefabActors);
// 		for (APrefabActor* ChildPrefabActor : ChildrenPrefabActors)
// 		{
// 			ChildPrefabActor->SetLockSelection(bInLocked, /*bCheckParent=*/ false, /*bRecursive=*/ false);
// 		}
// 	}
}

const TMap<FName, AActor*>& UPrefabComponent::GetPrefabInstancesMap() const
{
	return PrefabInstancesMap;
}

TMap<FName, AActor*>& UPrefabComponent::GetPrefabInstancesMap()
{
	return PrefabInstancesMap;
}

#if WITH_EDITOR

bool UPrefabComponent::SetPrefab(class UPrefabAsset* NewPrefab)
{
	if (NewPrefab == GetPrefab())
	{
		return false;
	}

	Prefab = NewPrefab;

	return true;
}

bool UPrefabComponent::IsPrefabContentChanged(bool bRecursive /*= false*/) const
{
	struct Local
	{
		static bool IsPrefabContentChanged(const UPrefabComponent* InputPrefabComponent)
		{
			if (!InputPrefabComponent)
			{
				return false;
			}

			if (!InputPrefabComponent->Prefab)
			{
				return false;
			}

			if (!InputPrefabComponent->CachedPrefabHash.Equals(InputPrefabComponent->Prefab->PrefabHash))
			{
				return true;
			}

			return false;
		}
	};

	bool bPrefabContentChanged = Local::IsPrefabContentChanged(this);

	if (!bPrefabContentChanged && bRecursive)
	{
		TArray<APrefabActor*> ChildPrefabActors;
		GetAllAttachedPrefabActors(ChildPrefabActors);
		for (APrefabActor* ChildPrefabActor : ChildPrefabActors)
		{
			bPrefabContentChanged = Local::IsPrefabContentChanged(ChildPrefabActor->GetPrefabComponent());
			if (bPrefabContentChanged)
			{
				break;
			}
		}
	}

	return bPrefabContentChanged;
}

FVector UPrefabComponent::GetPrefabPivotOffset(bool bWorldSpace) const
{
	const bool bUseActorPivotOffset = false;
	return bUseActorPivotOffset ? GetOwner()->GetPivotOffset() : FVector::ZeroVector;
}

void UPrefabComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
// 		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrefabComponent, bLockSelection))
// 		{
// 			SetLockSelection(bLockSelection, /*bCheckParent=*/true, /*bRecursive=*/true);
// 		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrefabComponent, bConnected))
		{
			//ValidateInstancesTag(); // Todo: Should validate tags after connected status changed?
			UpdatePrefabSpriteTexture();
		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrefabComponent, Prefab))
		{
			APrefabActor* Owner = Cast<APrefabActor>(GetOwner());
			FPrefabToolEditorUtil::RevertPrefabActor(Owner);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPrefabComponent::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		SpriteComponent->SpriteInfo.Category = TEXT("Prefab");
		SpriteComponent->SpriteInfo.DisplayName = LOCTEXT("Prefab", "Prefab");

		UpdatePrefabSpriteTexture();
	}
}

void UPrefabComponent::OnUnregister()
{
	Super::OnUnregister();
}

UTexture2D* UPrefabComponent::GetEditorSprite() const
{
	return bConnected ? PrefabConnectedEditorTexture : PrefabDisConnectedEditorTexture;
}

float UPrefabComponent::GetEditorSpriteScale() const
{
	return bConnected ? PrefabConnectedEditorTextureScale : PrefabDisConnectedEditorTextureScale;
}

void UPrefabComponent::UpdatePrefabSpriteTexture()
{
	if (SpriteComponent != NULL)
	{
		SpriteComponent->SetSprite(GetEditorSprite());

		float SpriteScale = GetEditorSpriteScale();
		SpriteComponent->SetRelativeScale3D(FVector(SpriteScale));
	}
}

#endif

#undef LOCTEXT_NAMESPACE