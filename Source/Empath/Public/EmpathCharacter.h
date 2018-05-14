// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTypes.h"
#include "GameFramework/Character.h"
#include "EmpathCharacter.generated.h"

UCLASS()
class EMPATH_API AEmpathCharacter : public ACharacter
{
	GENERATED_BODY()

public:	
	// Sets default values for this character's properties
	AEmpathCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Empath|Health")
	bool bDead;

	/** The maximum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float MaxEffectiveDistance;

	/** Wrapper for Get Distance To function that accounts for VR characters. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility")
	float GetDistanceToVR(const AActor* OtherActor) const;
	
	/** Called when the behavior tree has begun running. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|NavRecoveryState", meta = (DisplayName = "OnAIInitialized"))
	void ReceiveAIInitalized();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
