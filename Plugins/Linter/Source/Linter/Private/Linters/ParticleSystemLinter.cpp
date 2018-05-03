// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/ParticleSystemLinter.h"
#include "Particles/ParticleEmitter.h"
#include "LintMessages.h"

bool FParticleSystemLinter::HasGoodEmitterNames()
{
	check(ParticleSystem);

	if (!GetDefault<ULinterSettings>()->bParticleSystemEmittersShouldBeNamed)
	{
		return true;
	}

	for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
	{
		if (Emitter->EmitterName.IsEqual(TEXT("Particle Emitter")))
		{
			return ReportError(Object->GetName(), FText::FormatNamed(Rule_5_1, TEXT("Name"), FText::FromString(Object->GetName())));
		}
	}

	return true;
}

bool FParticleSystemLinter::Lint(const UObject* InObject)
{
	START_LINT();

	ParticleSystem = CastChecked<UParticleSystem>(Object);

	bFailedLint |= !PassesBasicAssetChecks();
	bFailedLint |= !HasGoodEmitterNames();

	ParticleSystem = nullptr;

	END_LINT();
}
