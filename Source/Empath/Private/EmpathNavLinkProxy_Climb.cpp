// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavLinkProxy_Climb.h"
#include "Components/ArrowComponent.h"
#include "EmpathNavArea_Climb.h"
#include "AI/Navigation/NavLinkCustomComponent.h"

AEmpathNavLinkProxy_Climb::AEmpathNavLinkProxy_Climb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LedgeAlignmentComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("LedgeAlignComp0"));
	LedgeAlignmentComponent->SetupAttachment(RootComponent);
	LedgeAlignmentComponent->RelativeLocation = FVector(0, 0, 500.f);
	LedgeAlignmentComponent->ArrowColor = FColor(255, 0, 200, 255);
	LedgeAlignmentComponent->ArrowSize = 0.5f;
	LedgeAlignmentComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LedgeAlignmentComponent->bGenerateOverlapEvents = false;

	UNavLinkCustomComponent* const LinkComp = GetSmartLinkComp();
	if (LinkComp)
	{
		LinkComp->SetEnabledArea(UEmpathNavArea_Climb::StaticClass());
		LinkComp->SetLinkData(FVector::ZeroVector, FVector(0, 0, 500.f), ENavLinkDirection::LeftToRight);
	}
}
