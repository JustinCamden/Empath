// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathTools/Public/EmpathTools.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

IMPLEMENT_GAME_MODULE(FEmpathToolsModule, EmpathTools);

DEFINE_LOG_CATEGORY(EmpathTools)

#define LOCTEXT_NAMESPACE "EmpathTools"

void FEmpathToolsModule::StartupModule()
{
	UE_LOG(EmpathTools, Warning, TEXT("EmpathTools: Log Started"));
}

void FEmpathToolsModule::ShutdownModule()
{
	UE_LOG(EmpathTools, Warning, TEXT("EmpathTools: Log Ended"));
}

#undef LOCTEXT_NAMESPACE