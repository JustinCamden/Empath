// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "Materials/Material.h"
#include "Linters/LinterBase.h"

class FMaterialLinter : public FLinterBase
{
	virtual bool Lint(const UObject* InObject) override;

	virtual bool IsNamedCorrectly() override;

protected:
	const UMaterial* Material;
};