// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/StaticMeshLinter.h"
#include "LintMessages.h"


bool FStaticMeshLinter::Lint(const UObject* InObject)
{
	START_LINT();

	StaticMesh = CastChecked<UStaticMesh>(Object);

	bFailedLint |= !PassesBasicAssetChecks();
	bFailedLint |= !HasValidUVs();

	StaticMesh = nullptr;

	END_LINT();
}

bool FStaticMeshLinter::HasValidUVs()
{
	check (StaticMesh);

	if (!GetDefault<ULinterSettings>()->bStaticMeshValidUVs)
	{
		return true;
	}

	bool bHadErrors = false;

	TArray<FString> MissingUVs;
	TArray<FString> BadUVs;
	TArray<FString> ValidUVs;

	UStaticMesh::CheckLightMapUVs(const_cast<UStaticMesh*>(StaticMesh), MissingUVs, BadUVs, ValidUVs, true);

	if (MissingUVs.Num() > 0)
	{
		ReportError(StaticMesh->GetName(), Rule_4_1_1);
		bHadErrors = true;
	}

	if (BadUVs.Num() > 0)
	{
		ReportError(StaticMesh->GetName(), Rule_4_1_2);
		bHadErrors = true;
	}

	return !bHadErrors;
}
