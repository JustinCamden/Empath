// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavArea_Fly.h"
#include "EmpathTypes.h"

UEmpathNavArea_Fly::UEmpathNavArea_Fly()
{
	DrawColor = FColor(128, 255, 0);
	AreaFlags |= EmpathNavAreaFlags::Fly;
}