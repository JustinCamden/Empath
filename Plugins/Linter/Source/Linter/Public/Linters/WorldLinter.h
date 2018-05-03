// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Linters/LinterBase.h"

class FWorldLinter : public FLinterBase
{
public:
	virtual bool Lint(const UObject* InObject) override;

	virtual bool HasAGoodPath() override;

	virtual bool HasNoErrorsOrWarnings();

protected:
	UWorld* World;
	
};