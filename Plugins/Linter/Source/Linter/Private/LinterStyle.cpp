// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "LinterStyle.h"
#include "SlateTypes.h"
#include "SlateStyle.h"
#include "IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "EditorStyleSet.h"

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FLinterStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define TTF_CORE_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToCoreContentDir(RelativePath, TEXT(".ttf") ), __VA_ARGS__)

FString FLinterStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Linter"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr< FSlateStyleSet > FLinterStyle::StyleSet = nullptr;
TSharedPtr< class ISlateStyle > FLinterStyle::Get() { return StyleSet; }

FName FLinterStyle::GetStyleSetName()
{
	static FName LinterStyleName(TEXT("LinterStyle"));
	return LinterStyleName;
}

void FLinterStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FTextBlockStyle& NormalText = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	// Asset actions
	{
		StyleSet->Set("AssetActions.RunLinter", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_Texture_Run_16x"), Icon16x16));
		StyleSet->Set("AssetActions.RunDokuwiki", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_Texture_Run_16x"), Icon16x16));
		StyleSet->Set("AssetActions.BatchRename", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_Texture_Run_16x"), Icon16x16));
		StyleSet->Set("AssetActions.TooltipTool", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_Texture_Run_16x"), Icon16x16));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT

void FLinterStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
