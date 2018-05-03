// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Styling/ISlateStyle.h"

class FLinterStyle
{
public:
	static void Initialize();

	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

	static FName GetStyleSetName();
private:
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);

private:
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};