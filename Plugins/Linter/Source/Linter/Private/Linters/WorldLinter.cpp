// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/WorldLinter.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Logging/MessageLog.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LintMessages.h"

bool FWorldLinter::Lint(const UObject* InObject)
{
	START_LINT();

	World = const_cast<UWorld*>(CastChecked<UWorld>(Object));

	bFailedLint |= !PassesBasicAssetChecks();
	bFailedLint |= !HasNoErrorsOrWarnings();

	World = nullptr;

	END_LINT();
}

bool FWorldLinter::HasAGoodPath()
{
	if (!GetDefault<ULinterSettings>()->bDoFolderNameChecks)
	{
		return true;
	}

	if (FLinterBase::HasAGoodPath())
	{
		if (!GetDefault<ULinterSettings>()->bMapsInMapsFolder)
		{
			return true;
		}

		FString PathName = Object->GetPathName();
		if (PathName.Contains(TEXT("/Maps/"), ESearchCase::CaseSensitive))
		{
			return true;
		}
		
		return ReportError(*Object->GetName(), Rule_2_4);
	}
	
	return false;
}

bool FWorldLinter::HasNoErrorsOrWarnings()
{
	check(World && GEditor);

	if (!GetDefault<ULinterSettings>()->bLevelsShouldHaveNoErrorsOrWarnings)
	{
		return true;
	}

	bool bHasErrorsOrWarnings = false;

	// Setup the world.
	World->WorldType = EWorldType::Editor;
	World->AddToRoot();
	if (!World->bIsWorldInitialized)
	{
		UWorld::InitializationValues IVS;
		IVS.RequiresHitProxies(false);
		IVS.ShouldSimulatePhysics(false);
		IVS.EnableTraceCollision(false);
		IVS.CreateNavigation(false);
		IVS.CreateAISystem(false);
		IVS.AllowAudioPlayback(false);
		IVS.CreatePhysicsScene(true);

		World->InitWorld(IVS);
		World->PersistentLevel->UpdateModelComponents();
		World->UpdateWorldComponents(true, false);
	}
	FWorldContext &WorldContext = GEditor->GetEditorWorldContext(true);
	WorldContext.SetCurrentWorld(World);
	GWorld = World;

#if WITH_EDITORONLY_DATA
	// Camera actors don't set their camera mesh when running in a commandlet
	// This causes their Static Mesh Component to trigger a null static mesh map error
	// So we go ahead and set the camera meshes to avoid this error
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		ACameraActor* Camera = CastChecked<ACameraActor>(Actor);
		static UStaticMesh* CameraMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"));
		Camera->GetCameraComponent()->SetCameraMesh(CameraMesh);
	}
#endif

	GEditor->Exec(World, TEXT("MAP CHECK"));
	if (FMessageLog("MapCheck").NumMessages(EMessageSeverity::Warning) > 0 || World->NumLightingUnbuiltObjects > 0)
	{
		bHasErrorsOrWarnings = true;
		ReportError(Object->GetName(), Rule_6_1);
	}

	World->RemoveFromRoot();
	WorldContext.SetCurrentWorld(nullptr);
	GWorld = nullptr;

	return !bHasErrorsOrWarnings;
}
