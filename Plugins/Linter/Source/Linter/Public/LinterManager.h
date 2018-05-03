// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "Linters/LinterBase.h"

class FLinterManager
{
public:
	FLinterManager();
	void RegisterLinters();

	void Lint(class UObject* Object);

	void StartReport();
	void EndReport();
	uint32 GetErrorCount();
	FString GetAndClearReport();
private:
	TMap<class UClass*, TSharedPtr<FLinterBase>> Linters;
	TSharedPtr<FLinterBase> DefaultLinter;
	FString JSONReport;
	uint32 ErrorCount;
};