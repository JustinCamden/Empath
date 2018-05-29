// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavLinkProxy.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "Components/BoxComponent.h"

AEmpathNavLinkProxy::AEmpathNavLinkProxy(const FObjectInitializer& ObjectInitializer)
{
	PointLinks.Empty();

	bSmartLinkIsRelevant = true;

	ClaimReleaseDelay = 0.5f;

#if WITH_EDITORONLY_DATA
	StartEditorComp = CreateDefaultSubobject<UBoxComponent>(TEXT("StartBox0"));
	StartEditorComp->InitBoxExtent(FVector(20.f, 20.f, 10.f));
	StartEditorComp->SetupAttachment(RootComponent);
	StartEditorComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartEditorComp->bGenerateOverlapEvents = false;

	EndEditorComp = CreateDefaultSubobject<UBoxComponent>(TEXT("EndBox0"));
	EndEditorComp->InitBoxExtent(FVector(20.f, 20.f, 10.f));
	EndEditorComp->SetupAttachment(RootComponent);
	EndEditorComp->RelativeLocation = FVector(0, 0, 500.f);
	EndEditorComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndEditorComp->bGenerateOverlapEvents = false;
#endif
}



#if WITH_EDITOR
void AEmpathNavLinkProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_EDITORONLY_DATA
	// Update the editor boxes. This assumes we aren't changing the location of the start/end components in this edit action.
	UNavLinkCustomComponent* const LinkComp = GetSmartLinkComp();
	if (LinkComp)
	{
		StartEditorComp->SetWorldLocation(LinkComp->GetStartPoint());
		EndEditorComp->SetWorldLocation(LinkComp->GetEndPoint());
	}
#endif // WITH_EDITORONLY_DATA

	SyncLinkDataToComponents();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AEmpathNavLinkProxy::OnChildTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType TeleportType)
{
	SyncLinkDataToComponents();
}

void AEmpathNavLinkProxy::PostEditMove(bool bFinished)
{
	SyncLinkDataToComponents();
	Super::PostEditMove(bFinished);
}

void AEmpathNavLinkProxy::SyncLinkDataToComponents()
{
#if WITH_EDITORONLY_DATA
	FVector const NewLinkRelativeStart = GetActorTransform().InverseTransformPosition(StartEditorComp->GetComponentLocation());
	FVector const NewLinkRelativeEnd = GetActorTransform().InverseTransformPosition(EndEditorComp->GetComponentLocation());

	FVector LeftPt, RightPt;
	ENavLinkDirection::Type Dir;
	GetSmartLinkComp()->GetLinkData(LeftPt, RightPt, Dir);
	GetSmartLinkComp()->SetLinkData(NewLinkRelativeStart, NewLinkRelativeEnd, Dir);
#endif // WITH_EDITORONLY_DATA
}

void AEmpathNavLinkProxy::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITORONLY_DATA
	if (!bRegisteredCallbacks && !IsTemplate())
	{
		// Listen to transform changes on child components.
		bRegisteredCallbacks = true;
		StartEditorComp->TransformUpdated.AddUObject(this, &AEmpathNavLinkProxy::OnChildTransformUpdated);
		EndEditorComp->TransformUpdated.AddUObject(this, &AEmpathNavLinkProxy::OnChildTransformUpdated);
	}
#endif // WITH_EDITORONLY_DATA
}


#endif // WITH_EDITOR


void AEmpathNavLinkProxy::Claim(AEmpathAIController* ClaimHolder)
{
	ensure(IsClaimed() == false);

	SetSmartLinkEnabled(false);
	ClaimedBy = ClaimHolder;
}

void AEmpathNavLinkProxy::BeginReleaseClaim(bool bImmediate)
{
	if (bImmediate)
	{
		ReleaseClaim();
	}
	else
	{
		// release timer after a short delay, so AI doesn't react perfectly
		GetWorldTimerManager().SetTimer(ReleaseClaimTimerHandle, this, &AEmpathNavLinkProxy::ReleaseClaim, ClaimReleaseDelay, false);
	}
}

void AEmpathNavLinkProxy::ReleaseClaim()
{
	SetSmartLinkEnabled(true);
	ClaimedBy = nullptr;
}


