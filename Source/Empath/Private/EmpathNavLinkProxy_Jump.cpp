// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavLinkProxy_Jump.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "EmpathNavArea_Jump.h"
#include "EmpathNavLineBatchComponent.h"
#include "Components/BoxComponent.h"
#include "EmpathFunctionLibrary.h"


namespace JumpProxyStatics
{
	static const FName LineBatcherName(TEXT("LineBatcher"));
	static const FLinearColor LineColorValid = FLinearColor::Green;
	static const FLinearColor LineColorInvalid = FLinearColor::Red;
	static const FLinearColor ApexColor = FLinearColor(FColor::Cyan);
	static const FLinearColor HitColor = FLinearColor(FLinearColor::Red);
	static const FLinearColor MissColor = FLinearColor(FLinearColor::Green);

	static const float RefreshTimerLoop = 3.0f;
	static const float RefreshTimerVariance = 1.5f;
}


AEmpathNavLinkProxy_Jump::AEmpathNavLinkProxy_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	EndEditorComp->RelativeLocation = FVector(300.f, 0, 0);
#endif

	UNavLinkCustomComponent* const LinkComp = GetSmartLinkComp();
	if (LinkComp)
	{
		LinkComp->SetEnabledArea(UEmpathNavArea_Jump::StaticClass());
		LinkComp->SetLinkData(FVector::ZeroVector, FVector(300.f, 0, 0), ENavLinkDirection::BothWays);
	}

	JumpArc = 0.5f;

#if WITH_EDITORONLY_DATA
	bTraceJumpArc = false;
	TraceRadius = 35.f;
	bTraceArcWithCollision = true;
	TraceCollisionChannel = ECC_Pawn;
	DrawArcSimFrequency = 5.f;
	LineBatcher = CreateEditorOnlyDefaultSubobject<UEmpathNavLineBatchComponent>(JumpProxyStatics::LineBatcherName);
	if (LineBatcher)
	{
		LineBatcher->PrimaryComponentTick.bCanEverTick = false; // Since we don't use lifetimes.
		LineBatcher->SetHiddenInGame(true);
		LineBatcher->SetupAttachment(RootComponent);
	}
	LastVisualizerUpdateFrame = uint64(-1);
#endif // WITH_EDITORONLY_DATA

}


void AEmpathNavLinkProxy_Jump::PostLoad()
{
	Super::PostLoad();
	bWasLoaded = true;
}

#if WITH_EDITOR
bool AEmpathNavLinkProxy_Jump::PredictJumpPath(FPredictProjectilePathResult& OutPathResult, bool& OutHit) const
{
	// Initialize variables
	const FVector PointOffset(0.f, 0.f, TraceRadius + 1.f);
	const FVector StartPos = StartEditorComp->GetComponentLocation() + PointOffset;
	const FVector EndPos = EndEditorComp->GetComponentLocation() + PointOffset;
	FVector LaunchVel;
	bool bHit = false;
	OutHit = bHit;

	// Get the suggested arc and see if we hit
	bool const bSuccess = UEmpathFunctionLibrary::SuggestProjectileVelocity(this, LaunchVel, StartPos, EndPos, JumpArc);
	if (bSuccess)
	{
		const float DistanceXY = FVector::Dist2D(StartPos, EndPos);
		const float VelocityXY = LaunchVel.Size2D();
		const float TimeToTarget = (VelocityXY > 0.f) ? DistanceXY / VelocityXY : 0.f;

		// Since we limit the jump arc, we should never have zero horizontal velocity
		if (TimeToTarget == 0.f)
		{
			return false;
		}

		// Set trace params
		FPredictProjectilePathParams PredictParams(TraceRadius, StartPos, LaunchVel, TimeToTarget, TraceCollisionChannel);
		PredictParams.bTraceWithCollision = bTraceArcWithCollision;
		PredictParams.SimFrequency = DrawArcSimFrequency;

		// Ignore some types.
		TArray<AActor*> ActorsToIgnore;
		for (const TSubclassOf<AActor>& TypeToIgnore : ActorTypesToIgnore)
		{
			UGameplayStatics::GetAllActorsOfClass(this, TypeToIgnore, ActorsToIgnore);
			PredictParams.ActorsToIgnore.Append(ActorsToIgnore);
		}

		// Do the trace
		bHit = UGameplayStatics::PredictProjectilePath(this, PredictParams, OutPathResult);
		if (OutPathResult.PathData.Num() == 0)
		{
			return false;
		}

		// We offset all the points to push the sphere up, but let's draw the bottom of the arc.
		for (FPredictProjectilePathPointData& PointData : OutPathResult.PathData)
		{
			PointData.Location -= PointOffset;
		}
	}

	OutHit = bHit;
	return bSuccess;
}
#endif // WITH_EDITOR


void AEmpathNavLinkProxy_Jump::RefreshPathVisualizer()
{
#if WITH_EDITOR
	if (LastVisualizerUpdateFrame != GFrameCounter && GIsEditor && !IsRunningCommandlet())
	{
		LastVisualizerUpdateFrame = GFrameCounter;
		if (UWorld* World = GetWorld())
		{
			if (World->IsGameWorld())
			{
				return;
			}
		}

		bTracedPath = false;
		TraceStart.Reset();
		TraceApex.Reset();
		TraceEndPoint.Reset();
		if (LineBatcher)
		{
			LineBatcher->Flush();
		}

		FPredictProjectilePathResult PathResult;
		if (PredictJumpPath(PathResult, bTraceHit) && PathResult.PathData.Num() > 0)
		{
			const bool bUpdateVisuals = (bTraceJumpArc && (LineBatcher != nullptr));
			bTracedPath = true;
			TArray<FBatchedLine> Lines;
			const TArray<FPredictProjectilePathPointData>& PathData = PathResult.PathData;
			const int32 NumPoints = PathData.Num();
			if (bUpdateVisuals)
			{
				Lines.Reserve(NumPoints - 1);
			}
			int32 ApexIndex = 0;
			TraceStart = PathData[0];

			FLinearColor EnabledColor = JumpProxyStatics::LineColorValid;
			UNavLinkCustomComponent* const LinkComp = GetSmartLinkComp();
			if (LinkComp)
			{
				if (TSubclassOf<UNavArea> NavAreaClass = LinkComp->GetLinkAreaClass())
				{
					EnabledColor = LinkComp->GetLinkAreaClass().GetDefaultObject()->DrawColor;
				}
			}

			const FLinearColor DrawColor = (!bTraceHit && IsNavigationRelevant()) ? EnabledColor : JumpProxyStatics::LineColorInvalid;
			for (int32 i = 0; i < NumPoints - 1; i++)
			{
				const FPredictProjectilePathPointData& P0 = PathData[i];
				const FPredictProjectilePathPointData& P1 = PathData[i + 1];
				if (P1.Location.Z > PathData[ApexIndex].Location.Z)
				{
					ApexIndex = i + 1;
				}

				if (bUpdateVisuals)
				{
					Lines.Emplace(FBatchedLine(P0.Location, P1.Location, DrawColor, LineBatcher->DefaultLifeTime, 5.f, 0));
				}
			}

			TraceEndPoint = PathData.Last();
			TraceApex = PathData[ApexIndex];

			if (bUpdateVisuals)
			{
				LineBatcher->DrawLines(Lines);
				LineBatcher->DrawPoint(TraceEndPoint.Location, bTraceHit ? JumpProxyStatics::HitColor : JumpProxyStatics::MissColor, 10.f, 0, 0);
				if (bDrawApex)
				{
					LineBatcher->DrawPoint(TraceApex.Location, JumpProxyStatics::ApexColor, 6.f, 0, 0);
				}
			}
		}
	}

	SetRefreshTimer();
#endif // WITH_EDITOR
}


#if WITH_EDITOR

void AEmpathNavLinkProxy_Jump::SetRefreshTimer()
{
	if (GIsEditor && !IsRunningCommandlet())
	{
		UWorld* World = GetWorld();
		if (World && !World->IsGameWorld())
		{
			float TimerDelay = JumpProxyStatics::RefreshTimerLoop + FMath::RandRange(-JumpProxyStatics::RefreshTimerVariance, JumpProxyStatics::RefreshTimerVariance);
			TimerDelay = FMath::Max(0.1f, TimerDelay);

			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(TimerHandle, this, &AEmpathNavLinkProxy_Jump::RefreshPathVisualizer, TimerDelay, false);
		}
	}
}

void AEmpathNavLinkProxy_Jump::SyncLinkDataToComponents()
{
	Super::SyncLinkDataToComponents();
	// force an update
	LastVisualizerUpdateFrame = 0;
	RefreshPathVisualizer();
}

void AEmpathNavLinkProxy_Jump::PostEditImport()
{
	Super::PostEditImport();
	RefreshPathVisualizer();
}

#endif // WITH_EDITOR



void AEmpathNavLinkProxy_Jump::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	if (!IsTemplate() && GIsEditor)
	{
		SetRefreshTimer();
		if (!bRegisteredCallbacks)
		{
			bRegisteredCallbacks = true;
			RefreshPathVisualizer();
		}
	}
#endif // WITH_EDITOR
}
