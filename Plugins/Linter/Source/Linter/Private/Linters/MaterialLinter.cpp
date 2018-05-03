// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/MaterialLinter.h"
#include "LintMessages.h"


bool FMaterialLinter::Lint(const UObject* InObject)
{
	START_LINT();

	Material = CastChecked<UMaterial>(Object);

	bFailedLint |= !PassesBasicAssetChecks();

	//@TODO: Future material checks

	Material = nullptr;

	END_LINT();
}

// Overridden so we can check for the special case of Post Process materials.
bool FMaterialLinter::IsNamedCorrectly()
{
	if (!GetDefault<ULinterSettings>()->bDoAssetNamingConventionChecks)
	{
		return true;
	}

	check(Material);

	const FAssetTypeNameSetting* NameSetting = (Material->MaterialDomain == EMaterialDomain::MD_PostProcess) ? &GetDefault<ULinterSettings>()->PostProcessMaterialSetting : GetDefault<ULinterSettings>()->AssetNameSettings.Find(Object->GetClass());

	// Run prefix and suffix checks using found name settings if they are non-null
	if (NameSetting != nullptr)
	{
		if (!NameSetting->Prefix.IsEmpty())
		{
			if (!Object->GetName().StartsWith(NameSetting->Prefix, ESearchCase::CaseSensitive))
			{
				return ReportError(*Object->GetName(), FText::FormatNamed(Rule_1, TEXT("Correction"), FText::FromString(FString::Printf(TEXT("Should start with '%s'."), *NameSetting->Prefix))));
			}
		}

		if (!NameSetting->Suffix.IsEmpty())
		{
			if (!Object->GetName().EndsWith(NameSetting->Suffix, ESearchCase::CaseSensitive))
			{
				return ReportError(*Object->GetName(), FText::FormatNamed(Rule_1, TEXT("Correction"), FText::FromString(FString::Printf(TEXT("Should end with '%s'."), *NameSetting->Suffix))));
			}
		}
	}

	// If we don't have name settings or passed all name checks, simply return true
	return true;
}
