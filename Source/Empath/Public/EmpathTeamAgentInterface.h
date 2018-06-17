// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "EmpathTypes.h"
#include "EmpathTeamAgentInterface.generated.h"

/** Interface used so characters and objects can be placed on teams and recognize teammates and enemies. */
UINTERFACE()
class UEmpathTeamAgentInterface : public UGenericTeamAgentInterface
{
	GENERATED_UINTERFACE_BODY()
};

class EMPATH_API IEmpathTeamAgentInterface : public IGenericTeamAgentInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Returns the team number of the actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = EmpathTeamAgentInterface)
	EEmpathTeam GetTeamNum() const;

	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};

