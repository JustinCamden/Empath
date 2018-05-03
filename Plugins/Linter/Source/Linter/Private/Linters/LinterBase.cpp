// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/LinterBase.h"
#include "Linter.h"
#include "LintMessages.h"

FLinterBase::FLinterBase()
	: PascalCase(TEXT("([A-Z0-9]+[a-z0-9]*)+"))
	, PascalBooleanCase(TEXT("b?([A-Z0-9]+[a-z0-9]*)+"))
	, BadCharacters(TEXT("[^a-zA-Z0-9]"))
{
	ErrorCount = 0;
}

bool FLinterBase::Lint(const UObject* InObject)
{
	START_LINT()
	bFailedLint |= !PassesBasicAssetChecks();
	END_LINT()
}

bool FLinterBase::PassesBasicAssetChecks()
{
	bool bFailedLint = false;
	bFailedLint |= !IsNamedCorrectly();
	bFailedLint |= !HasAGoodPath();
	bFailedLint |= !IsNotATopLevelAsset();
	return !bFailedLint;
}

bool FLinterBase::IsNamedCorrectly()
{
	if (!GetDefault<ULinterSettings>()->bDoAssetNamingConventionChecks)
	{
		return true;
	}

	check(Object);

	const FAssetTypeNameSetting* NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(Object->GetClass());

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

bool FLinterBase::HasAGoodPath()
{
	const ULinterSettings* DefaultSettings = GetDefault<ULinterSettings>();
	if (!DefaultSettings->bDoFolderNameChecks)
	{
		return true;
	}
	check(Object);

	FString PathName = Object->GetPathName();
	TArray<FString> PathElements;
	PathName.ParseIntoArray(PathElements, TEXT("/"), true);

	for (int32 i = 0; i < PathElements.Num() - 1; ++i)
	{

		if (DefaultSettings->bDoFolderNameSpaceCheck)
		{
			// Space check
			PathElements[i].Shrink();
			if (PathElements[i].Contains(TEXT(" ")))
			{
				return ReportError(Object->GetName(), FText::FormatNamed(Rule_2_1_2, TEXT("Correction"), FText::FromString(PathElements[i])));
			}
		}
		
		if (DefaultSettings->bDoFolderNameAssetsRedundantCheck)
		{
			// 'Assets' check
			if (PathElements[i].Equals(TEXT("Assets")))
			{
				return ReportError(Object->GetName(), Rule_2_6_1);
			}
		}

		if (DefaultSettings->bDoFolderNameAssetTypeRedundantCheck)
		{
			// 'Meshes, Textures, Materials' check
			if (PathElements[i].Equals(TEXT("Meshes")) || PathElements[i].Equals(TEXT("Textures")) || PathElements[i].Equals(TEXT("Materials")))
			{
				return ReportError(Object->GetName(), FText::FormatNamed(Rule_2_6_2, TEXT("Correction"), FText::FromString(PathElements[i])));
			}
		}

		if (DefaultSettings->bDoFolderNameUnicodeCheck)
		{
			FRegexMatcher BadCharacterMatcher(BadCharacters, PathElements[i]);
			if (BadCharacterMatcher.FindNext())
			{
				return ReportError(Object->GetName(), FText::FormatNamed(Rule_2_1_3, TEXT("Correction"), FText::FromString(PathElements[i])));
			}
		}

		if (DefaultSettings->bDoFolderNamePascalCaseCheck)
		{
			// PascalCase check
			FRegexMatcher PascalCaseMatcher(PascalCase, PathElements[i]);
			if (PascalCaseMatcher.FindNext())
			{
				// Is full path element conforming?
				if (PascalCaseMatcher.GetMatchBeginning() != 0 || PascalCaseMatcher.GetMatchEnding() != PathElements[i].Len())
				{
					return ReportError(*Object->GetName(), FText::FormatNamed(Rule_2_1_1, TEXT("Correction"), FText::FromString(PathElements[i])));
				}
			}
			else
			{
				return ReportError(*Object->GetName(), FText::FormatNamed(Rule_2_1_1, TEXT("Correction"), FText::FromString(PathElements[i])));
			}
		}

		
	}

	return true;
}

bool FLinterBase::IsNotATopLevelAsset()
{
	if (!GetDefault<ULinterSettings>()->bDoFolderNameChecks || !GetDefault<ULinterSettings>()->bUseTopLevelFolderForAssets)
	{
		return true;
	}

	check(Object);

	const FString PathName = Object->GetPathName();
	int32 LastIndex;
	PathName.FindLastChar(TEXT('/'), LastIndex);

	if (PathName.Left(LastIndex) == TEXT("/Game"))
	{
		return ReportError(*Object->GetName(), Rule_2_2);
	}
	return true;
}

const FRegexPattern* FLinterBase::GetCamelCasePattern()
{
	static FRegexPattern CamelCase(TEXT("([A-Z0-9]+[a-z0-9]*)+"));
	return &CamelCase;
}

const FRegexPattern* FLinterBase::GetBooleanCamelCasePattern()
{
	static FRegexPattern bCamelCase(TEXT("(b?[A-Z0-9]+[a-z0-9]*)+"));
	return &bCamelCase;
}

void FLinterBase::StartReport()
{
	check(Object);

	LintJSON = FString::Printf(TEXT("{\"object\": \"%s\", \"errors\" : ["), *Object->GetFullName());
	ErrorCount = 0;
}

void FLinterBase::EndReport()
{
	LintJSON.RemoveFromEnd(TEXT(","));
	LintJSON += TEXT("]}");
}

uint32 FLinterBase::GetErrorCount()
{
	return ErrorCount;
}

FString FLinterBase::GetAndClearReport()
{
	FString Report = LintJSON;
	LintJSON.Empty();
	ErrorCount = 0;
	return Report;
}

bool FLinterBase::ReportError(const FString& Causer, const FText& Rule)
{
	LintJSON += FString::Printf(TEXT("\"%s\","), *Rule.ToString());
	UE_LOG(LogLinter, Error, TEXT("%s - FAILED - %s"), *Causer, *Rule.ToString());
	ErrorCount += 1;
	return false;
}
