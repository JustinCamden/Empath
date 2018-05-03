// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabActor.h"
#include "PrefabComponent.h"
#include "PrefabAsset.h"
#include "PrefabToolHelpers.h"
#include "PrefabToolSettings.h"

#include "Engine/Brush.h"

#define LOCTEXT_NAMESPACE "APrefabActor"

bool APrefabActor::bSuppressPostDuplicate = false;

APrefabActor::APrefabActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bCanBeDamaged = false;

	PrefabComponent = CreateDefaultSubobject<UPrefabComponent>(TEXT("PrefabComponent"));
	PrefabComponent->Mobility = EComponentMobility::Static;
#if WITH_EDITORONLY_DATA
	PrefabComponent->bVisualizeComponent = true; // Focusable
#endif

	RootComponent = PrefabComponent;
}

void APrefabActor::SetMobility(EComponentMobility::Type InMobility)
{
	if (PrefabComponent)
	{
		PrefabComponent->SetMobility(InMobility);
	}
}

void APrefabActor::SetPrefab(UPrefabAsset* NewPrefab, bool bForceRevertEvenDisconnected /*= false*/)
{
	PrefabComponent->Modify();
	PrefabComponent->Prefab = NewPrefab;
#if WITH_EDITOR
	if (bForceRevertEvenDisconnected)
	{
		FPrefabToolEditorUtil::RevertPrefabActorEvenDisconnected(this);
	}
	else
	{
		FPrefabToolEditorUtil::RevertPrefabActor(this);
	}
	
#endif
}

class UPrefabAsset* APrefabActor::GetPrefab()
{
	return PrefabComponent->Prefab;
}

void APrefabActor::DestroyPrefabActor(bool bDestroyInstanceActors)
{
#if WITH_EDITOR
	FPrefabToolEditorUtil::DestroyPrefabActor(this, bDestroyInstanceActors);
#endif
}

UPrefabComponent* APrefabActor::GetPrefabComponent() const { return PrefabComponent; }

void APrefabActor::SetAndModifyLockSelection(bool bInLocked)
{
	if (PrefabComponent)
	{
		PrefabComponent->Modify();
		PrefabComponent->SetLockSelection(bInLocked);
	}
}

void APrefabActor::SetLockSelection(bool bInLocked)
{
	if (PrefabComponent)
	{
		PrefabComponent->SetLockSelection(bInLocked);
	}
}

bool APrefabActor::GetLockSelection()
{
	return PrefabComponent && PrefabComponent->GetLockSelection();
}

bool APrefabActor::IsConnected() const
{
	return PrefabComponent && PrefabComponent->GetConnected();
}

bool APrefabActor::IsTransient() const
{
	return PrefabComponent && PrefabComponent->bTransient;
}

#if WITH_EDITOR

void APrefabActor::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bSuppressPostDuplicate && !bDuplicateForPIE)
	{
		if (PrefabComponent && PrefabComponent->GetConnected())
		{
			FPrefabToolEditorUtil::RevertPrefabActor(this);
		}
	}
}

void APrefabActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* Property = PropertyChangedEvent.Property;

	const bool bHideChildrenActors = true;
	if (bHideChildrenActors && Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, bHiddenEd))
	{
		TArray<AActor*> ChildActors;
		FPrefabActorUtil::GetAllAttachedChildren(this, ChildActors);
		for (AActor* ChildActor : ChildActors)
		{
			if (ChildActor && !ChildActor->IsPendingKillPending() && !ChildActor->IsTemporarilyHiddenInEditor())
			{
				ChildActor->SetIsTemporarilyHiddenInEditor(true);
			}
		}
	}
}

bool APrefabActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (PrefabComponent && PrefabComponent->GetPrefab())
	{
		Objects.Add(PrefabComponent->GetPrefab());
	}
	return true;
}

void APrefabActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	const bool bSupportBSP = GetDefault<UPrefabToolSettings>()->ShouldEnableBSPBrushSupport();

	const bool bPropagatePostEditModeEvent = true;

	if (PrefabComponent && PrefabComponent->GetPrefab())
	{

	}

	if (bSupportBSP || bPropagatePostEditModeEvent)
	{
		if (bFinished && PrefabComponent && PrefabComponent->GetPrefab())
		{
			TArray<AActor*> ChildActors;
			PrefabComponent->GetAllPotentialChildrenInstanceActors(ChildActors);

			if (bPropagatePostEditModeEvent)
			{
				for (AActor* Actor : ChildActors)
				{
					if (Actor && !Actor->IsPendingKillPending())
					{
						Actor->PostEditMove(bFinished);
					}
				}
			}

			if (bSupportBSP)
			{
				bool bHasBSP = false;
				for (AActor* Actor : ChildActors)
				{
					if (Actor && !Actor->IsPendingKillPending())
					{
						ABrush* Brush = Cast<ABrush>(Actor);
						if (Brush && !Brush->IsVolumeBrush())
						{
							bHasBSP = true;
							break;
						}
					}
				}
				if (bHasBSP)
				{
					FPrefabToolEditorUtil::RebuildAlteredBSP(ChildActors);
				}
			}
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE