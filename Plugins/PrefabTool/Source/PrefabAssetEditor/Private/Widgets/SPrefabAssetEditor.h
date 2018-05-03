// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "SlateCore.h"

/**
* Prefab Asset editor for viewing and editing prefab asset
*/
class SPrefabAssetEditor : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPrefabAssetEditor) { }
	SLATE_END_ARGS()

public:

	virtual ~SPrefabAssetEditor();

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InPrefabAsset The UPrefabAsset asset to edit.
	 * @param InStyleSet The style set to use.
	 */
	void Construct(const FArguments& InArgs, class UPrefabAsset* InPrefabAsset, const TSharedRef<ISlateStyle>& InStyle);

private:

	void HandleEditableTextBoxTextChanged(const FText& NewText);

	void HandleEditableTextBoxTextCommitted(const FText& Comment, ETextCommit::Type CommitType);

	void HandlePrefabAssetPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

private:

	TSharedPtr<class SMultiLineEditableTextBox> EditableTextBox;

	UPrefabAsset* PrefabAsset;
};
