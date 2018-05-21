// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Dot.h"
#include "EmpathEnvQueryTest_Dot.generated.h"

// This is a clone of the standard Unreal dot test, with some modified behavior of Gather Line Directions
// that allows us to account for the rotation of VR characters

UCLASS(MinimalAPI)
class UEmpathEnvQueryTest_Dot : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = Dot)
		bool bUseCamRotInsteadOfPlayerIn360TrackingMode;
protected:
	/** defines direction of first line used by test */
	UPROPERTY(EditDefaultsOnly, Category = Dot)
		FEnvDirection LineA;

	/** defines direction of second line used by test */
	UPROPERTY(EditDefaultsOnly, Category = Dot)
		FEnvDirection LineB;

	UPROPERTY(EditDefaultsOnly, Category = Dot)
		EEnvTestDot TestMode;

	/** If true, this test uses the absolute value of the dot product rather than the dot product itself.  Useful
	* when you want to compare "how lateral" something is.  I.E. values closer to zero are further to the side,
	* and values closer to 1 are more in front or behind (without distinguishing forward/backward).
	*/
	UPROPERTY(EditDefaultsOnly, Category = Dot)
		bool bAbsoluteValue;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	/** helper function: gather directions from context pairs */
	void GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance, const FVector& ItemLocation,
		TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo) const;

	/** helper function: gather directions from context */
	virtual void GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance, const FRotator& ItemRotation,
		TSubclassOf<UEnvQueryContext> LineDirection) const;

	/** helper function: gather directions from proper contexts */
	void GatherLineDirections(TArray<FVector>& Directions, FEnvQueryInstance& QueryInstance,
		TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo, TSubclassOf<UEnvQueryContext> LineDirection, bool bUseDirectionContext,
		const FVector& ItemLocation = FVector::ZeroVector, const FRotator& ItemRotation = FRotator::ZeroRotator) const;

	/** helper function: check if contexts are updated per item */
	bool RequiresPerItemUpdates(TSubclassOf<UEnvQueryContext> LineFrom, TSubclassOf<UEnvQueryContext> LineTo, TSubclassOf<UEnvQueryContext> LineDirection, bool bUseDirectionContext) const;
};
