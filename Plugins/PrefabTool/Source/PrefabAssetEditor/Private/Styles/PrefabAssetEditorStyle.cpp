// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "PrefabAssetEditorStyle.h"

#include "IPluginManager.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)


TSharedPtr< FSlateStyleSet > FPrefabAssetEditorStyle::StyleInstance = NULL;

TSharedRef< class FSlateStyleSet > FPrefabAssetEditorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FPrefabAssetEditorStyle());
	return Style;
}

void FPrefabAssetEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FPrefabAssetEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
}

FPrefabAssetEditorStyle::FPrefabAssetEditorStyle()
	: FSlateStyleSet("PrefabAssetEditorStyle")
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(40.0f, 40.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	const FString BaseDir = IPluginManager::Get().FindPlugin("PrefabTool")->GetBaseDir();
	SetContentRoot(BaseDir / TEXT("Resources"));

	// Toolbar icons
	// Set("PrefabAssetEditor.PrefabTool", new IMAGE_BRUSH("Icons/PrefabAssetIcon64", Icon64x64));

	// ClassIcon
	{
		Set("ClassIcon.PrefabActor", new IMAGE_BRUSH(TEXT("Icons/PrefabAssetIcon16"), Icon16x16));
		Set("ClassThumbnail.PrefabActor", new IMAGE_BRUSH(TEXT("Icons/PrefabAssetIcon64"), Icon64x64));

		Set("ClassIcon.PrefabAsset", new IMAGE_BRUSH(TEXT("Icons/PrefabAssetIcon16"), Icon16x16));
		Set("ClassThumbnail.PrefabAsset", new IMAGE_BRUSH(TEXT("Icons/PrefabAssetIcon64"), Icon64x64));
	}

	// Text Style
	{
		Set("PrefabTool.ConnectedText", FTextBlockStyle()
			.SetColorAndOpacity(FLinearColor::White)
		);
		Set("PrefabTool.DisConnectedText", FTextBlockStyle()
			.SetColorAndOpacity(FLinearColor::Gray)
		);
	}
}

/** Destructor. */
FPrefabAssetEditorStyle::~FPrefabAssetEditorStyle()
{
}

const ISlateStyle& FPrefabAssetEditorStyle::Get()
{
	return *StyleInstance;
}


#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
