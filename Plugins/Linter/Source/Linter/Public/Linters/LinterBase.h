// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "LinterSettings.h"
#include "Regex.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

#define START_LINT() Object = InObject; StartReport(); bool bFailedLint = false;
#define END_LINT() EndReport(); Object = nullptr; return bFailedLint;

class FLinterBase : public TSharedFromThis<FLinterBase>
{
public:
	FLinterBase();
	virtual ~FLinterBase() {}

	virtual bool Lint(const UObject* InObject);

	virtual bool PassesBasicAssetChecks();
	virtual bool IsNamedCorrectly();
	virtual bool HasAGoodPath();
	virtual bool IsNotATopLevelAsset();

	static const FRegexPattern* GetCamelCasePattern();
	static const FRegexPattern* GetBooleanCamelCasePattern();

	void StartReport();
	void EndReport();
	uint32 GetErrorCount();
	FString GetAndClearReport();

	virtual bool ReportError(const FString& Causer, const FText& Rule);
protected:
	FString LintJSON;
	uint32 ErrorCount;

	const UObject* Object;

	FRegexPattern PascalCase;
	FRegexPattern PascalBooleanCase;
	FRegexPattern BadCharacters;
};
