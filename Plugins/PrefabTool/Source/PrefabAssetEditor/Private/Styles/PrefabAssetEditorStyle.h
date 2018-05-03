// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "IPluginManager.h"

/**
* Editor Style Set for Prefab Actor
*/
class FPrefabAssetEditorStyle : public FSlateStyleSet
{
public:

	static void Initialize();

	static void Shutdown();

	FPrefabAssetEditorStyle();

	~FPrefabAssetEditorStyle();

	static const ISlateStyle& Get();

public:
	static TSharedPtr< class FSlateStyleSet > StyleInstance;

private:
	static TSharedRef< class FSlateStyleSet > Create();
};

