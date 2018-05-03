// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "BatchRenameTool.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/TextLayout.h"
#include "SBox.h"
#include "STextBlock.h"
#include "SButton.h"
#include "SlateEnums.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "LinterBatchRenamer"

FDlgBatchRenameTool::FDlgBatchRenameTool()
	: bRemovePrefix(false)
	, bRemoveSuffix(false)
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title(LOCTEXT("BatchRenameToolDlgTitle", "Batch Rename Tool"))
			.SupportsMinimize(false).SupportsMaximize(false)
			.SaneWindowPlacement(true)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.ClientSize(FVector2D(350, 165));

		TSharedPtr<SBorder> DialogWrapper =
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SDlgBatchRenameTool)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

FDlgBatchRenameTool::EResult FDlgBatchRenameTool::ShowModal()
{
	//Show Dialog
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	EResult UserResponse = (EResult)DialogWidget->GetUserResponse();

	if (UserResponse == EResult::Confirm)
	{
		Prefix = DialogWidget->PrefixTextBox->GetText().ToString();
		Suffix = DialogWidget->SuffixTextBox->GetText().ToString();
		bRemovePrefix = DialogWidget->PrefixRemoveBox->IsChecked();
		bRemoveSuffix = DialogWidget->SuffixRemoveBox->IsChecked();

		Find = DialogWidget->FindTextBox->GetText().ToString();
		Replace = DialogWidget->ReplaceTextBox->GetText().ToString();

		// If no information is given, treat as canceled
		if (Prefix.IsEmpty() && Suffix.IsEmpty() && Find.IsEmpty())
		{
			return EResult::Cancel;
		}
	}
	return UserResponse;
}

void SDlgBatchRenameTool::Construct(const FArguments& InArgs)
{
	UserResponse = FDlgBatchRenameTool::Cancel;
	ParentWindow = InArgs._ParentWindow.Get();

	this->ChildSlot[
		SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(48.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BatchRenameToolDlgPrefix", "Prefix"))
					.Justification(ETextJustify::Right)
					]
				]
			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SAssignNew(PrefixTextBox, SEditableTextBox)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(PrefixRemoveBox, SCheckBox)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchRenameToolDlgRemove", "Remove"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(48.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BatchRenameToolDlgSuffix", "Suffix"))
					.Justification(ETextJustify::Right)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SAssignNew(SuffixTextBox, SEditableTextBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(SuffixRemoveBox, SCheckBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchRenameToolDlgRemove", "Remove"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(48.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchRenameToolDlgFind", "Find"))
					.Justification(ETextJustify::Right)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SAssignNew(FindTextBox, SEditableTextBox)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(48.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchRenameToolDlgReplace", "Replace"))
					.Justification(ETextJustify::Right)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SAssignNew(ReplaceTextBox, SEditableTextBox)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SDlgBatchRenameTool::OnButtonClick, FDlgBatchRenameTool::Confirm)
				.Text(LOCTEXT("SkeletonMergeOk", "OK"))
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SDlgBatchRenameTool::OnButtonClick, FDlgBatchRenameTool::Cancel)
				.Text(LOCTEXT("SkeletonMergeCancel", "Cancel"))
			]
		]
	];
}

FDlgBatchRenameTool::EResult SDlgBatchRenameTool::GetUserResponse() const
{
	return UserResponse;
}

#undef LOCTEXT_NAMESPACE
