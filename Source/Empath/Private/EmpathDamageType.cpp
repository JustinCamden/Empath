// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathDamageType.h"

UEmpathDamageType::UEmpathDamageType()
	: Super()
{
	StunDamageMultiplier = 1.0f;
	FriendlyFireDamageMultiplier = 1.0f;
	DeathImpulse = 800.0f;
	DeathImpulseUpkick = 800.0f;
	bIgnorePerBoneDamageScaling = false;
}




