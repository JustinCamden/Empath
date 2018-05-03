// Copyright 2017 marynate. All Rights Reserved.

#include "SPrefabAssetEditor.h"
#include "PrefabAsset.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SPrefabAssetEditor"

SPrefabAssetEditor::~SPrefabAssetEditor()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void SPrefabAssetEditor::Construct(const FArguments& InArgs, UPrefabAsset* InPrefabAsset, const TSharedRef<ISlateStyle>& InStyle)
{
	PrefabAsset = InPrefabAsset;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(EditableTextBox, SMultiLineEditableTextBox)
					.OnTextChanged(this, &SPrefabAssetEditor::HandleEditableTextBoxTextChanged)
					.OnTextCommitted(this, &SPrefabAssetEditor::HandleEditableTextBoxTextCommitted)
					.Text(PrefabAsset->PrefabContent)
			]
	];

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SPrefabAssetEditor::HandlePrefabAssetPropertyChanged);
}

void SPrefabAssetEditor::HandleEditableTextBoxTextChanged(const FText& NewText)
{
	PrefabAsset->MarkPackageDirty();
}

void SPrefabAssetEditor::HandleEditableTextBoxTextCommitted(const FText& Comment, ETextCommit::Type CommitType)
{
	PrefabAsset->SetPrefabContent(EditableTextBox->GetText());
}

void SPrefabAssetEditor::HandlePrefabAssetPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object == PrefabAsset)
	{
		EditableTextBox->SetText(PrefabAsset->PrefabContent);
	}
}


#undef LOCTEXT_NAMESPACE
