// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavArea_Jump.h"
#include "EmpathTypes.h"

UEmpathNavArea_Jump::UEmpathNavArea_Jump()
{
	DrawColor = FColor(0, 255, 128);
	AreaFlags |= (EmpathNavAreaFlags::Jump | EmpathNavAreaFlags::Fly);
}


