// Copyright 2017 marynate. All Rights Reserved.

#include "CreateBlueprintFromPrefabActorDialog.h"
#include "PrefabToolHelpers.h"
#include "PrefabActor.h"
#include "PrefabComponent.h"

#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "SCreateAssetFromObject.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintFromPrefabActorDialog"

TWeakObjectPtr<APrefabActor> FCreateBlueprintFromPrefabActorDialog::SourceActor;

void FCreateBlueprintFromPrefabActorDialog::OpenDialog(APrefabActor* InActorOverride )
{
	SourceActor = InActorOverride;

	TSharedPtr<SWindow> PickBlueprintPathWidget;
	SAssignNew(PickBlueprintPathWidget, SWindow)
		.Title(LOCTEXT("SelectPath", "Select Path"))
		.ToolTipText(LOCTEXT("SelectPathTooltip", "Select the path where the Blueprint will be created at"))
		.ClientSize(FVector2D(400, 400));

	TSharedPtr<SCreateAssetFromObject> CreateBlueprintFromActorDialog;
	PickBlueprintPathWidget->SetContent
	(
		SAssignNew(CreateBlueprintFromActorDialog, SCreateAssetFromObject, PickBlueprintPathWidget)
		.AssetFilenameSuffix(TEXT("Blueprint"))
		.HeadingText(LOCTEXT("CreateBlueprintFromActor_Heading", "Blueprint Name"))
		.CreateButtonText(LOCTEXT("CreateBlueprintFromActor_ButtonLabel", "Create Blueprint"))
		.OnCreateAssetAction(FOnPathChosen::CreateStatic(FCreateBlueprintFromPrefabActorDialog::OnCreateBlueprint))
	);

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(PickBlueprintPathWidget.ToSharedRef(), RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(PickBlueprintPathWidget.ToSharedRef());
	}
}

void FCreateBlueprintFromPrefabActorDialog::OnCreateBlueprint(const FString& InAssetPath)
{
	UBlueprint* Blueprint = NULL;

	APrefabActor* ActorToUse = SourceActor.Get();

	if (ActorToUse)
	{
		FCreateBlueprintFromPrefabActor Creator;
		const bool bReplaceActor = true;
		Blueprint = Creator.CreateBlueprint(InAssetPath, ActorToUse, bReplaceActor);
	}

	if(Blueprint)
	{
		// Rename new instance based on the original actor label rather than the asset name
// 		USelection* SelectedActors = GEditor->GetSelectedActors();
// 		if( SelectedActors && SelectedActors->Num() == 1 )
// 		{
// 			AActor* Actor = Cast<AActor>(SelectedActors->GetSelectedObject(0));
// 			if(Actor)
// 			{
// 				FActorLabelUtilities::SetActorLabelUnique(Actor, FPackageName::GetShortName(InAssetPath));
// 			}
// 		}

		ActorToUse->GetPrefabComponent()->GeneratedBlueprint = Blueprint;

		// Select the newly created blueprint in the content browser, but don't activate the browser
		TArray<UObject*> Objects;
		Objects.Add(Blueprint);
		GEditor->SyncBrowserToObjects( Objects, false );
	}
	else
	{
		FNotificationInfo Info( LOCTEXT("CreateBlueprintFromActorFailed", "Unable to create a blueprint from prefab actor.") );
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if ( Notification.IsValid() )
		{
			Notification->SetCompletionState( SNotificationItem::CS_Fail );
		}
	}
}

#undef LOCTEXT_NAMESPACE
