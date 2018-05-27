// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathEnvQueryTest_Dot.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Dot.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Item.h"
#include "EQC_AttackTarget.h"
#include "EmpathAIController.h"
#include "EmpathPlayerCharacter.h"

UEmpathEnvQueryTest_Dot::UEmpathEnvQueryTest_Dot(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	LineA.DirMode = EEnvDirection::Rotation;
	LineA.Rotation = UEnvQueryContext_Querier::StaticClass();
	LineB.DirMode = EEnvDirection::TwoPoints;
	LineB.LineFrom = UEnvQueryContext_Querier::StaticClass();
	LineB.LineTo = UEnvQueryContext_Item::StaticClass();

	TestMode = EEnvTestDot::Dot3D;
	bAbsoluteValue = false;
	bUseCamRotInsteadOfPlayerIn360TrackingMode = true;
}

void UEmpathEnvQueryTest_Dot::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	FloatValueMin.BindData(QueryOwner, QueryInstance.QueryID);
	float MinThresholdValue = FloatValueMin.GetValue();

	FloatValueMax.BindData(QueryOwner, QueryInstance.QueryID);
	float MaxThresholdValue = FloatValueMax.GetValue();

	// gather all possible directions: for contexts different than Item
	TArray<FVector> LineADirs;
	const bool bUpdateLineAPerItem = RequiresPerItemUpdates(LineA.LineFrom, LineA.LineTo, LineA.Rotation, LineA.DirMode == EEnvDirection::Rotation);
	if (!bUpdateLineAPerItem)
	{
		GatherLineDirections(LineADirs, QueryInstance, LineA.LineFrom, LineA.LineTo, LineA.Rotation, LineA.DirMode == EEnvDirection::Rotation);
		if (LineADirs.Num() == 0)
		{
			return;
		}
	}

	TArray<FVector> LineBDirs;
	const bool bUpdateLineBPerItem = RequiresPerItemUpdates(LineB.LineFrom, LineB.LineTo, LineB.Rotation, LineB.DirMode == EEnvDirection::Rotation);
	if (!bUpdateLineBPerItem)
	{
		GatherLineDirections(LineBDirs, QueryInstance, LineB.LineFrom, LineB.LineTo, LineB.Rotation, LineB.DirMode == EEnvDirection::Rotation);
		if (LineBDirs.Num() == 0)
		{
			return;
		}
	}

	// loop through all items
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		// update lines for contexts using current item
		if (bUpdateLineAPerItem || bUpdateLineBPerItem)
		{
			const FVector ItemLocation = (LineA.DirMode == EEnvDirection::Rotation && LineB.DirMode == EEnvDirection::Rotation) ? FVector::ZeroVector : GetItemLocation(QueryInstance, It.GetIndex());
			const FRotator ItemRotation = (LineA.DirMode == EEnvDirection::Rotation || LineB.DirMode == EEnvDirection::Rotation) ? GetItemRotation(QueryInstance, It.GetIndex()) : FRotator::ZeroRotator;

			if (bUpdateLineAPerItem)
			{
				LineADirs.Reset();
				GatherLineDirections(LineADirs, QueryInstance, LineA.LineFrom, LineA.LineTo, LineA.Rotation, LineA.DirMode == EEnvDirection::Rotation, ItemLocation, ItemRotation);
			}

			if (bUpdateLineBPerItem)
			{
				LineBDirs.Reset();
				GatherLineDirections(LineBDirs, QueryInstance, LineB.LineFrom, LineB.LineTo, LineB.Rotation, LineB.DirMode == EEnvDirection::Rotation, ItemLocation, ItemRotation);
			}
		}

		// perform test for each line pair
		for (int32 LineAIndex = 0; LineAIndex < LineADirs.Num(); LineAIndex++)
		{
			for (int32 LineBIndex = 0; LineBIndex < LineBDirs.Num(); LineBIndex++)
			{
				float DotValue = 0.f;
				switch (TestMode)
				{
				case EEnvTestDot::Dot3D:
					DotValue = FVector::DotProduct(LineADirs[LineAIndex], LineBDirs[LineBIndex]);
					break;

				case EEnvTestDot::Dot2D:
					DotValue = LineADirs[LineAIndex].CosineAngle2D(LineBDirs[LineBIndex]);
					break;

				default:
					UE_LOG(LogEQS, Error, TEXT("Invalid TestMode in EmpathEnvQueryTest_Dot in query %s!"), *QueryInstance.QueryName);
					break;
				}

				if (bAbsoluteValue)
				{
					DotValue = FMath::Abs(DotValue);
				}
				It.SetScore(TestPurpose, FilterType, DotValue, MinThresholdValue, MaxThresholdValue);
			}
		}
	}
}

void UEmpathEnvQueryTest_Dot::GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance, const FVector& ItemLocation,
	TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo) const
{
	TArray<FVector> ContextLocationFrom;
	if (IsContextPerItem(LineFrom))
	{
		ContextLocationFrom.Add(ItemLocation);
	}
	else
	{
		QueryInstance.PrepareContext(LineFrom, ContextLocationFrom);
	}

	for (int32 FromIndex = 0; FromIndex < ContextLocationFrom.Num(); FromIndex++)
	{
		TArray<FVector> ContextLocationTo;
		if (IsContextPerItem(LineTo))
		{
			ContextLocationTo.Add(ItemLocation);
		}
		else
		{
			QueryInstance.PrepareContext(LineTo, ContextLocationTo);
		}

		for (int32 ToIndex = 0; ToIndex < ContextLocationTo.Num(); ToIndex++)
		{
			const FVector Dir = (ContextLocationTo[ToIndex] - ContextLocationFrom[FromIndex]).GetSafeNormal();
			Directions.Add(Dir);
		}
	}
}

void UEmpathEnvQueryTest_Dot::GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance, const FRotator& ItemRotation, TSubclassOf<UEnvQueryContext> LineDirection) const
{
	TArray<FRotator> ContextRotations;
	if (IsContextPerItem(LineDirection))
	{
		ContextRotations.Add(ItemRotation);
	}
	else
	{
		QueryInstance.PrepareContext(LineDirection, ContextRotations);
	}

	for (int32 RotationIndex = 0; RotationIndex < ContextRotations.Num(); RotationIndex++)
	{
		const FVector Dir = ContextRotations[RotationIndex].Vector();
		Directions.Add(Dir);
	}

	// Override if necessary
	if (bUseCamRotInsteadOfPlayerIn360TrackingMode && (LineDirection == UEQC_AttackTarget::StaticClass()))
	{
		APawn* const QueryOwner = Cast<APawn>(QueryInstance.Owner.Get());
		if (QueryOwner)
		{
			AEmpathAIController const* const AI = Cast<AEmpathAIController>(QueryOwner->GetController());
			if (AI)
			{
				AEmpathPlayerCharacter const* const PlayerAttackTarget = Cast<AEmpathPlayerCharacter>(AI->GetAttackTarget());
				if (PlayerAttackTarget)
				{
					APlayerController const* const TargetPC = Cast<APlayerController>(PlayerAttackTarget->GetController());
					if (TargetPC)
					{
						FVector CamLoc;
						FRotator CamRot;
						TargetPC->GetPlayerViewPoint(CamLoc, CamRot);

						FVector const CamFacingDir = CamRot.Vector();

						for (int32 DirIndex = 0; DirIndex < Directions.Num(); DirIndex++)
						{
							// set to cam facing direction
							Directions[DirIndex] = CamFacingDir;
						}
					}
				}
			}
		}
	}
}

void UEmpathEnvQueryTest_Dot::GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance,
	TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo, TSubclassOf<UEnvQueryContext> LineDirection, bool bUseDirectionContext,
	const FVector& ItemLocation, const FRotator& ItemRotation) const
{
	if (bUseDirectionContext)
	{
		GatherLineDirections(Directions, QueryInstance, ItemRotation, LineDirection);
	}
	else
	{
		GatherLineDirections(Directions, QueryInstance, ItemLocation, LineFrom, LineTo);
	}
}

bool UEmpathEnvQueryTest_Dot::RequiresPerItemUpdates(TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo, TSubclassOf<UEnvQueryContext> LineDirection, bool bUseDirectionContext) const
{
	bool bRequirePerItemUpdate = false;
	if (bUseDirectionContext)
	{
		bRequirePerItemUpdate = IsContextPerItem(LineDirection);
	}
	else
	{
		bRequirePerItemUpdate = IsContextPerItem(LineFrom) || IsContextPerItem(LineTo);
	}

	return bRequirePerItemUpdate;
}

FText UEmpathEnvQueryTest_Dot::GetDescriptionTitle() const
{
	FString ModeDesc;
	switch (TestMode)
	{
	case EEnvTestDot::Dot3D:
		ModeDesc = TEXT("");
		break;

	case EEnvTestDot::Dot2D:
		ModeDesc = TEXT(" 2D");
		break;

	default:
		break;
	}

	return FText::FromString(FString::Printf(TEXT("(Empath) %s%s%s: %s and %s"), bAbsoluteValue ? TEXT("Absolute ") : TEXT(""),
		*Super::GetDescriptionTitle().ToString(), *ModeDesc, *LineA.ToText().ToString(), *LineB.ToText().ToString()));

}

FText UEmpathEnvQueryTest_Dot::GetDescriptionDetails() const
{
	return DescribeFloatTestParams();
}
