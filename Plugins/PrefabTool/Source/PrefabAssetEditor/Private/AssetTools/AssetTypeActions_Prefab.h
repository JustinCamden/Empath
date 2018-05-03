// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

/**
* AssetTypeActions for Prefab
*/
class FAssetTypeActions_Prefab : public FAssetTypeActions_Base
{
public:

	FAssetTypeActions_Prefab(const TSharedRef<ISlateStyle>& InStyle, EAssetTypeCategories::Type InAssetCategory);

public:

	//~ Begin IAssetTypeActions
	virtual bool CanFilter() override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	//~ End IAssetTypeActions

private:	
	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style; 
	EAssetTypeCategories::Type MyAssetCategory;
};
