// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "Particles/ParticleSystem.h"
#include "Linters/LinterBase.h"

class FParticleSystemLinter : public FLinterBase
{
public:
	virtual bool Lint(const UObject* InObject) override;
	virtual bool HasGoodEmitterNames();

protected:
	const UParticleSystem* ParticleSystem;

};