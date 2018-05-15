// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTypes.h"
#include "EmpathTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "EmpathCharacter.generated.h"

class AEmpathAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDeathDelegate);

UCLASS()
class EMPATH_API AEmpathCharacter : public ACharacter, public IEmpathTeamAgentInterface
{
	GENERATED_BODY()

public:	
	// Sets default values for this character's properties
	AEmpathCharacter(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	EEmpathTeam GetTeamNum() const;

	/** Returns the controlling Empath AI controller. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Health")
	AEmpathAIController* GetEmpathAICon();

	UPROPERTY(BlueprintReadOnly, Category = "Empath|Health")
	bool bDead;

	/** The maximum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float MaxEffectiveDistance;

	/** The minimum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float MinEffectiveDistance;

	/** Wrapper for Get Distance To function that accounts for VR characters. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility")
	float GetDistanceToVR(const AActor* OtherActor) const;
	
	/** Called when the behavior tree has begun running. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnAIInitialized"))
	void ReceiveAIInitalized();

	/** Called when character's health depletes to 0. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Health")
	void Die();

	/** Called when character's health depletes to 0. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health", meta = (DisplayName = "Die"))
	void ReceiveDie();

	/** Called when character's health depletes to 0. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Health")
	FOnCharacterDeathDelegate OnDeath;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Default team of this character if it is uncontrolled by a controller (The controller's team takes precendence). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Team)
	EEmpathTeam DefaultTeam;
private:
	/** Stored reference to our control Empath AI controller */
	AEmpathAIController* CachedEmpathAICon;
};