// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabComponentVisualizer.h"
#include "PrefabComponent.h"
#include "PrefabToolSettings.h"
#include "PrefabToolHelpers.h"

#include "SceneManagement.h"

static const FLinearColor ConnectedColor(0.f, 1.f, 0.f); // Green
static const FLinearColor DisConnectedColor(1.0f, 0.f, 0.f); // Red
static const FLinearColor LockedConnectedColor(0.f, 0.12f, 0.f); // Dark Green
static const FLinearColor LockedDisConnectedColor(0.12f, 0.f, 0.f); // Dark Red

void FPrefabComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if (IsVisualizerEnabled(View->Family->EngineShowFlags))
	{
		const UPrefabComponent* PrefabComponent = Cast<const UPrefabComponent>(Component);

		if (PrefabComponent != nullptr)
		{
			const bool bConnected = PrefabComponent->GetConnected();
			const bool bLocked = PrefabComponent->GetLockSelection();
			const FTransform& PrefabComponentTransform = PrefabComponent->GetComponentTransform();

			FLinearColor WireBoxColor = bConnected
				? (bLocked ? LockedConnectedColor : ConnectedColor)
				: (bLocked ? LockedDisConnectedColor : DisConnectedColor);

			FBox BoundingBox = FPrefabActorUtil::GetAllComponentsBoundingBox(PrefabComponent->GetOwner());
			DrawWireBox(PDI, BoundingBox, WireBoxColor, /*DepthPriority*/ SDPG_Foreground); // , float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

			const bool bDrawLineToInstanceActors = false;

			if (bDrawLineToInstanceActors)
			{
				TArray<AActor*> InstanceActors;
				PrefabComponent->GetDirectAttachedInstanceActors(InstanceActors);

				const float ArrowSize = 4.f;
				const float ArrowThickness = 1.f;
				const float MinArrowLengthToDraw = 1.f;

				for (AActor* InstanceActor : InstanceActors)
				{
					FTransform InstanceActorTransform = InstanceActor->GetActorTransform();
					FVector ToPrefabVector = InstanceActorTransform.GetTranslation() - PrefabComponentTransform.GetTranslation();

					const float ArrowLength = ToPrefabVector.Size();
					if (ArrowLength > MinArrowLengthToDraw)
					{
						if (bConnected)
						{
							FQuat LookAtRotation = InstanceActorTransform.GetRotation().FindBetween(FVector::ForwardVector, ToPrefabVector.GetSafeNormal());
							DrawDirectionalArrow(PDI, FRotationMatrix(LookAtRotation.Rotator()) * PrefabComponentTransform.ToMatrixNoScale(), ConnectedColor, /*Length=*/ ArrowLength, /*ArrowSize=*/ ArrowSize, SDPG_Foreground, /*Thickness*/ ArrowThickness);
						}
						else
						{
							DrawDashedLine(PDI, /*Start*/PrefabComponentTransform.GetLocation(), /*End*/InstanceActorTransform.GetLocation(), DisConnectedColor, /*DashSize*/ArrowThickness, SDPG_Foreground, /*DepthBias =*/ 0.0f);
						}
					}
				}
			}
		}
	}
}

bool FPrefabComponentVisualizer::IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const
{
	const UPrefabToolSettings* PrefabToolSetting = GetDefault<UPrefabToolSettings>();
	return (PrefabToolSetting && PrefabToolSetting->ShouldEnablePrefabComponentVisualizer());
}
