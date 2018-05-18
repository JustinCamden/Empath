// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTypes.h"
#include "EmpathTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "EmpathCharacter.generated.h"

class AEmpathAIController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDeathDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterStunnedDelegate, float, StunDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterStunEndDelegate);

UCLASS()
class EMPATH_API AEmpathCharacter : public ACharacter, public IEmpathTeamAgentInterface
{
	GENERATED_BODY()

public:	

	// Sets default values for this character's properties
	AEmpathCharacter(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Override for Take Damage that calls our own custom Process Damage script (Since we can't override the OnAnyDamage event in c++)
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	EEmpathTeam GetTeamNum() const;

	// ---------------------------------------------------------
	//	Setters and getters

	/** Returns the controlling Empath AI controller. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	AEmpathAIController* GetEmpathAICon();

	/** Wrapper for Get Distance To function that accounts for VR characters. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility")
	float GetDistanceToVR(const AActor* OtherActor) const;

	/** Overridable function for whether the character can currently die. By default, only true if not Invincible and not Dead. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	bool CanDie();

	/** Whether the character is currently dead. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Health")
	bool IsDead() const { return bDead; }

	/** Whether the character is currently stunned. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
	bool IsStunned() const { return bStunned; }

	/** Overridable function for whether the character can be stunned. By default, only true if IsStunnable, and not Stunned or Dead. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat")
	bool CanBeStunned();

	// ---------------------------------------------------------
	//	Events and receives

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

	/** Called when character becomes stunned. If StunDuration <= 0, then stun must be removed by EndStun(). */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
	void BeStunned(float StunDuration = 3.0);

	FTimerHandle StunTimerHandle;

	/** Called when character becomes stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat", meta = (DisplayName = "BeStunned"))
	void ReceiveStunned(float StunDuration);

	/** Called when character becomes stunned. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Combat")
	FOnCharacterStunnedDelegate OnStunned;

	/** Ends the stun effect on the character. Called automatically at the end of stun duration if it was > 0. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
	void EndStun();

	/** Called when character becomes stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat", meta = (DisplayName = "OnStunEnd"))
	void ReceiveStunEnd();

	/** Called when character becomes stunned. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Combat")
	FOnCharacterStunEndDelegate OnStunEnd;

	// ---------------------------------------------------------
	//	Combat

	/** The maximum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float MaxEffectiveDistance;

	/** The minimum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float MinEffectiveDistance;

	/** Whether this character is stunnable in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	bool bStunnable;

	/** How much the character has to accrue the StunDamageThreshold and become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunTimeThreshold;

	/** How much damage needs to be done in the time defined in StunTimeThreshold for the character to become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunDamageThreshold;

	/** How much stun damage the character has accrued in the StunTimeThreshold. */
	UPROPERTY(BlueprintReadWrite, Category = "Empath|Combat")
	float StunDamage;

	/** How long a stun lasts by default. */
	UPROPERTY(BlueprintReadWrite, Category = "Empath|Combat")
	float StunDurationDefault;

	/** History of stun damage that has been applied to this character. */
	TArray<FDamageHistoryEvent, TInlineAllocator<16>> StunDamageHistory;

	/** Checks whether we should become stunned */
	virtual void TakeStunDamage(float StunDamage);

	// ---------------------------------------------------------
	//	Health

	/** The maximum health of the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float MaxHealth;

	/** The current health of the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float CurrentHealth;

	/** Whether this character can take damage or die, in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	bool bInvincible;

	/** Whether this character can take damage from friendly fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	bool bCanTakeFriendlyFire;

	/** Allows us to define weak and strong spots on the character by inflicting different amounts of damage depending on the bone that was hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|Health")
	TArray<FPerBoneDamageScale> PerBoneDamage;

	/** Allows us to define weak and strong spots on the character by inflicting different amounts of damage depending on the bone that was hit. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Health")
	float GetPerBoneDamageScale(FName BoneName) const;

	/** Modifies any damage received from Take Damage calls */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyAnyDamage(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageType);

	/** Modifies any damage from Point Damage calls (Called after ModifyAnyDamage and per bone damage scaling). */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyPointDamage(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Modifies any damage from Radial Damage calls (Called after ModifyAnyDamage). */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyRadialDamage(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Processes final damage after all calculations are complete. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	void ProcessFinalDamage(const float DamageAmount, const UDamageType* DamageType);

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Default team of this character if it is uncontrolled by a controller (The controller's team takes precendence). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Empath|Team")
	EEmpathTeam DefaultTeam;

private:

	/** Whether the character is currently dead. */
	bool bDead;

	/** Whether the character is currently stunned. */
	bool bStunned;

	/** Stored reference to our control Empath AI controller */
	AEmpathAIController* CachedEmpathAICon;
};