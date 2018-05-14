// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathTeamAgentInterface.h"
#include "Empath.h"

UEmpathTeamAgentInterface::UEmpathTeamAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGenericTeamId IEmpathTeamAgentInterface::GetGenericTeamId() const
{
	EEmpathTeam const Team = const_cast<IEmpathTeamAgentInterface*>(this)->GetTeamNum_Implementation();
	return FGenericTeamId(static_cast<uint8>(Team));
}

ETeamAttitude::Type IEmpathTeamAgentInterface::GetTeamAttitudeTowards(const AActor& Other) const
{
	IEmpathTeamAgentInterface const* TeamActorB = Cast<const IEmpathTeamAgentInterface>(&Other);
	if (TeamActorB == NULL && Other.Instigator)
	{
		TeamActorB = Cast<const IEmpathTeamAgentInterface>(Other.Instigator);
	}

	if (TeamActorB)
	{
		// This is a bit dirty, but we can't make GetTeamNum() const if it is a BlueprintNativeEvent unfortunately
		const EEmpathTeam TeamA = const_cast<IEmpathTeamAgentInterface*>(this)->GetTeamNum_Implementation();
		const EEmpathTeam TeamB = const_cast<IEmpathTeamAgentInterface*>(TeamActorB)->GetTeamNum_Implementation();

		// If either team is neutral, then we don't care about them
		if (TeamA == EEmpathTeam::Neutral || TeamB == EEmpathTeam::Neutral)
		{
			return ETeamAttitude::Neutral;
		}

		return (TeamA == TeamB) ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}

	return ETeamAttitude::Neutral;
}