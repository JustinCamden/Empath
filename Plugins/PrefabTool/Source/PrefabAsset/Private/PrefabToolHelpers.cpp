// Copyright 2017 marynate. All Rights Reserved.

#if WITH_EDITOR

#include "PrefabToolHelpers.h"
#include "PrefabToolSettings.h"
#include "PrefabActor.h"
#include "PrefabComponent.h"
#include "PrefabAsset.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "Misc/ScopedSlowTask.h"
#include "EngineUtils.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Misc/FeedbackContext.h"
#include "Components/BrushComponent.h"
#include "GameFramework/DefaultPhysicsVolume.h"

#include "BlueprintEditorUtils.h"
#include "Engine/InheritableComponentHandler.h"
#include "K2Node_AddComponent.h"
#include "BusyCursor.h"
#include "Layers/ILayers.h"
#include "Editor/EditorEngine.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "LevelUtils.h"
#include "ScopedTransaction.h"
#include "BSPOps.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "ActorEditorUtils.h"
#include "Logging/MessageLog.h"
#include "Editor/GroupActor.h"
#include "ActorGroupingUtils.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "ReferencedAssetsUtils.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Landscape.h"
#include "LandscapeGizmoActiveActor.h"

#include "Engine/LevelStreaming.h"
#include "UObject/PropertyPortFlags.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "ComponentAssetBroker.h"
#include "AssetRegistryModule.h"
#include "Toolkits/AssetEditorManager.h"

#include "Engine/StreamableManager.h"

#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"

#include "InstancedReferenceSubobjectHelper.h"
#include "UnrealExporter.h"

DEFINE_LOG_CATEGORY(LogPrefabTool);

#define LOCTEXT_NAMESPACE "PrefabToolHelpers"

////////////////////////////////////////////
// FPrefabPropertyUtil
//

struct FPrefabPropertyUtil
{
	static bool IsPropertyValueSpecified(const TCHAR* Buffer)
	{
		return Buffer && *Buffer && *Buffer != TCHAR(',') && *Buffer != TCHAR(')');
	}

	static void SkipWhitespace(const TCHAR*& Str)
	{
		while (FChar::IsWhitespace(*Str))
		{
			Str++;
		}
	}

	static void SkipAfterEqual(const TCHAR*& Str)
	{
		while (*Str && (*Str != '='))
		{
			Str++;
		}
		if (*Str && (*Str == '='))
		{
			Str++;
		}
		SkipWhitespace(Str);
	}

	static bool GetAssetPath(const FString& AssetPathLine, FString* OutAssetPath)
	{
		FText OutReason;

		FString ClassName;
		FString ObjectPath;
		if (FPrefabParser::ParseExportTextPath(AssetPathLine, &ClassName, &ObjectPath))
		{
			if (!ClassName.IsEmpty())
			{
				if (ClassName.Len() > NAME_SIZE || !FName(*ClassName).IsValidObjectName(OutReason))
				{
					return false;
				}
			}
			if (!ObjectPath.IsEmpty())
			{
				FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
				FString ObjectName = FPackageName::ObjectPathToObjectName(ObjectPath);

				if (FPackageName::IsValidLongPackageName(PackageName, /*bIncludeReadOnlyRoots*/ false, &OutReason)
					&& FName(*ObjectName).IsValidObjectName(OutReason))
				{
					*OutAssetPath = ObjectPath;
					return true;
				}
			}
		}
		return false;
	}

	static bool IsValidAssetPath(const FString& AssetPathLine)
	{
		FString ObjectPath;
		if (GetAssetPath(AssetPathLine, &ObjectPath))
		{
			return !ObjectPath.IsEmpty();
		}
		return false;
	}

	static bool GetBlueprintAssetPathFromArchetype(const FString& ArchetypeName, FString& BlueprintAssetFullPath)
	{
		FString ObjectClass;
		FString ObjectPath;
		if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
		{
			FString PackgePath = ObjectPath;
			FPackageName::TryConvertFilenameToLongPackageName(ObjectPath, PackgePath);

			int32 DotPosition = PackgePath.Find(TEXT("."), ESearchCase::CaseSensitive);
			if (DotPosition == INDEX_NONE)
			{
				const int32 LastSlashIdx = PackgePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				FString PackageNameAsObjectName = PackgePath.Mid(LastSlashIdx + 1);
				PackgePath += FString::Printf(TEXT(".%s"), *PackageNameAsObjectName);
			}
			BlueprintAssetFullPath = FString::Printf(TEXT("Blueprint'%s'"), *PackgePath);
			PREFABTOOL_LOG(Display, TEXT("[GetBlueprintAssetPathFromArchetype] Archetype=%s, ObjectPath=%s, PackagePath=%s, BlueprintAssetFullPath=%s"), *ArchetypeName, *ObjectPath, *PackgePath, *BlueprintAssetFullPath);

			FSoftObjectPath BlueprintAssetReference(BlueprintAssetFullPath);
			return BlueprintAssetReference.IsValid();
		}
		return false;
	}

	static UClass* GetRemappedBlueprintClass(const FString& OldBlueprintAssetpath, const TMap<FString, FSoftObjectPath>& InAssetRemapper)
	{
		UClass* BlueprintClass = NULL;

		FSoftObjectPath OldAssetReference(OldBlueprintAssetpath);
		if (const FSoftObjectPath* RemappedBlueprintRef = InAssetRemapper.Find(OldAssetReference.ToString()))
		{
			const FSoftObjectPath& NewBlueprintRef = *RemappedBlueprintRef;
			if (NewBlueprintRef.IsValid())
			{
				if (UObject* NewAsset = NewBlueprintRef.TryLoad())
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(NewAsset);
					if (Blueprint)
					{
						PREFABTOOL_LOG(Display, TEXT("[GetRemappedBlueprintClass] from {%s} to {%s}"), *OldAssetReference.ToString(), *NewBlueprintRef.ToString());
						BlueprintClass = Blueprint->GeneratedClass;
					}
				}
			}
		}

		return BlueprintClass;
	}

	static UClass* GetBlueprintClassFromArchetype(const FString& ArchetypeName, const TMap<FString, FSoftObjectPath>& InAssetRemapper)
	{
		UClass* BlueprintClass = NULL;

		FString BlueprintAssetPath;
		if (GetBlueprintAssetPathFromArchetype(ArchetypeName, BlueprintAssetPath))
		{
			if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(LoadObject<UBlueprint>(nullptr, *BlueprintAssetPath)))
			{
				BlueprintClass = BlueprintAsset->GeneratedClass;
				PREFABTOOL_LOG(Display, TEXT("[GetBlueprintClassFromArchetype] Found Blueprint Class %s, from Archetype: %s"), *BlueprintClass->GetName(), *BlueprintAssetPath);
			}
			else
			{
				BlueprintClass = GetRemappedBlueprintClass(BlueprintAssetPath, InAssetRemapper);
			}
		}
		return BlueprintClass;
	}

	static AActor* TryLoadArchetype(const FString& ArchetypeName, const TMap<FString, FSoftObjectPath>& InAssetRemapper)
	{
		AActor* Archetype = NULL;

		// if given a name, break it up along the ' so separate the class from the name
		FString ObjectClass;
		FString ObjectPath;
		if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
		{
			// find the class
			UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);

			const bool bBlueprintClass = ObjectClass.EndsWith(TEXT("_C"));
			if (!ArchetypeClass && bBlueprintClass)
			{
				ArchetypeClass = GetBlueprintClassFromArchetype(ArchetypeName, InAssetRemapper);
			}

			if (ArchetypeClass)
			{
				if (ArchetypeClass->IsChildOf(AActor::StaticClass()))
				{
					// if we had the class, find the archetype
					Archetype = Cast<AActor>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
				}
				else
				{
					PREFABTOOL_LOG(Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"), *ArchetypeName, *ObjectClass);
				}
			}
		}

		return Archetype;
	}
};

class FDefaultPropertiesContextSupplier : public FContextSupplier
{
public:
	/** the current line number */
	int32 CurrentLine;

	/** the package we're processing */
	FString PackageName;

	/** the class we're processing */
	FString ClassName;

	FString GetContext()
	{
		return FString::Printf
		(
			TEXT("%sDevelopment/Src/%s/Classes/%s.h(%i)"),
			*FPaths::RootDir(),
			*PackageName,
			*ClassName,
			CurrentLine
		);
	}

	FDefaultPropertiesContextSupplier() {}
	FDefaultPropertiesContextSupplier(const TCHAR* Package, const TCHAR* Class, int32 StartingLine)
		: CurrentLine(StartingLine), PackageName(Package), ClassName(Class)
	{
	}

};

static FDefaultPropertiesContextSupplier* ContextSupplier = NULL;

////////////////////////////////////////////
// FPrefabGEditorAdapter
//

bool FPrefabGEditorAdapter::EditorDestroyActor(AActor* Actor, bool bShouldModifyLevel)
	{
		bool bResult = false;
		if (Actor && !Actor->IsPendingKillPending())
		{
			GEditor->Layers->DisassociateActorFromLayers(Actor);
			bResult = Actor->GetWorld()->EditorDestroyActor(Actor, bShouldModifyLevel);
		}
		return bResult;
	}

bool  FPrefabGEditorAdapter::GEditor_CanParentActors(const AActor* ParentActor, const AActor* ChildActor, FText* ReasonText)
	{
		if (ChildActor == NULL || ParentActor == NULL)
		{
			if (ReasonText)
			{
				*ReasonText = NSLOCTEXT("ActorAttachmentError", "Null_ActorAttachmentError", "Cannot attach NULL actors.");
			}
			return false;
		}

		if (ChildActor == ParentActor)
		{
			if (ReasonText)
			{
				*ReasonText = NSLOCTEXT("ActorAttachmentError", "Self_ActorAttachmentError", "Cannot attach actor to self.");
			}
			return false;
		}

		USceneComponent* ChildRoot = ChildActor->GetRootComponent();
		USceneComponent* ParentRoot = ParentActor->GetDefaultAttachComponent();
		if (ChildRoot == NULL || ParentRoot == NULL)
		{
			if (ReasonText)
			{
				*ReasonText = NSLOCTEXT("ActorAttachmentError", "MissingComponent_ActorAttachmentError", "Cannot attach actors without root components.");
			}
			return false;
		}

		const bool bForceBSPAttach = true;

		if (!bForceBSPAttach)
		{
			const ABrush* ParentBrush = Cast<const  ABrush >(ParentActor);
			const ABrush* ChildBrush = Cast<const  ABrush >(ChildActor);
			if ((ParentBrush && !ParentBrush->IsVolumeBrush()) || (ChildBrush && !ChildBrush->IsVolumeBrush()))
			{
				if (ReasonText)
				{
					*ReasonText = NSLOCTEXT("ActorAttachmentError", "Brush_ActorAttachmentError", "BSP Brushes cannot be attached");
				}
				return false;
			}
		}

		{
			FText Reason;
			if (!ChildActor->EditorCanAttachTo(ParentActor, Reason))
			{
				if (ReasonText)
				{
					if (Reason.IsEmpty())
					{
						*ReasonText = FText::Format(NSLOCTEXT("ActorAttachmentError", "CannotBeAttached_ActorAttachmentError", "{0} cannot be attached to {1}"), FText::FromString(ChildActor->GetActorLabel()), FText::FromString(ParentActor->GetActorLabel()));
					}
					else
					{
						*ReasonText = MoveTemp(Reason);
					}
				}
				return false;
			}
		}

		if (ChildRoot->Mobility == EComponentMobility::Static && ParentRoot->Mobility != EComponentMobility::Static)
		{
			if (ReasonText)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("StaticActor"), FText::FromString(ChildActor->GetActorLabel()));
				Arguments.Add(TEXT("DynamicActor"), FText::FromString(ParentActor->GetActorLabel()));
				*ReasonText = FText::Format(NSLOCTEXT("ActorAttachmentError", "StaticDynamic_ActorAttachmentError", "Cannot attach static actor {StaticActor} to dynamic actor {DynamicActor}."), Arguments);
			}
			return false;
		}

		if (ChildActor->GetLevel() != ParentActor->GetLevel())
		{
			if (ReasonText)
			{
				*ReasonText = NSLOCTEXT("ActorAttachmentError", "WrongLevel_AttachmentError", "Actors need to be in the same level!");
			}
			return false;
		}

		if (ParentRoot->IsAttachedTo(ChildRoot))
		{
			if (ReasonText)
			{
				*ReasonText = NSLOCTEXT("ActorAttachmentError", "CircularInsest_ActorAttachmentError", "Parent cannot become the child of their descendant");
			}
			return false;
		}

		return true;
	}

void  FPrefabGEditorAdapter::edactPasteSelected(UWorld* InWorld, bool bDuplicate, bool bOffsetLocations, bool bWarnIfHidden, FString* SourceData, const TMap<FString, FSoftObjectPath>& AssetRemapper, TArray<AActor*>& OutSpawnActors, TArray<AGroupActor*>* OutGroupActorsPtr, EObjectFlags InObjectFlags)
{
	if (!SourceData)
	{
		return;
	}

	//check and warn if the user is trying to paste to a hidden level. This will return if he wishes to abort the process
	if (bWarnIfHidden && GUnrealEd->WarnIfDestinationLevelIsHidden(InWorld) == true)
	{
		return;
	}

	{
		const FScopedBusyCursor BusyCursor;

		struct Local
		{
			static FVector CreateLocationOffset(bool bInDuplicate, bool bInOffsetLocations)
			{
				const float Offset = static_cast<float>(bInOffsetLocations ? GEditor->GetGridSize() : 0);
				FVector LocationOffset(Offset, Offset, Offset);
				if (bInDuplicate && GCurrentLevelEditingViewportClient)
				{
					switch (GCurrentLevelEditingViewportClient->ViewportType)
					{
					case LVT_OrthoXZ:
						LocationOffset = FVector(Offset, 0.f, Offset);
						break;
					case LVT_OrthoYZ:
						LocationOffset = FVector(0.f, Offset, Offset);
						break;
					default:
						LocationOffset = FVector(Offset, Offset, 0.f);
						break;
					}
				}
				return LocationOffset;
			}

		};

		// Create a location offset.
		const FVector LocationOffset = Local::CreateLocationOffset(bDuplicate, bOffsetLocations);

		//			FCachedActorLabels ActorLabels(InWorld);

		// Transact the current selection set.
		USelection* SelectedActors = GEditor->GetSelectedActors();
		SelectedActors->Modify();

		// Get pasted text.
		FString PasteString = *SourceData; //FPlatformMisc::ClipboardPaste(PasteString);
		const TCHAR* Paste = *PasteString;

		// Import the actors.
		FPrefabGEditorAdapter::LevelFactory_FactoryCreateText(ULevel::StaticClass(), InWorld->GetCurrentLevel(), InWorld->GetCurrentLevel()->GetFName(), InObjectFlags, NULL, bDuplicate ? TEXT("move") : TEXT("paste"), Paste, Paste + FCString::Strlen(Paste), GWarn, AssetRemapper, OutSpawnActors, OutGroupActorsPtr);

		// Update the actors' locations and update the global list of visible layers.
		for (AActor* Actor : OutSpawnActors)
		{
			if (Actor->IsA(AGroupActor::StaticClass()))
			{
				continue;
			}

			checkSlow(Actor->IsA(AActor::StaticClass()));

			// We only want to offset the location if this actor is the root of a selected attachment hierarchy
			// Offsetting children of an attachment hierarchy would cause them to drift away from the node they're attached to
			// as the offset would effectively get applied twice
			const AActor* const ParentActor = Actor->GetAttachParentActor();
			const FVector& ActorLocationOffset = (ParentActor && ParentActor->IsSelected()) ? FVector::ZeroVector : LocationOffset;

			// Offset the actor's location.
			Actor->TeleportTo(Actor->GetActorLocation() + ActorLocationOffset, Actor->GetActorRotation(), false, true);

			// Re-label duplicated actors so that labels become unique
			const bool bUniqueLabel = false;
			if (bUniqueLabel)
			{
				FActorLabelUtilities::SetActorLabelUnique(Actor, Actor->GetActorLabel(), nullptr);
			}

			GEditor->Layers->InitializeNewActorLayers(Actor);

			// Ensure any layers this actor belongs to are visible
			GEditor->Layers->SetLayersVisibility(Actor->Layers, true);

			Actor->CheckDefaultSubobjects();
			Actor->InvalidateLightingCache();
			// Call PostEditMove to update components, etc.
			Actor->PostEditMove(true);

			APrefabActor::SetSuppressPostDuplicate(true);
			Actor->PostDuplicate(EDuplicateMode::Normal);
			APrefabActor::SetSuppressPostDuplicate(false);

			Actor->CheckDefaultSubobjects();

			// Request saves/refreshes.
			Actor->MarkPackageDirty();
		}
	}
}

//@ULevelFactory
UObject*  FPrefabGEditorAdapter::LevelFactory_FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, const TCHAR* InType, const TCHAR*& InBuffer, const TCHAR* InBufferEnd, FFeedbackContext* InWarn, const TMap<FString, FSoftObjectPath>& InAssetRemapper, TArray<AActor*>& OutSpawnActors, TArray<AGroupActor*>* OutGroupActorsPtr)
	{
		const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
		const bool bUpdateSelection = false;
		const bool bGroupSupport = false;

		struct Local
		{
			static bool GetBEGIN(const TCHAR** Stream, const TCHAR* Match)
			{
				const TCHAR* Original = *Stream;
				if (FParse::Command(Stream, TEXT("BEGIN")) && FParse::Command(Stream, Match))
					return true;
				*Stream = Original;
				return false;
			}

			static bool GetEND(const TCHAR** Stream, const TCHAR* Match)
			{
				const TCHAR* Original = *Stream;
				if (FParse::Command(Stream, TEXT("END")) && FParse::Command(Stream, Match)) return 1; // Gotten.
				*Stream = Original;
				return false;
			}

			static bool GetFVECTOR(const TCHAR* Stream, FVector& Value)
			{
				int32 NumVects = 0;

				Value = FVector::ZeroVector;

				// Support for old format.
				NumVects += FParse::Value(Stream, TEXT("X="), Value.X);
				NumVects += FParse::Value(Stream, TEXT("Y="), Value.Y);
				NumVects += FParse::Value(Stream, TEXT("Z="), Value.Z);

				// New format.
				if (NumVects == 0)
				{
					Value.X = FCString::Atof(Stream);
					Stream = FCString::Strchr(Stream, ',');
					if (!Stream)
					{
						return 0;
					}

					Stream++;
					Value.Y = FCString::Atof(Stream);
					Stream = FCString::Strchr(Stream, ',');
					if (!Stream)
					{
						return 0;
					}

					Stream++;
					Value.Z = FCString::Atof(Stream);

					NumVects = 3;
				}

				return NumVects == 3;
			}

			static bool GetFVECTOR(const TCHAR*	Stream, const TCHAR* Match, FVector& Value)
			{
				TCHAR Temp[80];
				if (!GetSUBSTRING(Stream, Match, Temp, 80)) return false;
				return GetFVECTOR(Temp, Value);

			}

			static bool GetFROTATOR(const TCHAR*	Stream, FRotator& Rotation, int32 ScaleFactor)
			{
				float	Temp = 0.0;
				int32 	N = 0;

				Rotation = FRotator::ZeroRotator;

				// Old format.
				if (FParse::Value(Stream, TEXT("PITCH="), Temp)) { Rotation.Pitch = Temp * ScaleFactor; N++; }
				if (FParse::Value(Stream, TEXT("YAW="), Temp)) { Rotation.Yaw = Temp * ScaleFactor; N++; }
				if (FParse::Value(Stream, TEXT("ROLL="), Temp)) { Rotation.Roll = Temp * ScaleFactor; N++; }

				// New format.
				if (N == 0)
				{
					Rotation.Pitch = FCString::Atof(Stream) * ScaleFactor;
					Stream = FCString::Strchr(Stream, ',');
					if (!Stream)
					{
						return false;
					}

					Rotation.Yaw = FCString::Atof(++Stream) * ScaleFactor;
					Stream = FCString::Strchr(Stream, ',');
					if (!Stream)
					{
						return false;
					}

					Rotation.Roll = FCString::Atof(++Stream) * ScaleFactor;
					return true;
				}

				return (N > 0);
			}

			static bool GetFROTATOR(const TCHAR* Stream, const TCHAR* Match, FRotator& Value, int32 ScaleFactor)
			{
				TCHAR Temp[80];
				if (!GetSUBSTRING(Stream, Match, Temp, 80)) return false;
				return GetFROTATOR(Temp, Value, ScaleFactor);
			}

			static bool GetREMOVE(const TCHAR** Stream, const TCHAR* Match)
			{
				const TCHAR* Original = *Stream;
				if (FParse::Command(Stream, TEXT("REMOVE")) && FParse::Command(Stream, Match))
					return true; // Gotten.
				*Stream = Original;
				return false;
			}

			/** Add a new point to the model (preventing duplicates) and return its index. */
			static int32 AddThing(TArray<FVector>& Vectors, FVector& V, float Thresh, int32 Check)
			{
				if (Check)
				{
					// See if this is very close to an existing point/vector.		
					for (int32 i = 0; i < Vectors.Num(); i++)
					{
						const FVector &TableVect = Vectors[i];
						float Temp = (V.X - TableVect.X);
						if ((Temp > -Thresh) && (Temp < Thresh))
						{
							Temp = (V.Y - TableVect.Y);
							if ((Temp > -Thresh) && (Temp < Thresh))
							{
								Temp = (V.Z - TableVect.Z);
								if ((Temp > -Thresh) && (Temp < Thresh))
								{
									// Found nearly-matching vector.
									return i;
								}
							}
						}
					}
				}
				return Vectors.Add(V);
			}

			/** Add a new point to the model, merging near-duplicates, and return its index. */
			static int32 bspAddPoint(UModel* Model, FVector* V, bool Exact)
			{
				const float Thresh = Exact ? THRESH_POINTS_ARE_SAME : THRESH_POINTS_ARE_NEAR;

				// Try to find a match quickly from the Bsp. This finds all potential matches
				// except for any dissociated from nodes/surfaces during a rebuild.
				FVector Temp;
				int32 pVertex;
				float NearestDist = Model->FindNearestVertex(*V, Temp, Thresh, pVertex);
				if ((NearestDist >= 0.0) && (NearestDist <= Thresh))
				{
					// Found an existing point.
					return pVertex;
				}
				else
				{
					// No match found; add it slowly to find duplicates.
					return AddThing(Model->Points, *V, Thresh, true);
				}
			}

			/** Add a new vector to the model, merging near-duplicates, and return its index. */
			static int32 bspAddVector(UModel* Model, FVector* V, bool Exact)
			{
				const float Thresh = Exact ? THRESH_NORMALS_ARE_SAME : THRESH_VECTORS_ARE_NEAR;

				return AddThing
				(
					Model->Vectors,
					*V,
					Exact ? THRESH_NORMALS_ARE_SAME : THRESH_VECTORS_ARE_NEAR,
					1
				);
			}

			static void bspValidateBrush(UModel* Brush, bool ForceValidate, bool DoStatusUpdate)
			{
				check(Brush != nullptr);
				Brush->Modify();
				if (ForceValidate || !Brush->Linked)
				{
					Brush->Linked = 1;
					for (int32 i = 0; i < Brush->Polys->Element.Num(); i++)
					{
						Brush->Polys->Element[i].iLink = i;
					}
					int32 n = 0;
					for (int32 i = 0; i < Brush->Polys->Element.Num(); i++)
					{
						FPoly* EdPoly = &Brush->Polys->Element[i];
						if (EdPoly->iLink == i)
						{
							for (int32 j = i + 1; j < Brush->Polys->Element.Num(); j++)
							{
								FPoly* OtherPoly = &Brush->Polys->Element[j];
								if
									(OtherPoly->iLink == j
										&&	OtherPoly->Material == EdPoly->Material
										&&	OtherPoly->TextureU == EdPoly->TextureU
										&&	OtherPoly->TextureV == EdPoly->TextureV
										&&	OtherPoly->PolyFlags == EdPoly->PolyFlags
										&& (OtherPoly->Normal | EdPoly->Normal) > 0.9999)
								{
									float Dist = FVector::PointPlaneDist(OtherPoly->Vertices[0], EdPoly->Vertices[0], EdPoly->Normal);
									if (Dist > -0.001 && Dist < 0.001)
									{
										OtherPoly->iLink = i;
										n++;
									}
								}
							}
						}
					}
					// 		UE_LOG(LogBSPOps, Log,  TEXT("BspValidateBrush linked %i of %i polys"), n, Brush->Polys->Element.Num() );
				}

				// Build bounds.
				Brush->BuildBound();
			}

			static void RemapProperty(UProperty* Property, int32 Index, const TMap<AActor*, AActor*>& ActorRemapper, uint8* DestData)
			{
				if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
				{
					// If there's a concrete index, use that, otherwise iterate all array members (for the case that this property is inside a struct, or there is exactly one element)
					const int32 Num = (Index == INDEX_NONE) ? ObjectProperty->ArrayDim : 1;
					const int32 StartIndex = (Index == INDEX_NONE) ? 0 : Index;
					for (int32 Count = 0; Count < Num; Count++)
					{
						uint8* PropertyAddr = ObjectProperty->ContainerPtrToValuePtr<uint8>(DestData, StartIndex + Count);
						AActor* Actor = Cast<AActor>(ObjectProperty->GetObjectPropertyValue(PropertyAddr));
						if (Actor)
						{
							AActor* const* RemappedObject = ActorRemapper.Find(Actor);
							if (RemappedObject && (*RemappedObject)->GetClass()->IsChildOf(ObjectProperty->PropertyClass))
							{
								ObjectProperty->SetObjectPropertyValue(PropertyAddr, *RemappedObject);
							}
						}

					}
				}
				else if (UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property))
				{
					// If there's a concrete index, use that, otherwise iterate all array members (for the case that this property is inside a struct, or there is exactly one element)
					const int32 Num = (Index == INDEX_NONE) ? WeakObjectProperty->ArrayDim : 1;
					const int32 StartIndex = (Index == INDEX_NONE) ? 0 : Index;
					for (int32 Count = 0; Count < Num; Count++)
					{
						uint8* PropertyAddr = WeakObjectProperty->ContainerPtrToValuePtr<uint8>(DestData, StartIndex + Count);
						AActor* Actor = Cast<AActor>(WeakObjectProperty->GetObjectPropertyValue(PropertyAddr));
						if (Actor)
						{
							PREFABTOOL_LOG(Display, TEXT("[PrefabTool.RemapProperty]: WeakObjectProperty.Actor: %d : %s"), Count, *Actor->GetFName().ToString());

							AActor* const* RemappedObject = ActorRemapper.Find(Actor);
							if (RemappedObject && (*RemappedObject)->GetClass()->IsChildOf(WeakObjectProperty->PropertyClass))
							{
								PREFABTOOL_LOG(Display, TEXT("   => Remap to %s"), *(*RemappedObject)->GetFName().ToString());

								WeakObjectProperty->SetObjectPropertyValue(PropertyAddr, *RemappedObject);
							}
						}

					}
				}
				else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(DestData));
					if (Index != INDEX_NONE)
					{
						RemapProperty(ArrayProperty->Inner, INDEX_NONE, ActorRemapper, ArrayHelper.GetRawPtr(Index));
					}
					else
					{
						for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ArrayIndex++)
						{
							RemapProperty(ArrayProperty->Inner, INDEX_NONE, ActorRemapper, ArrayHelper.GetRawPtr(ArrayIndex));
						}
					}
				}
				else if (UMapProperty* MapProperty = Cast<UMapProperty>(Property))
				{
					FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(DestData));
					if (Index != INDEX_NONE)
					{
						uint8* PairPtr = MapHelper.GetPairPtr(Index);
						RemapProperty(MapProperty->ValueProp, INDEX_NONE, ActorRemapper, PairPtr);
					}
					else
					{
						for (int32 MapIndex = 0; MapIndex < MapHelper.Num(); MapIndex++)
						{
							uint8* PairPtr = MapHelper.GetPairPtr(MapIndex);
							RemapProperty(MapProperty->ValueProp, INDEX_NONE, ActorRemapper, PairPtr);
						}
					}
				}
				else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					if (Index != INDEX_NONE)
					{
						// If a concrete index was given, remap just that
						for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
						{
							RemapProperty(*It, INDEX_NONE, ActorRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Index));
						}
					}
					else
					{
						// If no concrete index was given, either the ArrayDim is 1 (i.e. not a static array), or the struct is within
						// a deeper structure (an array or another struct) and we cannot know which element was changed, so iterate through all elements.
						for (int32 Count = 0; Count < StructProperty->ArrayDim; Count++)
						{
							for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
							{
								RemapProperty(*It, INDEX_NONE, ActorRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Count));
							}
						}
					}
				}
			}

			static void RemapAssetReference(UProperty* Property, int32 Index, const FString& InOldAssetPath, const TMap<FString, FStringAssetReference>* AssetRemapper, uint8* DestData)
			{
				if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
				{
					// If there's a concrete index, use that, otherwise iterate all array members (for the case that this property is inside a struct, or there is exactly one element)
					const int32 Num = (Index == INDEX_NONE) ? ObjectProperty->ArrayDim : 1;
					const int32 StartIndex = (Index == INDEX_NONE) ? 0 : Index;
					for (int32 Count = 0; Count < Num; Count++)
					{
						uint8* PropertyAddr = ObjectProperty->ContainerPtrToValuePtr<uint8>(DestData, StartIndex + Count);
						UObject* PropertyValue = ObjectProperty->GetObjectPropertyValue(PropertyAddr);
						if (nullptr == PropertyValue && AssetRemapper != NULL)
						{
							if (const FSoftObjectPath* NewAssetRefPtr = (*AssetRemapper).Find(InOldAssetPath))
							{
								const FSoftObjectPath& NewAssetRef = *NewAssetRefPtr;
								if (NewAssetRef.IsValid())
								{
									UObject* NewAsset = NewAssetRef.TryLoad();
									if (NewAsset)
									{
										ObjectProperty->SetObjectPropertyValue(PropertyAddr, NewAsset);
										GWarn->Logf(ELogVerbosity::Display, TEXT("[RemapAssetReference] from {%s} to {%s}"), *InOldAssetPath, *NewAssetRef.ToString());
									}
								}
							}
						}
					}
				}
				else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(DestData));
					if (Index != INDEX_NONE)
					{
						RemapAssetReference(ArrayProperty->Inner, INDEX_NONE, InOldAssetPath, AssetRemapper, ArrayHelper.GetRawPtr(Index));
					}
					else
					{
						for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ArrayIndex++)
						{
							RemapAssetReference(ArrayProperty->Inner, INDEX_NONE, InOldAssetPath, AssetRemapper, ArrayHelper.GetRawPtr(ArrayIndex));
						}
					}
				}
				else if (UMapProperty* MapProperty = Cast<UMapProperty>(Property))
				{
					FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(DestData));
					if (Index != INDEX_NONE)
					{
						uint8* PairPtr = MapHelper.GetPairPtr(Index);
						RemapAssetReference(MapProperty->ValueProp, INDEX_NONE, InOldAssetPath, AssetRemapper, PairPtr);
					}
					else
					{
						for (int32 MapIndex = 0; MapIndex < MapHelper.Num(); MapIndex++)
						{
							uint8* PairPtr = MapHelper.GetPairPtr(MapIndex);
							RemapAssetReference(MapProperty->ValueProp, INDEX_NONE, InOldAssetPath, AssetRemapper, PairPtr);
						}
					}
				}
				else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					if (Index != INDEX_NONE)
					{
						// If a concrete index was given, remap just that
						for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
						{
							RemapAssetReference(*It, INDEX_NONE, InOldAssetPath, AssetRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Index));
						}
					}
					else
					{
						// If no concrete index was given, either the ArrayDim is 1 (i.e. not a static array), or the struct is within
						// a deeper structure (an array or another struct) and we cannot know which element was changed, so iterate through all elements.
						for (int32 Count = 0; Count < StructProperty->ArrayDim; Count++)
						{
							for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
							{
								RemapAssetReference(*It, INDEX_NONE, InOldAssetPath, AssetRemapper, StructProperty->ContainerPtrToValuePtr<uint8>(DestData, Count));
							}
						}
					}
				}
			}

			static const TCHAR* ImportProperties(uint8* DestData, const TCHAR* SourceText, UStruct* ObjectStruct, UObject* SubobjectRoot, UObject* SubobjectOuter, FFeedbackContext* Warn, int32 Depth, FObjectInstancingGraph& InstanceGraph, const TMap<AActor*, AActor*>* ActorRemapper, const TMap<FString, FSoftObjectPath>* AssetRemapper = NULL, TMap<FName, FString>* OutStaticMeshComponentCustomProps = NULL)
			{
				check(!GIsUCCMakeStandaloneHeaderGenerator);
				check(ObjectStruct != NULL);
				check(DestData != NULL);

				if (SourceText == NULL)
					return NULL;

				// Cannot create subobjects when importing struct defaults, or if SubobjectOuter (used as the Outer for any subobject declarations encountered) is NULL
				bool bSubObjectsAllowed = !ObjectStruct->IsA(UScriptStruct::StaticClass()) && SubobjectOuter != NULL;

				// true when DestData corresponds to a subobject in a class default object
				bool bSubObject = false;

				UClass* ComponentOwnerClass = NULL;

				if (bSubObjectsAllowed)
				{
					bSubObject = SubobjectRoot != NULL && SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject);
					if (SubobjectRoot == NULL)
					{
						SubobjectRoot = SubobjectOuter;
					}

					ComponentOwnerClass = SubobjectOuter != NULL
						? SubobjectOuter->IsA(UClass::StaticClass())
						? CastChecked<UClass>(SubobjectOuter)
						: SubobjectOuter->GetClass()
						: NULL;
				}


				// The PortFlags to use for all ImportText calls
				uint32 PortFlags = PPF_Delimited | PPF_CheckReferences;
				if (GIsImportingT3D)
				{
					PortFlags |= PPF_AttemptNonQualifiedSearch;
				}

				FString StrLine;

				TArray<FDefinedProperty> DefinedProperties;

				// Parse all objects stored in the actor.
				// Build list of all text properties.
				bool ImportedBrush = 0;
				int32 LinesConsumed = 0;
				while (FParse::LineExtended(&SourceText, StrLine, LinesConsumed, true))
				{
					// remove extra whitespace and optional semicolon from the end of the line
					{
						int32 Length = StrLine.Len();
						while (Length > 0 &&
							(StrLine[Length - 1] == TCHAR(';') || StrLine[Length - 1] == TCHAR(' ') || StrLine[Length - 1] == 9))
						{
							Length--;
						}
						if (Length != StrLine.Len())
						{
							StrLine = StrLine.Left(Length);
						}
					}

					if (ContextSupplier != NULL)
					{
						ContextSupplier->CurrentLine += LinesConsumed;
					}
					if (StrLine.Len() == 0)
					{
						continue;
					}

					const TCHAR* Str = *StrLine;

					int32 NewLineNumber;
					if (FParse::Value(Str, TEXT("linenumber="), NewLineNumber))
					{
						if (ContextSupplier != NULL)
						{
							ContextSupplier->CurrentLine = NewLineNumber;
						}
					}
					else if (GetBEGIN(&Str, TEXT("Brush")) && ObjectStruct->IsChildOf(ABrush::StaticClass()))
					{
						// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
						if (!bSubObjectsAllowed)
						{
							Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN BRUSH: Subobjects are not allowed in this context"));
							return NULL;
						}

						// Parse brush on this line.
						TCHAR BrushName[NAME_SIZE];
						if (FParse::Value(Str, TEXT("Name="), BrushName, NAME_SIZE))
						{
							// If an initialized brush with this name already exists in the level, rename the existing one.
							// It is deemed to be initialized if it has a non-zero poly count.
							// If it is uninitialized, the existing object will have been created by a forward reference in the import text,
							// and it will now be redefined.  This relies on the behavior that NewObject<> will return an existing pointer
							// if an object with the same name and outer is passed.
							UModel* ExistingBrush = FindObject<UModel>(SubobjectRoot, BrushName);
							if (ExistingBrush && ExistingBrush->Polys && ExistingBrush->Polys->Element.Num() > 0)
							{
								ExistingBrush->Rename();
							}

							// Create model.
							Local::ModelFactory_FactoryCreateText(UModel::StaticClass(), SubobjectRoot, FName(BrushName, FNAME_Add), RF_NoFlags, NULL, TEXT("t3d"), SourceText, SourceText + FCString::Strlen(SourceText), Warn);
							ImportedBrush = 1;
						}
					}
					else if (GetBEGIN(&Str, TEXT("Foliage")))
					{
						UFoliageType* SourceFoliageType;
						FName ComponentName;
						if (SubobjectRoot &&
							ParseObject<UFoliageType>(Str, TEXT("FoliageType="), SourceFoliageType, ANY_PACKAGE) &&
							FParse::Value(Str, TEXT("Component="), ComponentName))
						{
							UPrimitiveComponent* ActorComponent = FindObjectFast<UPrimitiveComponent>(SubobjectRoot, ComponentName);

							if (ActorComponent && ActorComponent->GetComponentLevel())
							{
								AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ActorComponent->GetComponentLevel(), true);

								FFoliageMeshInfo* MeshInfo = nullptr;
								UFoliageType* FoliageType = IFA->AddFoliageType(SourceFoliageType, &MeshInfo);

								const TCHAR* StrPtr;
								FString TextLine;
								while (MeshInfo && FParse::Line(&SourceText, TextLine))
								{
									StrPtr = *TextLine;
									if (GetEND(&StrPtr, TEXT("Foliage")))
									{
										break;
									}
									// WIP: For future Foliage Instances Support
#if 0
									// Parse the instance properties
									FFoliageInstance Instance;
									FString Temp;
									if (FParse::Value(StrPtr, TEXT("Location="), Temp, false))
									{
										GetFVECTOR(*Temp, Instance.Location);
									}
									if (FParse::Value(StrPtr, TEXT("Rotation="), Temp, false))
									{
										GetFROTATOR(*Temp, Instance.Rotation, 1);
									}
									if (FParse::Value(StrPtr, TEXT("PreAlignRotation="), Temp, false))
									{
										GetFROTATOR(*Temp, Instance.PreAlignRotation, 1);
									}
									if (FParse::Value(StrPtr, TEXT("DrawScale3D="), Temp, false))
									{
										GetFVECTOR(*Temp, Instance.DrawScale3D);
									}
									FParse::Value(StrPtr, TEXT("Flags="), Instance.Flags);

									// Add the instance
									MeshInfo->AddInstance(IFA, FoliageType, Instance, ActorComponent);
#endif
								}
							}
						}
					}
					else if (GetBEGIN(&Str, TEXT("Object")))
					{
						// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
						if (!bSubObjectsAllowed)
						{
							Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: Subobjects are not allowed in this context"));
							return NULL;
						}

						// Parse subobject default properties.
						// Note: default properties subobjects have compiled class as their Outer (used for localization).
						UClass*	TemplateClass = NULL;
						bool bInvalidClass = false;
						ParseObject<UClass>(Str, TEXT("Class="), TemplateClass, ANY_PACKAGE, &bInvalidClass);

						if (bInvalidClass)
						{
							Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: Invalid class specified: %s"), *StrLine);
							return NULL;
						}

						// parse the name of the template
						FName	TemplateName = NAME_None;
						FParse::Value(Str, TEXT("Name="), TemplateName);
						if (TemplateName == NAME_None)
						{
							Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: Must specify valid name for subobject/component: %s"), *StrLine);
							return NULL;
						}

						// points to the parent class's template subobject/component, if we are overriding a subobject/component declared in our parent class
						UObject* BaseTemplate = NULL;
						bool bRedefiningSubobject = false;
						if (TemplateClass)
						{
						}
						else
						{
							// next, verify that a template actually exists in the parent class
							UClass* ParentClass = ComponentOwnerClass->GetSuperClass();
							check(ParentClass);

							UObject* ParentCDO = ParentClass->GetDefaultObject();
							check(ParentCDO);

							BaseTemplate = StaticFindObjectFast(UObject::StaticClass(), SubobjectOuter, TemplateName);
							bRedefiningSubobject = (BaseTemplate != NULL);

							if (BaseTemplate == NULL)
							{
								BaseTemplate = StaticFindObjectFast(UObject::StaticClass(), ParentCDO, TemplateName);
							}

							if (BaseTemplate == NULL)
							{
								// wasn't found
								Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: No base template named %s found in parent class %s: %s"), *TemplateName.ToString(), *ParentClass->GetName(), *StrLine);
								return NULL;
							}

							TemplateClass = BaseTemplate->GetClass();
						}

						// because the outer won't be a default object

						checkSlow(TemplateClass != NULL);
						if (bRedefiningSubobject)
						{
							// since we're redefining an object in the same text block, only need to import properties again
							SourceText = Local::ImportObjectProperties((uint8*)BaseTemplate, SourceText, TemplateClass, SubobjectRoot, BaseTemplate,
								Warn, Depth + 1, ContextSupplier ? ContextSupplier->CurrentLine : 0, &InstanceGraph, ActorRemapper, AssetRemapper, OutStaticMeshComponentCustomProps);
						}
						else
						{
							UObject* Archetype = NULL;
							UObject* ComponentTemplate = NULL;

							// Since we are changing the class we can't use the Archetype,
							// however that is fine since we will have been editing the CDO anyways
							if (!FBlueprintEditorUtils::IsAnonymousBlueprintClass(SubobjectOuter->GetClass()))
							{
								// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
								FString ArchetypeName;
								if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
								{
									// if given a name, break it up along the ' so separate the class from the name
									FString ObjectClass;
									FString ObjectPath;
									if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
									{
										// find the class
										UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
										if (ArchetypeClass)
										{
											// if we had the class, find the archetype
											Archetype = StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath);
										}
									}
								}
							}

							if (SubobjectOuter->HasAnyFlags(RF_ClassDefaultObject))
							{
								if (!Archetype) // if an archetype was specified explicitly, we will stick with that
								{
									Archetype = ComponentOwnerClass->GetDefaultSubobjectByName(TemplateName);
									if (Archetype)
									{
										if (BaseTemplate == NULL)
										{
											// BaseTemplate should only be NULL if the Begin Object line specified a class
											Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: The component name %s is already used (if you want to override the component, don't specify a class): %s"), *TemplateName.ToString(), *StrLine);
											return NULL;
										}

										// the component currently in the component template map and the base template should be the same
										checkf(Archetype == BaseTemplate, TEXT("OverrideComponent: '%s'   BaseTemplate: '%s'"), *Archetype->GetFullName(), *BaseTemplate->GetFullName());
									}
								}
							}
							else // handle the non-template case (subobjects and non-template components)
							{
								// don't allow Actor-derived subobjects
								if (TemplateClass->IsChildOf(AActor::StaticClass()))
								{
									Warn->Logf(ELogVerbosity::Error, TEXT("Cannot create subobjects from Actor-derived classes: %s"), *StrLine);
									return NULL;
								}

								ComponentTemplate = FindObject<UObject>(SubobjectOuter, *TemplateName.ToString());
								if (ComponentTemplate != NULL)
								{
									// if we're overriding a subobject declared in a parent class, we should already have an object with that name that
									// was instanced when ComponentOwnerClass's CDO was initialized; if so, it's archetype should be the BaseTemplate.  If it
									// isn't, then there are two unrelated subobject definitions using the same name.
									if (ComponentTemplate->GetArchetype() != BaseTemplate)
									{
									}
									else if (BaseTemplate == NULL)
									{
										// BaseTemplate should only be NULL if the Begin Object line specified a class
										Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: A subobject named %s is already declared in a parent class.  If you intended to override that subobject, don't specify a class in the derived subobject definition: %s"), *TemplateName.ToString(), *StrLine);
										return NULL;
									}
								}

							}

							// Propagate object flags to the sub object.
							EObjectFlags NewFlags = SubobjectOuter->GetMaskedFlags(RF_PropagateToSubObjects);

							if (!Archetype) // no override and we didn't find one from the class table, so go with the base
							{
								Archetype = BaseTemplate;
							}

							UObject* OldComponent = NULL;
							if (ComponentTemplate)
							{
								bool bIsOkToReuse = ComponentTemplate->GetClass() == TemplateClass
									&& ComponentTemplate->GetOuter() == SubobjectOuter
									&& ComponentTemplate->GetFName() == TemplateName
									&& (ComponentTemplate->GetArchetype() == Archetype || !Archetype);

								if (!bIsOkToReuse)
								{
									PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("Could not reuse component instance %s, name clash?"), *ComponentTemplate->GetFullName());
									ComponentTemplate->Rename(); // just abandon the existing component, we are going to create
									OldComponent = ComponentTemplate;
									ComponentTemplate = NULL;
								}
							}


							if (!ComponentTemplate)
							{
								ComponentTemplate = NewObject<UObject>(
									SubobjectOuter,
									TemplateClass,
									TemplateName,
									NewFlags,
									Archetype,
									!!SubobjectOuter,
									&InstanceGraph
									);
							}
							else
							{
								// We do not want to set RF_Transactional for construction script created components, so we have to monkey with things here
								if (NewFlags & RF_Transactional)
								{
									UActorComponent* Component = Cast<UActorComponent>(ComponentTemplate);
									if (Component && Component->IsCreatedByConstructionScript())
									{
										NewFlags &= ~RF_Transactional;
									}
								}

								// Make sure desired flags are set - existing object could be pending kill
								ComponentTemplate->ClearFlags(RF_AllFlags);
								ComponentTemplate->ClearInternalFlags(EInternalObjectFlags::AllFlags);
								ComponentTemplate->SetFlags(NewFlags);
							}

							// replace all properties in this subobject outer' class that point to the original subobject with the new subobject
							TMap<UObject*, UObject*> ReplacementMap;
							if (Archetype)
							{
								checkSlow(ComponentTemplate->GetArchetype() == Archetype);
								ReplacementMap.Add(Archetype, ComponentTemplate);
								InstanceGraph.AddNewInstance(ComponentTemplate);
							}
							if (OldComponent)
							{
								ReplacementMap.Add(OldComponent, ComponentTemplate);
							}
							FArchiveReplaceObjectRef<UObject> ReplaceAr(SubobjectOuter, ReplacementMap, false, false, true);

							// import the properties for the subobject
							SourceText = Local::ImportObjectProperties(
								(uint8*)ComponentTemplate,
								SourceText,
								TemplateClass,
								SubobjectRoot,
								ComponentTemplate,
								Warn,
								Depth + 1,
								ContextSupplier ? ContextSupplier->CurrentLine : 0,
								&InstanceGraph,
								ActorRemapper,
								AssetRemapper,
								OutStaticMeshComponentCustomProps
							);
						}
					}
					else if (FParse::Command(&Str, TEXT("CustomProperties")))
					{
						check(SubobjectOuter);

						if (OutStaticMeshComponentCustomProps != NULL)
						{
							if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SubobjectOuter))
							{
								OutStaticMeshComponentCustomProps->Add(StaticMeshComponent->GetFName(), FString(*(&Str)));
							}
						}

						SubobjectOuter->ImportCustomProperties(Str, Warn);
					}
					else if (Local::GetEND(&Str, TEXT("Actor")) || GetEND(&Str, TEXT("DefaultProperties")) || GetEND(&Str, TEXT("structdefaultproperties")) || (GetEND(&Str, TEXT("Object")) && Depth))
					{
						// End of properties.
						break;
					}
					else if (Local::GetREMOVE(&Str, TEXT("Component")))
					{
						checkf(false, TEXT("Remove component is illegal in pasted text"));
					}
					else
					{
						int32 CountDefinedPropertiesBefore = DefinedProperties.Num();
						UProperty::ImportSingleProperty(Str, DestData, ObjectStruct, SubobjectOuter, PortFlags, Warn, DefinedProperties);
						int32 CountDefinedPropertiesAfter = DefinedProperties.Num();

						if ((DefinedProperties.Num() > 0) && (CountDefinedPropertiesAfter == CountDefinedPropertiesBefore + 1))
						{
							const TCHAR* StrCopy = Str;
							FPrefabPropertyUtil::SkipAfterEqual(StrCopy);
							FString AssetPath;
							if (*StrCopy && FPrefabPropertyUtil::GetAssetPath(StrCopy, &AssetPath))
							{
								FSoftObjectPath CurrentAssetRef(AssetPath);
								if (CurrentAssetRef.IsValid() && (nullptr == CurrentAssetRef.TryLoad()))
								{
									//Warn->Logf(ELogVerbosity::Display, TEXT("          => [FSoftObjectPath]: StrCopy: %s, ToString(): %s"), StrCopy, *(CurrentAssetRef.ToString()));
									FDefinedProperty& DefinedProperty = DefinedProperties.Last();
									RemapAssetReference(DefinedProperty.Property, DefinedProperty.Index, CurrentAssetRef.ToString(), AssetRemapper, DestData);
								}
							}
						}
					}
				}

				if (ActorRemapper)
				{
					for (const auto& DefinedProperty : DefinedProperties)
					{
						RemapProperty(DefinedProperty.Property, DefinedProperty.Index, *ActorRemapper, DestData);
					}
				}

				// Prepare brush.
				if (ImportedBrush && ObjectStruct->IsChildOf<ABrush>() && !ObjectStruct->IsChildOf<AVolume>())
				{
					check(GIsEditor);
					ABrush* Actor = (ABrush*)DestData;
					check(Actor->GetBrushComponent());
					if (Actor->GetBrushComponent()->Mobility == EComponentMobility::Static)
					{
						// Prepare static brush.
						Actor->SetNotForClientOrServer();
					}
					else
					{
						// Prepare moving brush.
						FBSPOps::csgPrepMovingBrush(Actor);
					}
				}

				return SourceText;
			}

			static const TCHAR* ImportObjectProperties(FImportObjectParams& InParams, const TMap<FString, FSoftObjectPath>* AssetRemapper = NULL, TMap<FName, FString>* OutStaticMeshComponentCustomProps = NULL)
			{
				FDefaultPropertiesContextSupplier Supplier;
				if (InParams.LineNumber != INDEX_NONE)
				{
					if (InParams.SubobjectRoot == NULL)
					{
						Supplier.PackageName = InParams.ObjectStruct->GetOwnerClass() ? InParams.ObjectStruct->GetOwnerClass()->GetOutermost()->GetName() : InParams.ObjectStruct->GetOutermost()->GetName();
						Supplier.ClassName = InParams.ObjectStruct->GetOwnerClass() ? InParams.ObjectStruct->GetOwnerClass()->GetName() : FName(NAME_None).ToString();
						Supplier.CurrentLine = InParams.LineNumber;

						ContextSupplier = &Supplier; //-V506
					}
					else
					{
						if (ContextSupplier != NULL)
						{
							ContextSupplier->CurrentLine = InParams.LineNumber;
						}
					}
					InParams.Warn->SetContext(ContextSupplier);
				}

				if (InParams.bShouldCallEditChange && InParams.SubobjectOuter != NULL)
				{
					InParams.SubobjectOuter->PreEditChange(NULL);
				}

				FObjectInstancingGraph* CurrentInstanceGraph = InParams.InInstanceGraph;
				if (InParams.SubobjectRoot != NULL && InParams.SubobjectRoot != UObject::StaticClass()->GetDefaultObject())
				{
					if (CurrentInstanceGraph == NULL)
					{
						CurrentInstanceGraph = new FObjectInstancingGraph;
					}
					CurrentInstanceGraph->SetDestinationRoot(InParams.SubobjectRoot);
				}

				FObjectInstancingGraph TempGraph;
				FObjectInstancingGraph& InstanceGraph = CurrentInstanceGraph ? *CurrentInstanceGraph : TempGraph;

				// Parse the object properties.
				const TCHAR* NewSourceText =
					ImportProperties(
						InParams.DestData,
						InParams.SourceText,
						InParams.ObjectStruct,
						InParams.SubobjectRoot,
						InParams.SubobjectOuter,
						InParams.Warn,
						InParams.Depth,
						InstanceGraph,
						InParams.ActorRemapper,
						AssetRemapper,
						OutStaticMeshComponentCustomProps
					);

				if (InParams.SubobjectOuter != NULL)
				{
					check(InParams.SubobjectRoot);

					// Update the object properties to point to the newly imported component objects.
					// Templates inside classes never need to have components instanced.
					if (!InParams.SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject))
					{
						UObject* SubobjectArchetype = InParams.SubobjectOuter->GetArchetype();
						InParams.ObjectStruct->InstanceSubobjectTemplates(InParams.DestData, SubobjectArchetype, SubobjectArchetype->GetClass(),
							InParams.SubobjectOuter, &InstanceGraph);
					}

					if (InParams.bShouldCallEditChange)
					{
						// notify the object that it has just been imported
						InParams.SubobjectOuter->PostEditImport();

						// notify the object that it has been edited
						InParams.SubobjectOuter->PostEditChange();
					}
					InParams.SubobjectRoot->CheckDefaultSubobjects();
				}

				if (InParams.LineNumber != INDEX_NONE)
				{
					if (ContextSupplier == &Supplier)
					{
						ContextSupplier = NULL;
						InParams.Warn->SetContext(NULL);
					}
				}

				// if we created the instance graph, delete it now
				if (CurrentInstanceGraph != NULL && InParams.InInstanceGraph == NULL)
				{
					delete CurrentInstanceGraph;
					CurrentInstanceGraph = NULL;
				}

				return NewSourceText;
			}

			static const TCHAR* ImportObjectProperties(uint8* DestData, const TCHAR* SourceText, UStruct* ObjectStruct, UObject* SubobjectRoot, UObject* SubobjectOuter, FFeedbackContext* Warn, int32 Depth, int32 LineNumber = INDEX_NONE, FObjectInstancingGraph* InInstanceGraph = NULL, const TMap<AActor*, AActor*>* ActorRemapper = NULL, const TMap<FString, FStringAssetReference>* AssetRemapper = NULL, TMap<FName, FString>* OutStaticMeshComponentCustomProps = NULL)
			{
				FImportObjectParams Params;
				{
					Params.DestData = DestData;
					Params.SourceText = SourceText;
					Params.ObjectStruct = ObjectStruct;
					Params.SubobjectRoot = SubobjectRoot;
					Params.SubobjectOuter = SubobjectOuter;
					Params.Warn = Warn;
					Params.Depth = Depth;
					Params.LineNumber = LineNumber;
					Params.InInstanceGraph = InInstanceGraph;
					Params.ActorRemapper = ActorRemapper;

					// This implementation always calls PreEditChange/PostEditChange
					Params.bShouldCallEditChange = true;
				}

				return ImportObjectProperties(Params, AssetRemapper, OutStaticMeshComponentCustomProps);
			}

			static UObject* PackageFactory_FactoryCreateText(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
			{
				const bool bUpdateSelectionInPackageFactory = false;

				//FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

				bool bSavedImportingT3D = GIsImportingT3D;
				// Mark us as importing a T3D.
				GEditor->IsImportingT3D = true;
				GIsImportingT3D = true;

				if (Parent != nullptr)
				{
					return nullptr;
				}

				TMap<FString, UPackage*> MapPackages;
				bool bImportingMapPackage = false;

				UPackage* TopLevelPackage = nullptr;
				UPackage* RootMapPackage = nullptr;
				UWorld* World = GWorld;
				if (World)
				{
					RootMapPackage = World->GetOutermost();
				}

				if (RootMapPackage)
				{
					if (RootMapPackage->GetName() == Name.ToString())
					{
						// Loading into the Map package!
						MapPackages.Add(RootMapPackage->GetName(), RootMapPackage);
						TopLevelPackage = RootMapPackage;
						bImportingMapPackage = true;
					}
				}

				// Unselect all actors.
				if (bUpdateSelectionInPackageFactory)
				{
					GEditor->SelectNone(false, false);
				}

				// Mark us importing a T3D (only from a file, not from copy/paste).
				GEditor->IsImportingT3D = FCString::Stricmp(Type, TEXT("paste")) != 0;
				GIsImportingT3D = GEditor->IsImportingT3D;

				// Maintain a list of a new package objects and the text they were created from.
				TMap<UObject*, FString> NewPackageObjectMap;

				FString StrLine;
				while (FParse::Line(&Buffer, StrLine))
				{
					const TCHAR* Str = *StrLine;

					if (GetBEGIN(&Str, TEXT("TOPLEVELPACKAGE")) && !bImportingMapPackage)
					{
						//Begin TopLevelPackage Class=Package Name=ExportTest_ORIG Archetype=Package'Core.Default__Package'
						UClass* TempClass;
						if (ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE))
						{
							// Get actor name.
							FName PackageName(NAME_None);
							FParse::Value(Str, TEXT("NAME="), PackageName);

							if (FindObject<UPackage>(ANY_PACKAGE, *(PackageName.ToString())))
							{
								PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Warning, TEXT("Package factory can only handle the map package or new packages!"));
								return nullptr;
							}
							TopLevelPackage = CreatePackage(nullptr, *(PackageName.ToString()));
							TopLevelPackage->SetFlags(RF_Standalone | RF_Public);
							MapPackages.Add(TopLevelPackage->GetName(), TopLevelPackage);

							// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
							FString ArchetypeName;
							AActor* Archetype = nullptr;
							if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
							{
							}
						}
					}
					else if (GetBEGIN(&Str, TEXT("PACKAGE")))
					{
						FString ParentPackageName;
						FParse::Value(Str, TEXT("PARENTPACKAGE="), ParentPackageName);
						UClass* PkgClass;
						if (ParseObject<UClass>(Str, TEXT("CLASS="), PkgClass, ANY_PACKAGE))
						{
							// Get the name of the object.
							FName NewPackageName(NAME_None);
							FParse::Value(Str, TEXT("NAME="), NewPackageName);

							// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
							FString ArchetypeName;
							UPackage* Archetype = nullptr;
							if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
							{
								// if given a name, break it up along the ' so separate the class from the name
								FString ObjectClass;
								FString ObjectPath;
								if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
								{
									// find the class
									UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
									if (ArchetypeClass)
									{
										if (ArchetypeClass->IsChildOf(UPackage::StaticClass()))
										{
											// if we had the class, find the archetype
											Archetype = Cast<UPackage>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
										}
										else
										{
											Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Package"),
												Str, *ObjectClass);
										}
									}
								}

								UPackage* ParentPkg = nullptr;
								UPackage** ppParentPkg = MapPackages.Find(ParentPackageName);
								if (ppParentPkg)
								{
									ParentPkg = *ppParentPkg;
								}
								check(ParentPkg);

								auto NewPackage = NewObject<UPackage>(ParentPkg, NewPackageName, RF_NoFlags, Archetype);
								check(NewPackage);
								NewPackage->SetFlags(RF_Standalone | RF_Public);
								MapPackages.Add(NewPackageName.ToString(), NewPackage);
							}
						}
					}
				}

				for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
				{
					UObject* LoadObject = *ObjIt;

					if (LoadObject)
					{
						bool bModifiedObject = false;

						FString* PropText = NewPackageObjectMap.Find(LoadObject);
						if (PropText)
						{
							LoadObject->PreEditChange(nullptr);
							ImportObjectProperties((uint8*)LoadObject, **PropText, LoadObject->GetClass(), LoadObject, LoadObject, Warn, 0);
						}

						if (bModifiedObject)
						{
							// Let the actor deal with having been imported, if desired.
							LoadObject->PostEditImport();
							// Notify actor its properties have changed.
							LoadObject->PostEditChange();
							LoadObject->SetFlags(RF_Standalone | RF_Public);
							LoadObject->MarkPackageDirty();
						}
					}
				}

				// Mark us as no longer importing a T3D.
				GEditor->IsImportingT3D = bSavedImportingT3D;
				GIsImportingT3D = bSavedImportingT3D;

				//FEditorDelegates::OnAssetPostImport.Broadcast(this, TopLevelPackage);

				return TopLevelPackage;
			}

			static UObject* ModelFactory_FactoryCreateText(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
			{
				//FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

				ABrush* TempOwner = (ABrush*)Context;
				UModel* Model = NewObject<UModel>(Parent, Name, Flags);
				Model->Initialize(TempOwner, true);

				const TCHAR* StrPtr;
				FString StrLine;
				if (TempOwner)
				{
					TempOwner->InitPosRotScale();
					GEditor->GetSelectedActors()->Deselect(TempOwner);
				}
				while (FParse::Line(&Buffer, StrLine))
				{
					StrPtr = *StrLine;
					if (GetEND(&StrPtr, TEXT("BRUSH")))
					{
						break;
					}
					else if (GetBEGIN(&StrPtr, TEXT("POLYLIST")))
					{
						Model->Polys = (UPolys*)PolysFactory_FactoryCreateText(UPolys::StaticClass(), Model, NAME_None, RF_Transactional, nullptr, Type, Buffer, BufferEnd, Warn);
						check(Model->Polys);
					}
					if (TempOwner)
					{
						if (FParse::Command(&StrPtr, TEXT("PREPIVOT")))
						{
							FVector TempPrePivot(0.f);
							GetFVECTOR(StrPtr, TempPrePivot);
							TempOwner->SetPivotOffset(TempPrePivot);
						}
						else if (FParse::Command(&StrPtr, TEXT("LOCATION")))
						{
							FVector NewLocation(0.f);
							GetFVECTOR(StrPtr, NewLocation);
							TempOwner->SetActorLocation(NewLocation, false);
						}
						else if (FParse::Command(&StrPtr, TEXT("ROTATION")))
						{
							FRotator NewRotation;
							GetFROTATOR(StrPtr, NewRotation, 1);
							TempOwner->SetActorRotation(NewRotation);
						}
						if (FParse::Command(&StrPtr, TEXT("SETTINGS")))
						{
							uint8 BrushType = (uint8)TempOwner->BrushType;
							FParse::Value(StrPtr, TEXT("BRUSHTYPE="), BrushType);
							TempOwner->BrushType = EBrushType(BrushType);
							FParse::Value(StrPtr, TEXT("POLYFLAGS="), TempOwner->PolyFlags);
						}
					}
				}

				//FEditorDelegates::OnAssetPostImport.Broadcast(this, Model);

				return Model;
			}

			static UObject* PolysFactory_FactoryCreateText(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext*	Warn)
			{
				FVector PointPool[4096];
				int32 NumPoints = 0;

				//FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

				// Create polys.	
				UPolys* Polys = Context ? CastChecked<UPolys>(Context) : NewObject<UPolys>(Parent, Name, Flags);

				// Eat up if present.
				GetBEGIN(&Buffer, TEXT("POLYLIST"));

				// Parse all stuff.
				int32 First = 1, GotBase = 0;
				FString StrLine, ExtraLine;
				FPoly Poly;
				while (FParse::Line(&Buffer, StrLine))
				{
					const TCHAR* Str = *StrLine;
					if (GetEND(&Str, TEXT("POLYLIST")))
					{
						// End of brush polys.
						break;
					}
					//
					//
					// AutoCad - DXF File
					//
					//
					else if (FCString::Strstr(Str, TEXT("ENTITIES")) && First)
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Log, TEXT("Reading Autocad DXF file"));
						int32 Started = 0, IsFace = 0;
						FPoly NewPoly; NewPoly.Init();
						NumPoints = 0;

						while
							(FParse::Line(&Buffer, StrLine, 1)
								&& FParse::Line(&Buffer, ExtraLine, 1))
						{
							// Handle the line.
							Str = *ExtraLine;
							int32 Code = FCString::Atoi(*StrLine);
							if (Code == 0)
							{
								// Finish up current poly.
								if (Started)
								{
									if (NewPoly.Vertices.Num() == 0)
									{
										// Got a vertex definition.
										NumPoints++;
									}
									else if (NewPoly.Vertices.Num() >= 3)
									{
										// Got a poly definition.
										if (IsFace) NewPoly.Reverse();
										NewPoly.Base = NewPoly.Vertices[0];
										NewPoly.Finalize(nullptr, 0);
										new(Polys->Element)FPoly(NewPoly);
									}
									else
									{
										// Bad.
										Warn->Logf(TEXT("DXF: Bad vertex count %i"), NewPoly.Vertices.Num());
									}

									// Prepare for next.
									NewPoly.Init();
								}
								Started = 0;

								if (FParse::Command(&Str, TEXT("VERTEX")))
								{
									// Start of new vertex.
									PointPool[NumPoints] = FVector::ZeroVector;
									Started = 1;
									IsFace = 0;
								}
								else if (FParse::Command(&Str, TEXT("3DFACE")))
								{
									// Start of 3d face definition.
									Started = 1;
									IsFace = 1;
								}
								else if (FParse::Command(&Str, TEXT("SEQEND")))
								{
									// End of sequence.
									NumPoints = 0;
								}
								else if (FParse::Command(&Str, TEXT("EOF")))
								{
									// End of file.
									break;
								}
							}
							else if (Started)
							{
								// Replace commas with periods to handle european dxf's.
								//for( TCHAR* Stupid = FCString::Strchr(*ExtraLine,','); Stupid; Stupid=FCString::Strchr(Stupid,',') )
								//	*Stupid = '.';

								// Handle codes.
								if (Code >= 10 && Code <= 19)
								{
									// X coordinate.
									int32 VertexIndex = Code - 10;
									if (IsFace && VertexIndex >= NewPoly.Vertices.Num())
									{
										NewPoly.Vertices.AddZeroed(VertexIndex - NewPoly.Vertices.Num() + 1);
									}
									NewPoly.Vertices[VertexIndex].X = PointPool[NumPoints].X = FCString::Atof(*ExtraLine);
								}
								else if (Code >= 20 && Code <= 29)
								{
									// Y coordinate.
									int32 VertexIndex = Code - 20;
									NewPoly.Vertices[VertexIndex].Y = PointPool[NumPoints].Y = FCString::Atof(*ExtraLine);
								}
								else if (Code >= 30 && Code <= 39)
								{
									// Z coordinate.
									int32 VertexIndex = Code - 30;
									NewPoly.Vertices[VertexIndex].Z = PointPool[NumPoints].Z = FCString::Atof(*ExtraLine);
								}
								else if (Code >= 71 && Code <= 79 && (Code - 71) == NewPoly.Vertices.Num())
								{
									int32 iPoint = FMath::Abs(FCString::Atoi(*ExtraLine));
									if (iPoint > 0 && iPoint <= NumPoints)
										new(NewPoly.Vertices) FVector(PointPool[iPoint - 1]);
									else PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Warning, TEXT("DXF: Invalid point index %i/%i"), iPoint, NumPoints);
								}
							}
						}
					}
					//
					//
					// 3D Studio MAX - ASC File
					//
					//
					else if (FCString::Strstr(Str, TEXT("Tri-mesh,")) && First)
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Log, TEXT("Reading 3D Studio ASC file"));
						NumPoints = 0;

					AscReloop:
						int32 TempNumPolys = 0, TempVerts = 0;
						while (FParse::Line(&Buffer, StrLine))
						{
							Str = *StrLine;

							FString VertText = FString::Printf(TEXT("Vertex %i:"), NumPoints);
							FString FaceText = FString::Printf(TEXT("Face %i:"), TempNumPolys);
							if (FCString::Strstr(Str, *VertText))
							{
								PointPool[NumPoints].X = FCString::Atof(FCString::Strstr(Str, TEXT("X:")) + 2);
								PointPool[NumPoints].Y = FCString::Atof(FCString::Strstr(Str, TEXT("Y:")) + 2);
								PointPool[NumPoints].Z = FCString::Atof(FCString::Strstr(Str, TEXT("Z:")) + 2);
								NumPoints++;
								TempVerts++;
							}
							else if (FCString::Strstr(Str, *FaceText))
							{
								Poly.Init();
								new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str, TEXT("A:")) + 2)]);
								new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str, TEXT("B:")) + 2)]);
								new(Poly.Vertices)FVector(PointPool[FCString::Atoi(FCString::Strstr(Str, TEXT("C:")) + 2)]);
								Poly.Base = Poly.Vertices[0];
								Poly.Finalize(nullptr, 0);
								new(Polys->Element)FPoly(Poly);
								TempNumPolys++;
							}
							else if (FCString::Strstr(Str, TEXT("Tri-mesh,")))
								goto AscReloop;
						}
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Log, TEXT("Imported %i vertices, %i faces"), TempVerts, Polys->Element.Num());
					}
					//
					//
					// T3D FORMAT
					//
					//
					else if (GetBEGIN(&Str, TEXT("POLYGON")))
					{
						// Init to defaults and get group/item and texture.
						Poly.Init();
						FParse::Value(Str, TEXT("LINK="), Poly.iLink);
						FParse::Value(Str, TEXT("ITEM="), Poly.ItemName);
						FParse::Value(Str, TEXT("FLAGS="), Poly.PolyFlags);
						FParse::Value(Str, TEXT("LightMapScale="), Poly.LightMapScale);
						Poly.PolyFlags &= ~PF_NoImport;

						FString TextureName;
						// only load the texture if it was present
						if (FParse::Value(Str, TEXT("TEXTURE="), TextureName))
						{
							Poly.Material = Cast<UMaterialInterface>(StaticFindObject(UMaterialInterface::StaticClass(), ANY_PACKAGE, *TextureName));
						}
					}
					else if (FParse::Command(&Str, TEXT("PAN")))
					{
						int32	PanU = 0,
							PanV = 0;

						FParse::Value(Str, TEXT("U="), PanU);
						FParse::Value(Str, TEXT("V="), PanV);

						Poly.Base += Poly.TextureU * PanU;
						Poly.Base += Poly.TextureV * PanV;
					}
					else if (FParse::Command(&Str, TEXT("ORIGIN")))
					{
						GotBase = 1;
						GetFVECTOR(Str, Poly.Base);
					}
					else if (FParse::Command(&Str, TEXT("VERTEX")))
					{
						FVector TempVertex;
						GetFVECTOR(Str, TempVertex);
						new(Poly.Vertices) FVector(TempVertex);
					}
					else if (FParse::Command(&Str, TEXT("TEXTUREU")))
					{
						GetFVECTOR(Str, Poly.TextureU);
					}
					else if (FParse::Command(&Str, TEXT("TEXTUREV")))
					{
						GetFVECTOR(Str, Poly.TextureV);
					}
					else if (GetEND(&Str, TEXT("POLYGON")))
					{
						if (!GotBase)
							Poly.Base = Poly.Vertices[0];
						if (Poly.Finalize(nullptr, 1) == 0)
							new(Polys->Element)FPoly(Poly);
						GotBase = 0;
					}
				}

				//FEditorDelegates::OnAssetPostImport.Broadcast(this, Polys);

				// Success.
				return Polys;
			}

			static void LockGroupActor(AGroupActor* GroupActor)
			{
				GroupActor->bLocked = true;
				for (int32 SubGroupIdx = 0; SubGroupIdx < GroupActor->SubGroups.Num(); ++SubGroupIdx)
				{
					if (GroupActor->SubGroups[SubGroupIdx] != NULL)
					{
						LockGroupActor(GroupActor->SubGroups[SubGroupIdx]);
					}
				}
			}
		};

		UWorld* World = GWorld;
		//@todo locked levels - if lock state is persistent, do we need to check for whether the level is locked?
#ifdef MULTI_LEVEL_IMPORT
		// this level is the current level for pasting. If we get a named level, not for pasting, we will look up the level, and overwrite this
		ULevel*				OldCurrentLevel = World->GetCurrentLevel();
		check(OldCurrentLevel);
#endif

		UPackage* RootMapPackage = Cast<UPackage>(InParent);
		TMap<FString, UPackage*> MapPackages;
		TMap<AActor*, AActor*> MapActors;
		// Assumes data is being imported over top of a new, valid map.
		FParse::Next(&InBuffer);
		if (Local::GetBEGIN(&InBuffer, TEXT("MAP")))
		{
			if (RootMapPackage)
			{
				FString MapName;
				if (FParse::Value(InBuffer, TEXT("Name="), MapName))
				{
					// Advance the buffer
					InBuffer += FCString::Strlen(TEXT("Name="));
					InBuffer += MapName.Len();
					// Check to make sure that there are no naming conflicts
					if (RootMapPackage->Rename(*MapName, nullptr, REN_Test | REN_ForceNoResetLoaders))
					{
						// Rename it!
						RootMapPackage->Rename(*MapName, nullptr, REN_ForceNoResetLoaders);
					}
					else
					{
						InWarn->Logf(ELogVerbosity::Warning, TEXT("The Root map package name : '%s', conflicts with the existing object : '%s'"), *RootMapPackage->GetFullName(), *MapName);
						//FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
						return nullptr;
					}

					// Stick it in the package map
					MapPackages.Add(MapName, RootMapPackage);
				}
			}
		}
		else
		{
			return World;
		}

		bool bIsExpectingNewMapTag = false;

		// Unselect all actors.
		if (bUpdateSelection)
		{
			GEditor->SelectNone(false, false);
		}

		// Mark us importing a T3D (only from a file, not from copy/paste).
		GEditor->IsImportingT3D = (FCString::Stricmp(InType, TEXT("paste")) != 0) && (FCString::Stricmp(InType, TEXT("move")) != 0);
		GIsImportingT3D = GEditor->IsImportingT3D;

		// We need to detect if the .t3d file is the entire level or just selected actors, because we
		// don't want to replace the WorldSettings and BuildBrush if they already exist. To know if we
		// can skip the WorldSettings and BuilderBrush (which will always be the first two actors if the entire
		// level was exported), we make sure the first actor is a WorldSettings, if it is, and we already had
		// a WorldSettings, then we skip the builder brush
		// In other words, if we are importing a full level into a full level, we don't want to import
		// the WorldSettings and BuildBrush
		bool bShouldSkipImportSpecialActors = false;
		bool bHitLevelToken = false;

		FString MapPackageText;

		int32 ActorIndex = 0;

		//@todo locked levels - what needs to happen here?


		// Maintain a list of a new actors and the text they were created from.
		TMap<AActor*, FString> NewActorMap;
		TMap< FString, AGroupActor* > NewGroups; // Key=The orig actor's group's name, Value=The new actor's group.

												 // Maintain a lookup for the new actors, keyed by their source FName.
		TMap<FName, AActor*> NewActorsFNames;

		// Maintain a lookup from existing to new actors, used when replacing internal references when copy+pasting / duplicating
		TMap<AActor*, AActor*> ExistingToNewMap;

		// Maintain a lookup of the new actors to their parent and socket attachment if provided.
		struct FAttachmentDetail
		{
			const FName ParentName;
			const FName SocketName;
			FAttachmentDetail(const FName InParentName, const FName InSocketName) : ParentName(InParentName), SocketName(InSocketName) {}
		};
		TMap<AActor*, FAttachmentDetail> NewActorsAttachmentMap;

		FString StrLine;
		while (FParse::Line(&InBuffer, StrLine))
		{
			const TCHAR* Str = *StrLine;

			// If we're still waiting to see a 'MAP' tag, then check for that
			if (bIsExpectingNewMapTag)
			{
				if (Local::GetBEGIN(&Str, TEXT("MAP")))
				{
					bIsExpectingNewMapTag = false;
				}
				else
				{
					// Not a new map tag, so continue on
				}
			}
			else if (Local::GetEND(&Str, TEXT("MAP")))
			{
				// End of brush polys.
				bIsExpectingNewMapTag = true;
			}
			else if (Local::GetBEGIN(&Str, TEXT("LEVEL")))
			{
				bHitLevelToken = true;
#ifdef MULTI_LEVEL_IMPORT
				// try to look up the named level. if this fails, we will need to create a new level
				if (ParseObject<ULevel>(Str, TEXT("NAME="), World->GetCurrentLevel(), World->GetOuter()) == false)
				{
					// get the name
					FString LevelName;
					// if there is no name, that means we are pasting, so just put this guy into the CurrentLevel - don't make a new one
					if (FParse::Value(Str, TEXT("NAME="), LevelName))
					{
						// create a new named level
						World->SetCurrentLevel(new(World->GetOuter(), *LevelName)ULevel(FObjectInitializer(), FURL(nullptr)));
					}
				}
#endif
			}
			else if (Local::GetEND(&Str, TEXT("LEVEL")))
			{
#ifdef MULTI_LEVEL_IMPORT
				// any actors outside of a level block go into the current level
				World->SetCurrentLevel(OldCurrentLevel);
#endif
			}
			else if (Local::GetBEGIN(&Str, TEXT("ACTOR")))
			{
				// Load Blueprint if not already
				FString ClassName;
				bool bBlueprintClass = false;
				if (FParse::Value(Str, TEXT("CLASS="), ClassName))
				{
					if (ClassName.EndsWith(TEXT("_C")))
					{
						bBlueprintClass = true;

						FString ArchetypeName;
						if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
						{
							FString ObjectClass;
							FString ObjectPath;
							if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
							{
								UObject* Archetype = LoadObject<UObject>(nullptr, *ObjectPath);
							}
						}
					}
				}

				UClass* TempClass = NULL;
				bool bClassFound = ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE);

				if (!bClassFound && bBlueprintClass)
				{
					// Try Load Blueprint Class from Blueprint Asset
					FString ArchetypeName;
					AActor* Archetype = nullptr;
					if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
					{
						TempClass = FPrefabPropertyUtil::GetBlueprintClassFromArchetype(ArchetypeName, InAssetRemapper);
						bClassFound = TempClass != NULL;
					}
				}

				if (bClassFound)
				{
					// Get actor name.
					FName ActorUniqueName(NAME_None);
					FName ActorSourceName(NAME_None);
					FParse::Value(Str, TEXT("NAME="), ActorSourceName);

					ActorUniqueName = ActorSourceName;
					// Make sure this name is unique.
					AActor* Found = nullptr;
					if (ActorUniqueName != NAME_None)
					{
						// look in the current level for the same named actor
						Found = FindObject<AActor>(World->GetCurrentLevel(), *ActorUniqueName.ToString());
					}

					if (Found)
					{
						ActorUniqueName = MakeUniqueObjectName(World->GetCurrentLevel(), TempClass, ActorUniqueName);
					}

					// Get parent name for attachment.
					FName ActorParentName(NAME_None);
					FParse::Value(Str, TEXT("ParentActor="), ActorParentName);

					// Get socket name for attachment.
					FName ActorParentSocket(NAME_None);
					FParse::Value(Str, TEXT("SocketName="), ActorParentSocket);

					// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
					FString ArchetypeName;
					AActor* Archetype = nullptr;
					if (FParse::Value(Str, TEXT("Archetype="), ArchetypeName))
					{
						// if given a name, break it up along the ' so separate the class from the name
						FString ObjectClass;
						FString ObjectPath;
						if (FPrefabParser::ParseExportTextPath(ArchetypeName, &ObjectClass, &ObjectPath))
						{
							// find the class
							UClass* ArchetypeClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ObjectClass);
							if (ArchetypeClass)
							{
								if (ArchetypeClass->IsChildOf(AActor::StaticClass()))
								{
									// if we had the class, find the archetype
									Archetype = Cast<AActor>(StaticFindObject(ArchetypeClass, ANY_PACKAGE, *ObjectPath));
								}
								else
								{
									InWarn->Logf(ELogVerbosity::Warning, TEXT("Invalid archetype specified in subobject definition '%s': %s is not a child of Actor"),
										Str, *ObjectClass);
								}
							}
						}
					}

					// If we're pasting from a class that belongs to a map we need to duplicate the class and use that instead
					if (FBlueprintEditorUtils::IsAnonymousBlueprintClass(TempClass))
					{
						UBlueprint* NewBP = DuplicateObject(CastChecked<UBlueprint>(TempClass->ClassGeneratedBy), World->GetCurrentLevel(), *FString::Printf(TEXT("%s_BPClass"), *ActorUniqueName.ToString()));
						if (NewBP)
						{
							NewBP->ClearFlags(RF_Standalone);

							FKismetEditorUtilities::CompileBlueprint(NewBP, EBlueprintCompileOptions::SkipGarbageCollection);

							TempClass = NewBP->GeneratedClass;

							// Since we changed the class we can't use an Archetype,
							// however that is fine since we will have been editing the CDO anyways
							Archetype = nullptr;
						}
					}

					if (TempClass->IsChildOf(AWorldSettings::StaticClass()))
					{
						// if we see a WorldSettings, then we are importing an entire level, so if we
						// are importing into an existing level, then we should not import the next actor
						// which will be the builder brush
						check(ActorIndex == 0);

						// if we have any actors, then we are importing into an existing level
						if (World->GetCurrentLevel()->Actors.Num())
						{
							check(World->GetCurrentLevel()->Actors[0]->IsA(AWorldSettings::StaticClass()));

							// full level into full level, skip the first two actors
							bShouldSkipImportSpecialActors = true;
						}
					}

					// Get property text.
					FString PropText, PropertyLine;
					while
						(Local::GetEND(&InBuffer, TEXT("ACTOR")) == 0
							&& FParse::Line(&InBuffer, PropertyLine))
					{
						PropText += *PropertyLine;
						PropText += TEXT("\r\n");
					}

					// If we need to skip the WorldSettings and BuilderBrush, skip the first two actors.  Note that
					// at this point, we already know that we have a WorldSettings and BuilderBrush in the .t3d.
					if (FLevelUtils::IsLevelLocked(World->GetCurrentLevel()))
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Warning, TEXT("Import actor: The requested operation could not be completed because the level is locked."));
						//FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
						return nullptr;
					}
					else if (!(bShouldSkipImportSpecialActors && ActorIndex < 2))
					{
						// Don't import the default physics volume, as it doesn't have a UModel associated with it
						// and thus will not import properly.
						if (!TempClass->IsChildOf(ADefaultPhysicsVolume::StaticClass()))
						{
							// Create a new actor.

							const bool bDuplicatedSourceNameFound = NewActorsFNames.Find(ActorSourceName) != nullptr;

							FActorSpawnParameters SpawnInfo;
							SpawnInfo.Name = ActorUniqueName;
							SpawnInfo.Template = Archetype;
							SpawnInfo.ObjectFlags = InFlags;
							SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
							if (GEditor->bIsSimulatingInEditor)
							{
								// During SIE, we don't want to run construction scripts on a BP until it is completely constructed
								SpawnInfo.bDeferConstruction = true;
							}

							AActor* NewActor = !bDuplicatedSourceNameFound ? World->SpawnActor(TempClass, nullptr, nullptr, SpawnInfo) : nullptr;

							if (NewActor)
							{
								if (UActorGroupingUtils::IsGroupingActive() && !Cast<AGroupActor>(NewActor))
								{
									bool bGrouped = false;

									AGroupActor** tmpNewGroup = nullptr;
									// We need to add all the objects we selected into groups with new objects that were in their group before.
									FString GroupName;
									if (FParse::Value(Str, TEXT("GroupActor="), GroupName))
									{
										if (bGroupSupport)
										{
											tmpNewGroup = NewGroups.Find(GroupName);
											bGrouped = true;
										}
									}

									// Does the group exist?
									if (tmpNewGroup)
									{
										AGroupActor* NewActorGroup = *tmpNewGroup;

										// Add it to the group.
										NewActorGroup->Add(*NewActor);
									}
									else if (bGrouped)
									{
										// Create a new group and add the actor.
										FActorSpawnParameters GroupActorSpawnInfo;
										GroupActorSpawnInfo.ObjectFlags = InFlags;
										AGroupActor* SpawnedGroupActor = NewActor->GetWorld()->SpawnActor<AGroupActor>(GroupActorSpawnInfo);
										SpawnedGroupActor->Add(*NewActor);

										// Place the group in the map so we can find it later.
										NewGroups.Add(GroupName, SpawnedGroupActor);
										FActorLabelUtilities::SetActorLabelUnique(SpawnedGroupActor, GroupName);
									}

									// If we're copying a sub-group, add add duplicated group to original parent
									// If we're just copying an actor, only append it to the original parent group if unlocked
									if (Found)
									{
										AGroupActor* FoundParent = Cast<AGroupActor>(Found->GroupActor);
										if (FoundParent && (Found->IsA(AGroupActor::StaticClass()) || !FoundParent->IsLocked()))
										{
											FoundParent->Add(*NewActor);
										}
									}
								}

								// Store the new actor and the text it should be initialized with.
								NewActorMap.Add(NewActor, *PropText);

								// Store the copy to original actor mapping
								MapActors.Add(NewActor, Found);

								// Store the new actor against its source actor name (not the one that may have been made unique)
								if (ActorSourceName != NAME_None)
								{
									NewActorsFNames.Add(ActorSourceName, NewActor);
									if (Found)
									{
										ExistingToNewMap.Add(Found, NewActor);
									}
								}

								// Store the new actor with its parent's FName, and socket FName if applicable
								if (ActorParentName != NAME_None)
								{
									NewActorsAttachmentMap.Add(NewActor, FAttachmentDetail(ActorParentName, ActorParentSocket));
								}
							}
						}
					}

					// increment the number of actors we imported
					ActorIndex++;
				}
			}
			else if (Local::GetBEGIN(&Str, TEXT("SURFACE")))
			{
				UMaterialInterface* SrcMaterial = nullptr;
				FVector SrcBase, SrcTextureU, SrcTextureV, SrcNormal;
				uint32 SrcPolyFlags = PF_DefaultFlags;
				int32 SurfacePropertiesParsed = 0;

				SrcBase = FVector::ZeroVector;
				SrcTextureU = FVector::ZeroVector;
				SrcTextureV = FVector::ZeroVector;
				SrcNormal = FVector::ZeroVector;

				bool bJustParsedTextureName = false;
				bool bFoundSurfaceEnd = false;
				bool bParsedLineSuccessfully = false;

				do
				{
					if (Local::GetEND(&InBuffer, TEXT("SURFACE")))
					{
						bFoundSurfaceEnd = true;
						bParsedLineSuccessfully = true;
					}
					else if (FParse::Command(&InBuffer, TEXT("TEXTURE")))
					{
						InBuffer++;	// Move past the '=' sign

						FString TextureName;
						bParsedLineSuccessfully = FParse::Line(&InBuffer, TextureName, true);
						if (TextureName != TEXT("None"))
						{
							SrcMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *TextureName, nullptr, LOAD_NoWarn, nullptr));
						}
						bJustParsedTextureName = true;
						SurfacePropertiesParsed++;
					}
					else if (FParse::Command(&InBuffer, TEXT("BASE")))
					{
						Local::GetFVECTOR(InBuffer, SrcBase);
						SurfacePropertiesParsed++;
					}
					else if (FParse::Command(&InBuffer, TEXT("TEXTUREU")))
					{
						Local::GetFVECTOR(InBuffer, SrcTextureU);
						SurfacePropertiesParsed++;
					}
					else if (FParse::Command(&InBuffer, TEXT("TEXTUREV")))
					{
						Local::GetFVECTOR(InBuffer, SrcTextureV);
						SurfacePropertiesParsed++;
					}
					else if (FParse::Command(&InBuffer, TEXT("NORMAL")))
					{
						Local::GetFVECTOR(InBuffer, SrcNormal);
						SurfacePropertiesParsed++;
					}
					else if (FParse::Command(&InBuffer, TEXT("POLYFLAGS")))
					{
						FParse::Value(InBuffer, TEXT("="), SrcPolyFlags);
						SurfacePropertiesParsed++;
					}

					// Parse to the next line only if the texture name wasn't just parsed or if the 
					// end of surface isn't parsed. Don't parse to the next line for the texture 
					// name because a FParse::Line() is called when retrieving the texture name. 
					// Doing another FParse::Line() would skip past a necessary surface property.
					if (!bJustParsedTextureName && !bFoundSurfaceEnd)
					{
						FString DummyLine;
						bParsedLineSuccessfully = FParse::Line(&InBuffer, DummyLine);
					}

					// Reset this bool so that we can parse lines starting during next iteration.
					bJustParsedTextureName = false;
				} while (!bFoundSurfaceEnd && bParsedLineSuccessfully);

				// There are 6 BSP surface properties exported via T3D. If there wasn't 6 properties 
				// successfully parsed, the parsing failed. This surface isn't valid then.
				if (SurfacePropertiesParsed == 6)
				{
					const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "PasteTextureToSurface", "Paste Texture to Surface"));

					for (int32 j = 0; j < World->GetNumLevels(); ++j)
					{
						ULevel* CurrentLevel = World->GetLevel(j);
						for (int32 i = 0; i < CurrentLevel->Model->Surfs.Num(); i++)
						{
							FBspSurf* DstSurf = &CurrentLevel->Model->Surfs[i];

							if (DstSurf->PolyFlags & PF_Selected)
							{
								CurrentLevel->Model->ModifySurf(i, 1);

								const FVector DstNormal = CurrentLevel->Model->Vectors[DstSurf->vNormal];

								// Need to compensate for changes in the polygon normal.
								const FRotator SrcRot = SrcNormal.Rotation();
								const FRotator DstRot = DstNormal.Rotation();
								const FRotationMatrix RotMatrix(DstRot - SrcRot);

								FVector NewBase = RotMatrix.TransformPosition(SrcBase);
								FVector NewTextureU = RotMatrix.TransformVector(SrcTextureU);
								FVector NewTextureV = RotMatrix.TransformVector(SrcTextureV);

								DstSurf->Material = SrcMaterial;
								DstSurf->pBase = Local::bspAddPoint(CurrentLevel->Model, &NewBase, 1);
								DstSurf->vTextureU = Local::bspAddVector(CurrentLevel->Model, &NewTextureU, 0);
								DstSurf->vTextureV = Local::bspAddVector(CurrentLevel->Model, &NewTextureV, 0);
								DstSurf->PolyFlags = SrcPolyFlags;

								DstSurf->PolyFlags &= ~PF_Selected;

								CurrentLevel->MarkPackageDirty();

								const bool bUpdateTexCoords = true;
								const bool bOnlyRefreshSurfaceMaterials = false;
								GEditor->polyUpdateMaster(CurrentLevel->Model, i, bUpdateTexCoords, bOnlyRefreshSurfaceMaterials);
							}
						}
					}
				}
			}
			else if (Local::GetBEGIN(&Str, TEXT("MAPPACKAGE")))
			{
				// Get all the text.
				while ((Local::GetEND(&InBuffer, TEXT("MAPPACKAGE")) == 0) && FParse::Line(&InBuffer, StrLine))
				{
					MapPackageText += *StrLine;
					MapPackageText += TEXT("\r\n");
				}
			}
		}

		// Import actor properties.
		// We do this after creating all actors so that actor references can be matched up.
		AWorldSettings* WorldSettings = World->GetWorldSettings();

		if (GIsImportingT3D && (MapPackageText.Len() > 0))
		{
			FName NewPackageName(*(RootMapPackage->GetName()));

			const TCHAR* MapPkg_BufferStart = *MapPackageText;
			const TCHAR* MapPkg_BufferEnd = MapPkg_BufferStart + MapPackageText.Len();
			Local::PackageFactory_FactoryCreateText(UPackage::StaticClass(), nullptr, NewPackageName, RF_NoFlags, 0, TEXT("T3D"), MapPkg_BufferStart, MapPkg_BufferEnd, InWarn);
		}

		// If Apply Custom Properties to StaticMeshComponent
		const bool bStaticMeshComponentCustomPropsHack = PrefabToolSettings->ShouldForceApplyPerInstanceVertexColor();

		// Pass 1: Sort out all the properties on the individual actors	
		bool bIsMoveToStreamingLevel = (FCString::Stricmp(InType, TEXT("move")) == 0);
		for (auto& ActorMapElement : NewActorMap)
		{
			AActor* Actor = ActorMapElement.Key;

			// StaticMeshComponent Instance Data Hack
			TMap<FName, FString> StaticMeshComponentCustomProps;

			// Import properties if the new actor is 
			bool		bActorChanged = false;
			FString*	PropText = &(ActorMapElement.Value);
			if (PropText)
			{
				if (Actor->ShouldImport(PropText, bIsMoveToStreamingLevel))
				{
					Actor->PreEditChange(nullptr);
					Local::ImportObjectProperties((uint8*)Actor, **PropText, Actor->GetClass(), Actor, Actor, InWarn, 0, INDEX_NONE, NULL, &ExistingToNewMap, &InAssetRemapper
						, (bStaticMeshComponentCustomPropsHack ? &StaticMeshComponentCustomProps : NULL)
					);
					bActorChanged = true;

					if (bUpdateSelection)
					{
						GEditor->SelectActor(Actor, true, /*bNotify=*/false, true);
					}
				}
				else // This actor is new, but rejected to import its properties, so just delete...
				{
					Actor->Destroy();
				}
			}
			else
				if (!Actor->IsA(AInstancedFoliageActor::StaticClass()))
				{
					// This actor is old
				}

			// If this is a newly imported brush, validate it.  If it's a newly imported dynamic brush, rebuild it first.
			// Previously, this just called bspValidateBrush.  However, that caused the dynamic brushes which require a valid BSP tree
			// to be built to break after being duplicated.  Calling RebuildBrush will rebuild the BSP tree from the imported polygons.
			ABrush* Brush = Cast<ABrush>(Actor);
			if (bActorChanged && Brush && Brush->Brush)
			{
				const bool bIsStaticBrush = Brush->IsStaticBrush();
				if (!bIsStaticBrush)
				{
					FBSPOps::RebuildBrush(Brush->Brush);
				}

				Local::bspValidateBrush(Brush->Brush, true, false);
			}

			// Copy brushes' model pointers over to their BrushComponent, to keep compatibility with old T3Ds.
			if (Brush && bActorChanged)
			{
				if (Brush->GetBrushComponent()) // Should always be the case, but not asserting so that old broken content won't crash.
				{
					Brush->GetBrushComponent()->Brush = Brush->Brush;

					// We need to avoid duplicating default/ builder brushes. This is done by destroying all brushes that are CSG_Active and are not
					// the default brush in their respective levels.
					if (Brush->IsStaticBrush() && Brush->BrushType == Brush_Default)
					{
						bool bIsDefaultBrush = false;

						// Iterate over all levels and compare current actor to the level's default brush.
						for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
						{
							ULevel* Level = World->GetLevel(LevelIndex);
							if (Level->GetDefaultBrush() == Brush)
							{
								bIsDefaultBrush = true;
								break;
							}
						}

						// Destroy actor if it's a builder brush but not the default brush in any of the currently loaded levels.
						if (!bIsDefaultBrush)
						{
							World->DestroyActor(Brush);

							// Since the actor has been destroyed, skip the rest of this iteration of the loop.
							continue;
						}
					}
				}
			}

			// If the actor was imported . . .
			if (bActorChanged)
			{
				// Let the actor deal with having been imported, if desired.
				Actor->PostEditImport();

				// Notify actor its properties have changed.
				Actor->PostEditChange();

				if (bStaticMeshComponentCustomPropsHack && StaticMeshComponentCustomProps.Num() > 0)
				{
					TArray<UActorComponent*> StaticMeshComponents = Actor->GetComponentsByClass(UStaticMeshComponent::StaticClass());
					for (UActorComponent* ActorComp : StaticMeshComponents)
					{
						if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(ActorComp))
						{
							FName CompName = StaticMeshComp->GetFName();
							if (FString* PropTextPtr = StaticMeshComponentCustomProps.Find(CompName))
							{
								StaticMeshComp->ImportCustomProperties(*(*PropTextPtr), InWarn);
								StaticMeshComp->CachePaintedDataIfNecessary();
							}
						}
					}
				}
			}
		}

		// Pass 2: Sort out any attachment parenting on the new actors now that all actors have the correct properties set
		for (auto It = MapActors.CreateIterator(); It; ++It)
		{
			AActor* const Actor = It.Key();

			// Fixup parenting
			FAttachmentDetail* ActorAttachmentDetail = NewActorsAttachmentMap.Find(Actor);
			if (ActorAttachmentDetail != nullptr)
			{
				AActor* ActorParent = nullptr;
				// Try to find the new copy of the parent
				AActor** NewActorParent = NewActorsFNames.Find(ActorAttachmentDetail->ParentName);
				if (NewActorParent != nullptr)
				{
					ActorParent = *NewActorParent;
				}
				// Parent the actors
				if (ActorParent != nullptr)
				{
					// Make sure our parent isn't selected (would cause GEditor->ParentActors to fail)
					const bool bParentWasSelected = ActorParent->IsSelected();
					if (bParentWasSelected && bUpdateSelection)
					{
						GEditor->SelectActor(ActorParent, false, /*bNotify=*/false, true);
					}

					GEditor->ParentActors(ActorParent, Actor, ActorAttachmentDetail->SocketName);

					if (bParentWasSelected && bUpdateSelection)
					{
						GEditor->SelectActor(ActorParent, true, /*bNotify=*/false, true);
					}
				}
			}
		}

		for (TMap<FName, AActor*>::TIterator It(NewActorsFNames); It; ++It)
		{
			AActor* Actor = It.Value();

			if (Actor && !Actor->IsPendingKillPending())
			{
				OutSpawnActors.Add(Actor);
			}
		}

		// Delete actors with duplicated name
		for (auto& ActorMapElement : NewActorMap)
		{
			AActor* Actor = ActorMapElement.Key;
			if (!OutSpawnActors.Contains(Actor))
			{
				EditorDestroyActor(Actor, false);
			}
		}

		// Go through all the groups we added and finalize them.
		for (TMap< FString, AGroupActor* >::TIterator It(NewGroups); It; ++It)
		{
			AGroupActor* GroupActor = It.Value();
			GroupActor->CenterGroupLocation();
			Local::LockGroupActor(GroupActor);
			if (OutGroupActorsPtr)
			{
				(*OutGroupActorsPtr).Add(GroupActor);
			}
		}

		// Mark us as no longer importing a T3D.
		GEditor->IsImportingT3D = 0;
		GIsImportingT3D = false;

		return World;
	}

////////////////////////////////////////////
// FPrefabActorUtil
//

APrefabActor* FPrefabActorUtil::GetFirstAttachedParentPrefabActor(AActor* InActor)
{
	if (InActor && !InActor->IsPendingKillPending())
	{
		for (AActor* ParentActor = InActor->GetAttachParentActor(); ParentActor && !ParentActor->IsPendingKillPending(); ParentActor = ParentActor->GetAttachParentActor())
		{
			if (APrefabActor* ParentPrefabActor = Cast<APrefabActor>(ParentActor))
			{
				return ParentPrefabActor;
			}
		}
	}
	return nullptr;
}

APrefabActor* FPrefabActorUtil::GetFirstLockedParentPrefabActor(AActor* InActor)
{
	APrefabActor* FirstLockedParentPrefabActor = NULL;
	if (InActor && !InActor->IsPendingKillPending())
	{
		for (AActor* ParentActor = InActor->GetAttachParentActor(); ParentActor && !ParentActor->IsPendingKillPending(); ParentActor = ParentActor->GetAttachParentActor())
		{
			if (APrefabActor* ParentPrefabActor = Cast<APrefabActor>(ParentActor))
			{
				if (ParentPrefabActor->GetLockSelection())
				{
					FirstLockedParentPrefabActor = ParentPrefabActor;
					break;
				}
			}
		}
	}
	return FirstLockedParentPrefabActor;
}

bool FPrefabActorUtil::IsAttachToConnectedChildPrefabActor(const TArray<APrefabActor*>& AllAttachedChildPrefabActors, AActor* InActor)
{
	if (APrefabActor* ParentPrefabActor = GetFirstAttachedParentPrefabActor(InActor))
	{
		if (ParentPrefabActor && ParentPrefabActor->IsConnected()
			&& AllAttachedChildPrefabActors.Contains(ParentPrefabActor))
		{
			return true;
		}
	}
	return false;
}

void FPrefabActorUtil::GetAllParentPrefabActors(AActor* InActor, TArray<APrefabActor*>& OutParentPrefabActors, const APrefabActor* RootPrefabActor)
{
	if (InActor && !InActor->IsPendingKillPending())
	{
		bool bReachRoot = false;
		for (AActor* ParentActor = InActor->GetAttachParentActor(); !bReachRoot && ParentActor && !ParentActor->IsPendingKillPending(); ParentActor = ParentActor->GetAttachParentActor())
		{
			if (ParentActor->IsA(APrefabActor::StaticClass()))
			{
				OutParentPrefabActors.AddUnique(Cast<APrefabActor>(ParentActor));
				if (RootPrefabActor && (ParentActor == RootPrefabActor))
				{
					bReachRoot = true;
				}
			}
		}
	}
}

void FPrefabActorUtil::GetAllParentPrefabAssets(AActor* InActor, TArray<UPrefabAsset*>& OutParentPrefabAssets, const APrefabActor* RootPrefabActor /*= nullptr*/)
{
	if (InActor && !InActor->IsPendingKillPending())
	{
		bool bReachRoot = false;
		for (AActor* ParentActor = InActor->GetAttachParentActor(); !bReachRoot && ParentActor && !ParentActor->IsPendingKillPending(); ParentActor = ParentActor->GetAttachParentActor())
		{
			if (APrefabActor* ParentPrefabActor = Cast<APrefabActor>(ParentActor))
			{
				if (ParentPrefabActor->GetPrefab())
				{
					OutParentPrefabAssets.AddUnique(ParentPrefabActor->GetPrefab());
				}
				if (RootPrefabActor && (ParentActor == RootPrefabActor))
				{
					bReachRoot = true;
				}
			}
		}
	}
}

void FPrefabActorUtil::GetAllAttachedChildren(AActor* Parent, TArray<AActor*>& OutChildActors)
{
	OutChildActors.Empty();

	TArray<USceneComponent*> ChildrenComponents;
	Parent->GetRootComponent()->GetChildrenComponents(/*bIncludeAllDecendants=*/true, ChildrenComponents);

	for (USceneComponent* Child : ChildrenComponents)
	{
		if (Child && !Child->IsPendingKill() && Child->GetOwner()
			&& !(Child->GetOwner() == Parent) && !Child->GetOwner()->IsPendingKillPending())
		{
			OutChildActors.AddUnique(Child->GetOwner());
		}
	}
}

void FPrefabActorUtil::ShowHidePrefabActor(class APrefabActor* InPrefabActor, bool bVisible)
{
	if (!InPrefabActor || InPrefabActor->IsPendingKillPending())
	{
		return;
	}

	TArray<AActor*> AllAttachedChildren;
	FPrefabActorUtil::GetAllAttachedChildren(InPrefabActor, AllAttachedChildren);

	SaveToTransactionBuffer(InPrefabActor, false);
	InPrefabActor->SetIsTemporarilyHiddenInEditor(!bVisible);
	for (AActor* Child : AllAttachedChildren)
	{
		SaveToTransactionBuffer(Child, false);
		Child->SetIsTemporarilyHiddenInEditor(!bVisible);
	}
	GEditor->RedrawLevelEditingViewports();

	// 	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	// 	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	// 	TSharedPtr<SDockTab> SceneOutlinerTab = LevelEditorTabManager->InvokeTab(FTabId("LevelEditorSceneOutliner"));
	// 	if (SceneOutlinerTab.IsValid())
	// 	{
	// 		auto BorderWidget = StaticCastSharedRef<SBorder>(SceneOutlinerTab->GetContent());
	// 		auto SceneOutlinerWidget = StaticCastSharedRef<ISceneOutliner>(BorderWidget->GetContent());
	// 		SceneOutlinerWidget->Refresh();
	// 		GEditor->RedrawLevelEditingViewports();
	// 	}
}

void FPrefabActorUtil::SelectPrefabActorsInWorld(const TArray<TWeakObjectPtr<class UPrefabAsset>>& PrefabAssets)
{
	
	GEditor->SelectNone(/*NoteSelectionChange*/true, /*DeselectBSPSurfs*/true, /*WarnAboutManyActors*/false);

	TArray<AActor*> ToSelectActors;

	for (auto& PrefabAsset : PrefabAssets)
	{
		if (PrefabAsset.IsValid())
		{
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				FScopedSlowTask SlowTask(1, LOCTEXT("SelectPrefabActorsInWorld", "Finding Prefab Actors that use this preab..."));
				SlowTask.MakeDialog();

				SlowTask.EnterProgressFrame();

				for (FActorIterator It(World); It; ++It)
				{
					AActor* Actor = *It;
					if (Actor && !Actor->IsPendingKillPending())
					{
						if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
						{
							if (PrefabAsset.Get() == PrefabActor->GetPrefabComponent()->Prefab)
							{
								ToSelectActors.Add(PrefabActor);
							}
						}
					}
				} // end of ActorIterator for loop
			}
		}
	}

	if (ToSelectActors.Num() > 0)
	{
		for (AActor* Actor : ToSelectActors)
		{
			GEditor->SelectActor(Actor, true, /*bNotify*/ false, /*bSelectEvenIsHidden*/ true);
		}

		GEditor->NoteSelectionChange();
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("NoReferencingActorsFound", "No actors found."));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FPrefabActorUtil::AttachSelectedActorsToPrefabActor()
{
	PREFABTOOL_LOG(Display, TEXT("AttachSelectedActorsToPrefabActor"));

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	if (SelectedActors.Num() >= 2)
	{
		if (APrefabActor* PrefabActor = Cast<APrefabActor>(SelectedActors.Last()))
		{
			if (UPrefabAsset* Prefab = PrefabActor->GetPrefabComponent()->Prefab)
			{
				const FScopedTransaction Transaction(LOCTEXT("AttachToPrefabActor", "Attach to Prefab Actor"));

				const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
				for (int32 Index = 0; Index < SelectedActors.Num() - 1; ++Index)
				{
					AActor* Actor = SelectedActors[Index];
					if (Actor && !Actor->IsPendingKillPending() && IsActorSupported(Actor, PrefabToolSettings))
					{
						FText ReasonText;
						if (FPrefabGEditorAdapter::GEditor_CanParentActors(PrefabActor, Actor, &ReasonText))
						{
							if (APrefabActor* ChildPrefabActor = Cast<APrefabActor>(Actor))
							{
								if (ChildPrefabActor->GetPrefabComponent()->Prefab == Prefab)
								{
									continue;
								}
							}
							Actor->AttachToActor(PrefabActor, FAttachmentTransformRules::KeepWorldTransform, NAME_None);
						}
					}
				}
			}
		}
	}
}

AActor* FPrefabActorUtil::SpawnEmptyActor(ULevel* InLevel, const FTransform& Transform, const FName Name, EObjectFlags InObjectFlags, EComponentMobility::Type InComponentMobility)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags = InObjectFlags;
	SpawnInfo.Name = Name;

	AActor* NewActor = InLevel->OwningWorld->SpawnActor(AActor::StaticClass(), &Transform, SpawnInfo);

	USceneComponent* RootComponent = NewObject<USceneComponent>(NewActor, USceneComponent::GetDefaultSceneRootVariableName(), InObjectFlags);
	RootComponent->Mobility = InComponentMobility;
	RootComponent->bVisualizeComponent = true;
	RootComponent->SetWorldTransform(Transform);

	NewActor->SetRootComponent(RootComponent);
	NewActor->AddInstanceComponent(RootComponent);

	RootComponent->RegisterComponent();

	return NewActor;
}

bool FPrefabActorUtil::IsActorSupported(AActor* Actor, const class UPrefabToolSettings* PrefabToolSettings)
{
	const bool bBSPSupport = PrefabToolSettings->ShouldEnableBSPBrushSupport();

	if (Actor->IsA(AGroupActor::StaticClass()))
	{
		return false;
	}
	if (Actor->IsA(ALandscape::StaticClass()) || Actor->IsA(ALandscapeGizmoActiveActor::StaticClass()))
	{
		return false;
	}

	if (!bBSPSupport)
	{
		if (ABrush* Brush = Cast<ABrush>(Actor))
		{
			if (Brush->IsVolumeBrush())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

void FPrefabActorUtil::SortActorsHierarchy(TArray<AActor*>& Actors, bool bFromParentToChild)
{
	int32 TotalActor = Actors.Num();

	auto CalcAttachDepth = [](AActor* InActor, int32 InTotalActor) -> int32 {
		int32 Depth = MAX_int32;
		if (InActor)
		{
			Depth = 0;
			if (InActor->GetRootComponent())
			{
				for (const USceneComponent* Test = InActor->GetRootComponent()->GetAttachParent(); Test != nullptr; Test = Test->GetAttachParent(), Depth++);
			}
			if (InActor->GetClass()->IsChildOf(AGroupActor::StaticClass()))
			{
				Depth -= InTotalActor;
			}
		}
		return Depth;
	};

	StableSortInternal(Actors.GetData(), TotalActor, [&](AActor* L, AActor* R) {
		return bFromParentToChild ? CalcAttachDepth(L, TotalActor) < CalcAttachDepth(R, TotalActor) : CalcAttachDepth(L, TotalActor) > CalcAttachDepth(R, TotalActor);
	});
}

void FPrefabActorUtil::FilterParentActors(const TArray<AActor*>& InActors, TArray<AActor*>& OutParentActors)
{
	for (AActor* InActor : InActors)
	{
		if (InActor && !InActor->IsPendingKillPending())
		{
			if (InActor->GetAttachParentActor() == NULL)
			{
				OutParentActors.AddUnique(InActor);
				continue;
			}

			bool bIsParent = true;
			for (AActor* ParentActor = InActor->GetAttachParentActor(); ParentActor && !ParentActor->IsPendingKillPending(); ParentActor = ParentActor->GetAttachParentActor())
			{
				if (InActors.Contains(ParentActor))
				{
					bIsParent = false;
					break;
				}
			}
			if (bIsParent)
			{
				OutParentActors.AddUnique(InActor);
			}
		}
	}
}

FBox FPrefabActorUtil::GetAllComponentsBoundingBox(AActor* InActor, bool bRecursive /*= true*/)
{
	FBox ActorBound = InActor->GetComponentsBoundingBox();

	if (bRecursive)
	{
		TArray<AActor*> Children;
		InActor->GetAttachedActors(Children);
		for (AActor* Actor : Children)
		{
			if (Actor && !Actor->IsPendingKillPending())
			{
				FBox Box = GetAllComponentsBoundingBox(Actor, bRecursive);
				if (!FMath::IsNearlyZero(Box.GetVolume(), KINDA_SMALL_NUMBER))
				{
					ActorBound = FMath::IsNearlyZero(ActorBound.GetVolume()) ? Box : ActorBound + Box;
				}
			}
		}
	}

	return ActorBound;
}

///////////////////////////////////////////////////////
// FPrefabToolEditorUtil
//

#if 0
void FPrefabToolEditorUtil::AddSelectedActorToPrefabAsset(class UPrefabAsset* InPrefabAsset)
{
	if (InPrefabAsset && GEditor->GetSelectedActorCount() > 0)
	{
		// Todo: verify selected actors
	}
}
#endif

bool FPrefabToolEditorUtil::bSkipSelectionMonitor = false;

void FPrefabToolEditorUtil::SpawnPrefabInstances(UPrefabAsset* Asset, UWorld* InWorld, TArray<AActor*>& OutPrefabInstances, TArray<AGroupActor*>* OutGroupActorsPtr, EObjectFlags InObjectFlags)
{
	BeginSkipSelectionMonitor();

	UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);
	FString PrefabContent = Prefab->PrefabContent.ToString();
	const TMap<FString, FSoftObjectPath>& AssetRemapper = Prefab->AssetReferences;
	//UE_LOG(LogTemp, Display, TEXT("[FPrefabToolEditorUtil::SpawnPrefabInstances] PrefabTagPrefix: %s"), *PrefabTagPrefix);

	UWorld* SavedWorld = GWorld;
	GWorld = InWorld;

	ABrush::SetSuppressBSPRegeneration(true);
	FPrefabGEditorAdapter::edactPasteSelected(InWorld, /*bDuplicate=*/false, /*bOffsetLocations=*/false, /*bWarnIfHidden=*/true, &PrefabContent, AssetRemapper, OutPrefabInstances, OutGroupActorsPtr, InObjectFlags);
	ABrush::SetSuppressBSPRegeneration(false);

	GWorld = SavedWorld;

	EndSkipSelectionMonitor();
}

void FPrefabToolEditorUtil::PostSpawnPrefabInstances(UPrefabAsset* Prefab, TArray<AActor*>& InSpawnPrefabInstances, APrefabActor* InParentPrefabActor, TArray<class AGroupActor*>* InSpawnGroupActorsPtr)
{
	// Ensure all potential prefab instances (including instances not spawned by current prefab) have prefab instance tag
	TArray<AActor*> PotentialInstanceActors;
	InParentPrefabActor->GetPrefabComponent()->GetAllPotentialChildrenInstanceActors(PotentialInstanceActors);
	const FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();
	EnsureActorsHavePrefabInstanceTag(PotentialInstanceActors, PrefabTagPrefix);

	// Attach prefab instances to prefab actor
	AttachSpawnInstances(Prefab, InSpawnPrefabInstances, InParentPrefabActor);

	// Update prefab component's generated blueprint reference from prefab asset
	if (Prefab->GeneratedBlueprintAssetReference.IsAsset())
	{
		UBlueprint* GeneratedBlueprint = Cast<UBlueprint>(Prefab->GeneratedBlueprintAssetReference.ResolveObject());

		if (GeneratedBlueprint == nullptr) // Not loaded yet
		{
			// Synchronous load
			FStreamableManager Streamable;
			Streamable.RequestSyncLoad(Prefab->GeneratedBlueprintAssetReference);

			// Resolve again
			GeneratedBlueprint = Cast<UBlueprint>(Prefab->GeneratedBlueprintAssetReference.ResolveObject());
		}

		InParentPrefabActor->GetPrefabComponent()->GeneratedBlueprint = GeneratedBlueprint;
	}

	// Rebuild BSP (place holder for BSP supprot)
	//RebuildAlteredBSP(InSpawnPrefabInstances);
	
	// Make sure spawned group is centered
	if (InSpawnGroupActorsPtr)
	{
		CenterGroupActors(*InSpawnGroupActorsPtr);
	}
}

void FPrefabToolEditorUtil::AttachSpawnInstances(UPrefabAsset* Asset, TArray<AActor*>& InSpawnActors, AActor* InParentActor, bool bKeepRelativetransform)
{
	if (InSpawnActors.Num() > 0 && InParentActor)
	{

		// Attach Instances
		{
			UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);
			const FVector PrefabPivot = Prefab->PrefabPivot;
			//UE_LOG(LogTemp, Display, TEXT("AttachSpawnInstances: %s; %s; %s"), *TargetLocation.ToString(), *FromLocation.ToString(), *Offset.ToString());

			struct FAttachData
			{
				FAttachData(AActor* InActor, AActor* InAttachParentActor, FName InSocketName, FTransform& InRelativeTransform)
					: Actor(InActor)
					, ParentActor(InAttachParentActor)
					, SocketName(InSocketName)
					, RelativeTransfrom(InRelativeTransform)
				{}
				AActor* Actor;
				AActor* ParentActor;
				FName SocketName;
				FTransform RelativeTransfrom;
			};

			int32 NumActorsToMove = InSpawnActors.Num();

			TArray<FAttachData> AttachDatas;
			AttachDatas.Reserve(NumActorsToMove);

			TArray<TArray<FAttachData>> AttachChildrenDatas;
			AttachChildrenDatas.Reserve(NumActorsToMove);

			TArray<AGroupActor*> GroupActors;

			TArray<AActor*> SortedSpawnActors = InSpawnActors;
			FPrefabActorUtil::SortActorsHierarchy(SortedSpawnActors, /*bFromParentToChild*/true);

			for (AActor* Actor : SortedSpawnActors)
			{
				AActor* ParentActor = Actor->GetAttachParentActor();
				//UE_LOG(LogTemp, Display, TEXT("[AttachSpawnInstances] Actor:%s -> Parent: %s"), *Actor->GetActorLabel(), (ParentActor == nullptr) ? TEXT("NULL") : *ParentActor->GetActorLabel());
				FName SocketName = Actor->GetAttachParentSocketName();
				FTransform RelativeTransform = Actor->GetRootComponent() ? Actor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
				Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				AttachDatas.Emplace(Actor, ParentActor, SocketName, RelativeTransform);

				TArray<AActor*> AttachedActors;
				Actor->GetAttachedActors(AttachedActors);
				TArray<FAttachData> AttachedActorData;
				AttachedActorData.Reserve(AttachedActors.Num());
				for (int32 AttachedActorIdx = 0; AttachedActorIdx < AttachedActors.Num(); ++AttachedActorIdx)
				{
					AActor* ChildActor = AttachedActors[AttachedActorIdx];
					if (InSpawnActors.Find(ChildActor) == INDEX_NONE)
					{
						RelativeTransform = ChildActor->GetRootComponent() ? ChildActor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
						AttachedActorData.Emplace(ChildActor, Actor, ChildActor->GetAttachParentSocketName(), RelativeTransform);
						ChildActor->Modify();
						ChildActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
					}
				}
				AttachChildrenDatas.Add(AttachedActorData);

				// If this actor is in a group, add it to the list
				if (UActorGroupingUtils::IsGroupingActive())
				{
					AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(Actor, true, true);
					if (ActorGroupRoot)
					{
						GroupActors.AddUnique(ActorGroupRoot);
					}
				}
			}

			// Restore attachments
			for (int32 Index = 0; Index < AttachDatas.Num(); ++Index)
			{
				if (AttachDatas[Index].ParentActor == nullptr)
				{
					AttachDatas[Index].Actor->AttachToActor(InParentActor, FAttachmentTransformRules::KeepWorldTransform);
					if (bKeepRelativetransform)
					{
						FTransform NewRelativeTM(AttachDatas[Index].RelativeTransfrom);
						NewRelativeTM.SetLocation(NewRelativeTM.GetLocation() - PrefabPivot); // Offset by Prefab Pivot
						AttachDatas[Index].Actor->SetActorRelativeTransform(NewRelativeTM);
					}
				}
				else
				{
					AttachDatas[Index].Actor->AttachToActor(AttachDatas[Index].ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachDatas[Index].SocketName);
					if (bKeepRelativetransform)
					{
						AttachDatas[Index].Actor->SetActorRelativeTransform(AttachDatas[Index].RelativeTransfrom);
					}
				}

				for (FAttachData& ChildAttachData : AttachChildrenDatas[Index])
				{
					if (ChildAttachData.ParentActor != nullptr)
					{
						ChildAttachData.Actor->AttachToActor(ChildAttachData.ParentActor, FAttachmentTransformRules::KeepWorldTransform, ChildAttachData.SocketName);
						if (bKeepRelativetransform)
						{
							ChildAttachData.Actor->SetActorRelativeTransform(ChildAttachData.RelativeTransfrom);
						}
						ChildAttachData.Actor->PostEditMove(true);
					}
				}

				//UE_LOG(LogTemp, Display, TEXT("[AttachSpawnInstances] Actor:%s -> Parent: %s"), *Actor->GetActorLabel(), (AttachData[Index].ParentActor == nullptr) ? TEXT("NULL") : *AttachData[Index].ParentActor->GetActorLabel());
				AttachDatas[Index].Actor->PostEditMove(true);
			}

			// If grouping is active, go through the unique group actors and update the group actor location
			if (UActorGroupingUtils::IsGroupingActive())
			{
				for (AGroupActor* GroupActor : GroupActors)
				{
					GroupActor->CenterGroupLocation();
				}
			}
		}
	}
}

void FPrefabToolEditorUtil::edactCopySelectedForNewPrefab(UWorld* InWorld, FString* DestinationData, const FString& InPrefabTagPrefix, const FVector& LastSelectedPivot)
{
	BeginSkipSelectionMonitor();

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	struct Local
	{
		static void LogInvalidAcor(AActor* Actor)
		{
			if (1)
			{
				if (Actor && !Actor->IsPendingKillPending())
				{
					PREFABTOOL_LOG(Display, TEXT("\tSkip %s: %s"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
				}
				else
				{
					PREFABTOOL_LOG(Display, TEXT("\tSkip actor been deleting."));
				}
			}
		}
	};

	const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();

	TArray<AActor*> ValidActors;
	TArray<AActor*> InValidActors;
	for (AActor* Actor : SelectedActors)
	{
		if (Actor && !Actor->IsPendingKillPending() && FPrefabActorUtil::IsActorSupported(Actor, PrefabToolSettings))
		{
			ValidActors.Add(Actor);

			// Select Prefab Children Instance Actors too
			if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
			{
				TArray<AActor*> ChildrenActors;
				PrefabActor->GetPrefabComponent()->GetAllPotentialChildrenInstanceActors(ChildrenActors);
				for (AActor* ChildActor : ChildrenActors)
				{
					if (ChildActor && !ChildActor->IsPendingKillPending() && FPrefabActorUtil::IsActorSupported(ChildActor, PrefabToolSettings)
						&& !SelectedActors.Contains(ChildActor))
					{
						ValidActors.Add(ChildActor);
						GEditor->SelectActor(ChildActor, /*bInSelected*/ true, /*bNotify*/ false, /*bSelectEvenifHidden*/true, /*bForceRefresh*/ false);
					}
				}
			}
		}
		else
		{
			InValidActors.Add(Actor);
			GEditor->SelectActor(Actor, false, /*bNotify*/false);
			Local::LogInvalidAcor(Actor);
		}
	}

	if (ValidActors.Num() > 0)
	{
		PREFABTOOL_LOG(Display, TEXT("  => %d actors copied for new prefab."), ValidActors.Num());

		AActor* LastSelectedActor = ValidActors.Last();
		if (LastSelectedActor)
		{
			TMap < AActor*, TArray<FName>> ValidActorTagsBackup;
			FPrefabTagUtil::BackupActorTags(ValidActors, ValidActorTagsBackup);

			ValidatePrefabInstancesTag(ValidActors);
			EnsureActorsHavePrefabInstanceTag(ValidActors, InPrefabTagPrefix);

			// Detach
			struct FAttachData
			{
				FAttachData(AActor* InActor, AActor* InParentActor, FName InSocketName, FTransform& InRelativeTransform, FTransform& InWorldTransfrom)
					: Actor(InActor)
					, ParentActor(InParentActor)
					, SocketName(InSocketName)
					, RelativeTransfrom(InRelativeTransform)
					, WorldTransfrom(InWorldTransfrom)
				{}
				AActor* Actor;
				AActor* ParentActor;
				FName SocketName;
				FTransform RelativeTransfrom;
				FTransform WorldTransfrom;
			};

			// Cache AttachData and Offset by Pivot
			TArray<FAttachData> AttachDatas;
			{
				TArray<AActor*> ValidParentActors;
				FPrefabActorUtil::FilterParentActors(ValidActors, ValidParentActors);
				for (AActor* InstanceActor : ValidParentActors)
				{
					if (InstanceActor && !InstanceActor->IsPendingKillPending())
					{
						FName SocketName = InstanceActor->GetAttachParentSocketName();
						AActor* ParentActor = InstanceActor->GetAttachParentActor();

						FTransform RelativeTransform = InstanceActor->GetRootComponent() ? InstanceActor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
						FTransform WorldTransfrom = InstanceActor->GetRootComponent() ? InstanceActor->GetRootComponent()->GetComponentTransform() : FTransform::Identity;

						// detach
						if (ParentActor)
						{
							InstanceActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
						}

						// offset
						InstanceActor->TeleportTo(InstanceActor->GetActorLocation() - LastSelectedPivot, InstanceActor->GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ true);

						AttachDatas.Emplace(InstanceActor, ParentActor, SocketName, RelativeTransform, WorldTransfrom);
					}
				}
			}

			GEditor->edactCopySelected(LastSelectedActor->GetWorld(), DestinationData);

			// Re-attach
			for (int32 Index = 0; Index < AttachDatas.Num(); ++Index)
			{
				if (AttachDatas[Index].ParentActor != nullptr)
				{
					AttachDatas[Index].Actor->AttachToActor(AttachDatas[Index].ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachDatas[Index].SocketName);
				}
				AttachDatas[Index].Actor->SetActorLocation(AttachDatas[Index].WorldTransfrom.GetLocation(), /*bSweep*/false);
				AttachDatas[Index].Actor->PostEditMove(true);
			}

			FPrefabTagUtil::RestoreActorTags(ValidActors, ValidActorTagsBackup);
		}
	}

	if (InValidActors.Num() > 0)
	{
		for (AActor* Actor : InValidActors)
		{
			GEditor->SelectActor(Actor, true, /*bNotify=*/false);
		}
	}

	EndSkipSelectionMonitor();
}

void FPrefabToolEditorUtil::RebuildAlteredBSP(const TArray<AActor*> InActors)
{
	const bool bBSPSupport = GetDefault<UPrefabToolSettings>()->ShouldEnableBSPBrushSupport();
	if (!bBSPSupport)
	{
		return;
	}

	if (!GIsTransacting)
	{
		// Early out if BSP auto-updating is disabled
		if (!GetDefault<ULevelEditorMiscSettings>()->bBSPAutoUpdate)
		{
			return;
		}

		FlushRenderingCommands();

		// A list of all the levels that need to be rebuilt
		TArray< TWeakObjectPtr< ULevel > > LevelsToRebuild;
		ABrush::NeedsRebuild(&LevelsToRebuild);

		// Determine which levels need to be rebuilt
		for (AActor* Actor : InActors)
		{
			if (Actor && !Actor->IsPendingKillPending())
			{
				ABrush* SelectedBrush = Cast< ABrush >(Actor);
				if (SelectedBrush && !FActorEditorUtils::IsABuilderBrush(Actor))
				{
					ULevel* Level = SelectedBrush->GetLevel();
					if (Level)
					{
						LevelsToRebuild.AddUnique(Level);
					}
				}
				else
				{
					// In addition to any selected brushes, any brushes attached to a selected actor should be rebuilt
					TArray<AActor*> AttachedActors;
					Actor->GetAttachedActors(AttachedActors);

					const bool bExactClass = true;
					TArray<AActor*> AttachedBrushes;
					// Get any brush actors attached to the selected actor
					if (ContainsObjectOfClass(AttachedActors, ABrush::StaticClass(), bExactClass, &AttachedBrushes))
					{
						for (int32 BrushIndex = 0; BrushIndex < AttachedBrushes.Num(); ++BrushIndex)
						{
							ULevel* Level = CastChecked<ABrush>(AttachedBrushes[BrushIndex])->GetLevel();
							if (Level)
							{
								LevelsToRebuild.AddUnique(Level);
							}
						}
					}
				}
			}
		}

		// Rebuild the levels
		{
			FScopedSlowTask SlowTask(LevelsToRebuild.Num(), NSLOCTEXT("EditorServer", "RebuildingBSP", "Rebuilding BSP..."));
			SlowTask.MakeDialogDelayed(3.0f);

			for (int32 LevelIdx = 0; LevelIdx < LevelsToRebuild.Num(); ++LevelIdx)
			{
				SlowTask.EnterProgressFrame(1.0f);

				TWeakObjectPtr< ULevel > LevelToRebuild = LevelsToRebuild[LevelIdx];
				if (LevelToRebuild.IsValid())
				{
					GEditor->RebuildLevel(*LevelToRebuild.Get());
				}
			}
		}

		GEditor->RedrawLevelEditingViewports();

		ABrush::OnRebuildDone();
	}
	else
	{
		ensureMsgf(0, TEXT("Rebuild BSP ignored during undo/redo"));
		ABrush::OnRebuildDone();
	}
}

namespace ReattachActorsHelper
{
	/** Holds the actor and socket name for attaching. */
	struct FActorAttachmentInfo
	{
		AActor* Actor;

		FName SocketName;

		FTransform RelativeTransform;
	};

	/** Used to cache the attachment info for an actor. */
	struct FActorAttachmentCache
	{
	public:
		/** The post-conversion actor. */
		AActor* NewActor;

		/** The parent actor and socket. */
		FActorAttachmentInfo ParentActor;

		/** Children actors and the sockets they were attached to. */
		TArray<FActorAttachmentInfo> AttachedActors;
	};

	/**
	* Caches the attachment info for the actors being converted.
	*
	* @param InActorsToReplace			List of actors to reattach.
	* @param InOutAttachmentInfo			List of attachment info for the list of actors.
	*/
	void CacheAttachments(const TArray<AActor*>& InActorsToReplace, TArray<FActorAttachmentCache>& InOutAttachmentInfo)
	{
		for (int32 ActorIdx = 0; ActorIdx < InActorsToReplace.Num(); ++ActorIdx)
		{
			AActor* ActorToReattach = InActorsToReplace[ActorIdx];

			InOutAttachmentInfo.AddZeroed();

			FActorAttachmentCache& CurrentAttachmentInfo = InOutAttachmentInfo[ActorIdx];

			// Retrieve the list of attached actors.
			TArray<AActor*> AttachedActors;
			ActorToReattach->GetAttachedActors(AttachedActors);

			// Cache the parent actor and socket name.
			CurrentAttachmentInfo.ParentActor.Actor = ActorToReattach->GetAttachParentActor();
			CurrentAttachmentInfo.ParentActor.SocketName = ActorToReattach->GetAttachParentSocketName();

			// Required to restore attachments properly.
			for (int32 AttachedActorIdx = 0; AttachedActorIdx < AttachedActors.Num(); ++AttachedActorIdx)
			{
				// Store the attached actor and socket name in the cache.
				CurrentAttachmentInfo.AttachedActors.AddZeroed();
				CurrentAttachmentInfo.AttachedActors[AttachedActorIdx].Actor = AttachedActors[AttachedActorIdx];
				CurrentAttachmentInfo.AttachedActors[AttachedActorIdx].SocketName = AttachedActors[AttachedActorIdx]->GetAttachParentSocketName();
				FTransform RelativeTransform = AttachedActors[AttachedActorIdx]->GetRootComponent() ? AttachedActors[AttachedActorIdx]->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
				CurrentAttachmentInfo.AttachedActors[AttachedActorIdx].RelativeTransform = RelativeTransform;

				AActor* ChildActor = CurrentAttachmentInfo.AttachedActors[AttachedActorIdx].Actor;
				ChildActor->Modify();
				ChildActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}

			// Modify the actor so undo will reattach it.
			ActorToReattach->Modify();
			ActorToReattach->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	/**
	* Caches the actor old/new information, mapping the old actor to the new version for easy look-up and matching.
	*
	* @param InOldActor			The old version of the actor.
	* @param InNewActor			The new version of the actor.
	* @param InOutReattachmentMap	Map object for placing these in.
	* @param InOutAttachmentInfo	Update the required attachment info to hold the Converted Actor.
	*/
	void CacheActorConvert(AActor* InOldActor, AActor* InNewActor, TMap<AActor*, AActor*>& InOutReattachmentMap, FActorAttachmentCache& InOutAttachmentInfo)
	{
		// Add mapping data for the old actor to the new actor.
		InOutReattachmentMap.Add(InOldActor, InNewActor);

		// Set the converted actor so re-attachment can occur.
		InOutAttachmentInfo.NewActor = InNewActor;
	}
	
	/**
	* Checks if two actors can be attached, creates Message Log messages if there are issues.
	*
	* @param InParentActor			The parent actor.
	* @param InChildActor			The child actor.
	* @param InOutErrorMessages	Errors with attaching the two actors are stored in this array.
	*
	* @return Returns true if the actors can be attached, false if they cannot.
	*/
	bool CanParentActors(AActor* InParentActor, AActor* InChildActor)
	{
		FText ReasonText;
		if (FPrefabGEditorAdapter::GEditor_CanParentActors(InParentActor, InChildActor, &ReasonText))
		{
			return true;
		}
		else
		{
			FMessageLog("EditorErrors").Error(ReasonText);
			return false;
		}
	}

	/**
	* Reattaches actors to maintain the hierarchy they had previously using a conversion map and an array of attachment info. All errors displayed in Message Log along with notifications.
	*
	* @param InReattachmentMap			Used to find the corresponding new versions of actors using an old actor pointer.
	* @param InAttachmentInfo			Holds parent and child attachment data.
	*/
	void ReattachActors(TMap<AActor*, AActor*>& InReattachmentMap, TArray<FActorAttachmentCache>& InAttachmentInfo, bool bSilence = true)
	{
		// Holds the errors for the message log.
		FMessageLog EditorErrors("EditorErrors");
		if (!bSilence)
		{
			EditorErrors.NewPage(LOCTEXT("AttachmentLogPage", "Actor Reattachment"));
		}

		for (int32 ActorIdx = 0; ActorIdx < InAttachmentInfo.Num(); ++ActorIdx)
		{
			FActorAttachmentCache& CurrentAttachment = InAttachmentInfo[ActorIdx];

			// Need to reattach all of the actors that were previously attached.
			for (int32 AttachedIdx = 0; AttachedIdx < CurrentAttachment.AttachedActors.Num(); ++AttachedIdx)
			{
				// Check if the attached actor was converted. If it was it will be in the TMap.
				AActor** CheckIfConverted = InReattachmentMap.Find(CurrentAttachment.AttachedActors[AttachedIdx].Actor);
				if (CheckIfConverted)
				{
					// This should always be valid.
					if (*CheckIfConverted)
					{
						AActor* ParentActor = CurrentAttachment.NewActor;
						AActor* ChildActor = *CheckIfConverted;

						if (CanParentActors(ParentActor, ChildActor))
						{
							// Attach the previously attached and newly converted actor to the current converted actor.
							ChildActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform, CurrentAttachment.AttachedActors[AttachedIdx].SocketName);
						}
					}

				}
				else
				{
					AActor* ParentActor = CurrentAttachment.NewActor;
					AActor* ChildActor = CurrentAttachment.AttachedActors[AttachedIdx].Actor;

					if (ParentActor && ChildActor && CanParentActors(ParentActor, ChildActor))
					{
						// Since the actor was not converted, reattach the unconverted actor.
						ChildActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform, CurrentAttachment.AttachedActors[AttachedIdx].SocketName);						
					}
				}

			}
		}

		// Add the errors to the message log, notifications will also be displayed as needed.
		if (!bSilence)
		{
			EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));
		}
	}
}

void FPrefabToolEditorUtil::ReplacePrefabInstances(class UPrefabAsset* Prefab, const TMap<FName, AActor*>& OldPrefabInstancesMap, TMap<FName, AActor*>& NewPrefabInstancesMap, TArray<AActor*>* OldActorsToDestroyPtr)
{
	FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

	TArray<AActor*> ActorsToReplace;
	for (TMap<FName, AActor*>::TConstIterator It(OldPrefabInstancesMap); It; ++It)
	{
		AActor* OldActor = It.Value();
		if (OldActor)
		{
			ActorsToReplace.Add(OldActor);
		}
	}

	// Cache for attachment info of all actors being converted.
	TArray<ReattachActorsHelper::FActorAttachmentCache> AttachmentInfo;

	// Maps actors from old to new for quick look-up.
	TMap<AActor*, AActor*> ConvertedMap;

	// Cache the current attachment states.
	ReattachActorsHelper::CacheAttachments(ActorsToReplace, AttachmentInfo);

	TArray<AActor*> OldActorsToDestroy;
	for (int32 ActorIdx = 0; ActorIdx < ActorsToReplace.Num(); ++ActorIdx)
	{
		AActor* OldActor = ActorsToReplace[ActorIdx];
		UWorld* World = OldActor->GetWorld();
		ULevel* Level = OldActor->GetLevel();
		AActor* NewActor = NULL;

		bool bOldActorRenamed = false;
		ERenameFlags RenameFlags = REN_DontCreateRedirectors;
		ERenameFlags RenameTestFlags = REN_Test | REN_DoNotDirty | REN_NonTransactional;

		// rename old actor to "*_REPLACED"
		const FName OldActorName = OldActor->GetFName();
		FName OldActorReplacedNamed = MakeUniqueObjectName(OldActor->GetOuter(), OldActor->GetClass(), *FString::Printf(TEXT("%s_REPLACED"), *OldActorName.ToString()));
		bool bCanRename = OldActor->Rename(*OldActorReplacedNamed.ToString(), NULL, RenameFlags | RenameTestFlags);
		if (bCanRename)
		{
			OldActor->Rename(*OldActorReplacedNamed.ToString(), NULL, RenameFlags);
			bOldActorRenamed = true;
		}
		else
		{
			// Unable to rename, use global unique name instead
			OldActorReplacedNamed = MakeUniqueObjectName(ANY_PACKAGE, OldActor->GetClass(), *FString::Printf(TEXT("%s_REPLACED"), *OldActorName.ToString()));
			bCanRename = OldActor->Rename(*OldActorReplacedNamed.ToString(), NULL, RenameFlags | REN_Test | REN_DoNotDirty | REN_NonTransactional);
			if (bCanRename)
			{
				OldActor->Rename(*OldActorReplacedNamed.ToString(), NULL, RenameFlags);
				bOldActorRenamed = true;
			}
			else
			{
				PREFABTOOL_LOG(Error, TEXT("[ReplacePrefabInstances] Failed to rename old actor from %s to %s"), *OldActor->GetFName().ToString(), *OldActorReplacedNamed.ToString());
			}
		}

		const FTransform OldTransform = OldActor->ActorToWorld();

		// find new actor
		if (const FName* OldInstanceTag = OldPrefabInstancesMap.FindKey(OldActor))
		{
			if (AActor** NewActorPtr = NewPrefabInstancesMap.Find(*OldInstanceTag))
			{
				NewActor = *NewActorPtr;
			}
		}

		// WIP: For blueprints, try to copy over properties
// 		if (UBlueprint* Blueprint = OldActor->GetClass()->ClassGeneratedBy)
// 		{
// 			// Only try to copy properties if this blueprint is based on the actor
// 			UClass* OldActorClass = OldActor->GetClass();
// 			if (Blueprint->GeneratedClass->IsChildOf(OldActorClass) && NewActor != NULL)
// 			{
// 				NewActor->UnregisterAllComponents();
// 				UEditorEngine::CopyPropertiesForUnrelatedObjects(OldActor, NewActor);
// 				NewActor->RegisterAllComponents();
// 			}
//  	}

		if (NewActor)
		{
			// Rename new actor to old actor's name
			if (bOldActorRenamed)
			{
				bCanRename = NewActor->Rename(*OldActorName.ToString(), NULL, RenameFlags | RenameTestFlags);
				if (bCanRename)
				{
					NewActor->Rename(*OldActorName.ToString(), NULL, RenameFlags);
				}
				else
				{
					PREFABTOOL_LOG(Error, TEXT("[ReplacePrefabInstances] Failed to rename new actor from %s to %s"), *NewActor->GetFName().ToString(), *OldActorName.ToString());
				}
			}

			// Merge Instances Tags from old actor to new actor
			{
				FPrefabTagUtil::MergeActorInstanceTags(OldActor->Tags, NewActor->Tags, PrefabTagPrefix);
			}

			// Allow actor derived classes a chance to replace properties.
			// NewActor->EditorReplacedActor(OldActor);
			
			// Caches information for finding the new actor using the pre-converted actor.
			ReattachActorsHelper::CacheActorConvert(OldActor, NewActor, ConvertedMap, AttachmentInfo[ActorIdx]);
		
			// WIP: Find compatible static mesh components and copy instance colors between them.
			// UStaticMeshComponent* NewActorStaticMeshComponent = NewActor->FindComponentByClass<UStaticMeshComponent>();
			// UStaticMeshComponent* OldActorStaticMeshComponent = OldActor->FindComponentByClass<UStaticMeshComponent>();
			// if (NewActorStaticMeshComponent != NULL && OldActorStaticMeshComponent != NULL)
			// {
			// 	NewActorStaticMeshComponent->CopyInstanceVertexColorsIfCompatible(OldActorStaticMeshComponent);
			// }

			// Todo: Should NewActor Invalidate Lighting Cache now?
			// 	NewActor->InvalidateLightingCache();
			// 	NewActor->PostEditMove(true);
			// 	NewActor->MarkPackageDirty();

			// Replace references in the level script Blueprint with the new Actor
			const bool bDontCreate = true;
			ULevelScriptBlueprint* LSB = NewActor->GetLevel()->GetLevelScriptBlueprint(bDontCreate);
			if (LSB)
			{
				// Only if the level script blueprint exists would there be references.  
				FBlueprintEditorUtils::ReplaceAllActorRefrences(LSB, OldActor, NewActor);
			}

			if (OldActorsToDestroyPtr)
			{
				(*OldActorsToDestroyPtr).AddUnique(OldActor);
			}
			else
			{
				FPrefabGEditorAdapter::EditorDestroyActor(OldActor, true);
			}
		}
		else
		{
			// If no matching new Actor found, put the old Actor's name back
			// Destroy now?
			if (bOldActorRenamed)
			{
				bCanRename = OldActor->Rename(*OldActorName.ToString(), NULL, RenameFlags | RenameTestFlags);
				if (bCanRename)
				{
					OldActor->Rename(*OldActorName.ToString(), NULL, RenameFlags);
				}
			}
		}
	}

	if (ConvertedMap.Num() > 0)
	{
		// Reattaches actors based on their previous parent child relationship.
		ReattachActorsHelper::ReattachActors(ConvertedMap, AttachmentInfo);

		// Perform reference replacement on all Actors referenced by World
		UWorld* CurrentEditorWorld = GEditor->GetEditorWorldContext().World();
		FArchiveReplaceObjectRef<AActor> Ar(CurrentEditorWorld, ConvertedMap, /*bNullPrivateRefs*/false, /*bIgnoreOuterRef*/true, /*bIgnoreArchetypeRef*/false);

		// Go through modified objects, marking their packages as dirty and informing them of property changes
		for (const auto& MapItem : Ar.GetReplacedReferences())
		{
			UObject* ModifiedObject = MapItem.Key;

			if (!ModifiedObject->HasAnyFlags(RF_Transient) && ModifiedObject->GetOutermost() != GetTransientPackage() && !ModifiedObject->RootPackageHasAnyFlags(PKG_CompiledIn))
			{
				ModifiedObject->MarkPackageDirty();
			}

			for (UProperty* Property : MapItem.Value)
			{
				FPropertyChangedEvent PropertyEvent(Property);
				ModifiedObject->PostEditChangeProperty(PropertyEvent);
			}
		}
	}
}

/** Generates a reference graph of the world and can then find actors referencing specified objects */
struct WorldReferenceGenerator : public FFindReferencedAssets
{
	void BuildReferencingData()
	{
		MarkAllObjects();

		const int32 MaxRecursionDepth = 0;
		const bool bIncludeClasses = true;
		const bool bIncludeDefaults = false;
		const bool bReverseReferenceGraph = true;


		UWorld* World = GWorld;

		// Generate the reference graph for the world
		FReferencedAssets* WorldReferencer = new(Referencers)FReferencedAssets(World);
		FFindAssetsArchive(World, WorldReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);

		// Also include all the streaming levels in the results
		for (int32 LevelIndex = 0; LevelIndex < World->StreamingLevels.Num(); ++LevelIndex)
		{
			ULevelStreaming* StreamingLevel = World->StreamingLevels[LevelIndex];
			if (StreamingLevel != NULL)
			{
				ULevel* Level = StreamingLevel->GetLoadedLevel();
				if (Level != NULL)
				{
					// Generate the reference graph for each streamed in level
					FReferencedAssets* LevelReferencer = new(Referencers) FReferencedAssets(Level);
					FFindAssetsArchive(Level, LevelReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);
				}
			}
		}
	}

	void MarkAllObjects()
	{
		// Mark all objects so we don't get into an endless recursion
		for (FObjectIterator It; It; ++It)
		{
			It->Mark(OBJECTMARK_TagExp);
		}
	}

	void Generate(const UObject* AssetToFind, TArray< TWeakObjectPtr<const UObject> >& OutObjects)
	{
		// Don't examine visited objects
		if (!AssetToFind->HasAnyMarks(OBJECTMARK_TagExp))
		{
			return;
		}

		AssetToFind->UnMark(OBJECTMARK_TagExp);

		// Return once we find a parent object that is an actor
		if (AssetToFind->IsA(AActor::StaticClass()))
		{
			OutObjects.Add(AssetToFind);
			return;
		}

		// Transverse the reference graph looking for actor objects
		TSet<UObject*>* ReferencingObjects = ReferenceGraph.Find(AssetToFind);
		if (ReferencingObjects)
		{
			for (TSet<UObject*>::TConstIterator SetIt(*ReferencingObjects); SetIt; ++SetIt)
			{
				Generate(*SetIt, OutObjects);
			}
		}
	}
};

void FPrefabToolEditorUtil::RevertPrefabActor(APrefabActor* PrefabActor, bool bRevertOnlyIfPrefabChanged /*=false*/)
{
	if (!GEditor)
	{
		return;
	}

	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent == NULL || !PrefabComponent->GetConnected())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RevertPrefabActor", "Revert Prefab Actor"));

	DoRevertPrefabActor(PrefabActor, /*bRevertingDisconnectedPrefab*/ false, bRevertOnlyIfPrefabChanged, 0);
}

void FPrefabToolEditorUtil::RevertPrefabActorEvenDisconnected(APrefabActor* PrefabActor, bool bRevertOnlyIfPrefabChanged /*=false*/)
{
	if (!GEditor)
	{
		return;
	}

	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent == NULL)
	{
		return;
	}

	const bool bRevertingDisconnectedPrefab = !PrefabComponent->GetConnected();

	const FScopedTransaction Transaction(LOCTEXT("RevertPrefabActor", "Revert Prefab Actor"));

	DoRevertPrefabActor(PrefabActor, bRevertingDisconnectedPrefab, bRevertOnlyIfPrefabChanged, 0);
}

void FPrefabToolEditorUtil::DoRevertPrefabActor(APrefabActor* PrefabActor, bool bRevertingDisconnectedPrefab /*= false*/, bool bRevertOnlyIfPrefabChanged /*=false*/, int32 Depth /*= 0*/)
{
	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent == NULL)
	{
		return;
	}

	const bool bConnected = PrefabComponent->GetConnected();
	
	if (!bConnected && !bRevertingDisconnectedPrefab)
	{
		return;
	}

	if (!bConnected && bRevertingDisconnectedPrefab && Depth > 0)
	{
		return;
	}

	UPrefabAsset* Prefab = PrefabComponent->GetPrefab();
	if (Prefab == NULL)
	{
		return;
	}

	BeginSkipSelectionMonitor();

	// Circular nested prefab check
	static TArray<UPrefabAsset*> RevertingPrefabs;
	static UPrefabAsset* RootPrefab = NULL;

	if (Depth == 0)
	{
		RevertingPrefabs.Empty();
		RootPrefab = Prefab;
		RevertingPrefabs.Add(RootPrefab);
	}

	struct InstancesMapInfo
	{
		APrefabActor* PrefabActor;
		TMap<FName, AActor*> InstancesMap;
		TArray<AActor*> Instances;
		TArray<APrefabActor*> ParentPrefabActors;
		int32 Depth;

		InstancesMapInfo(APrefabActor* InPrefabActor, int32 InDepth)
			: PrefabActor(InPrefabActor)
			, Depth(InDepth)
		{
			InstancesMap = InPrefabActor->GetPrefabComponent()->GetPrefabInstancesMap();

			TArray<AActor*> ChildInstances;
			for (TMap<FName, AActor*>::TConstIterator It(InstancesMap); It; ++It)
			{
				ChildInstances.Add(It.Value());
			}
			Instances = ChildInstances;

			FPrefabActorUtil::GetAllParentPrefabActors(InPrefabActor, ParentPrefabActors);
		}
	};

	struct Local
	{
		static void DestroyActors(const TArray<AActor*>& ActorsToDestroy)
		{
			for (AActor* Actor : ActorsToDestroy)
			{
				if (Actor && !Actor->IsPendingKillPending())
				{
					FPrefabGEditorAdapter::EditorDestroyActor(Actor, true);
				}
			}
		}

		static void DeleteNotAttached(AActor* ParentActor, const TArray<AActor*>& ChildActors)
		{
			for (AActor* Child : ChildActors)
			{
				if (Child && !Child->IsPendingKillPending())
				{
					if (!Child->IsAttachedTo(ParentActor))
					{
						FPrefabGEditorAdapter::EditorDestroyActor(Child, true);
					}
				}
			}
		}

		static void MergeChildInstancesMap(TMap<FName, AActor*>& ParentPrefabInstancesMap, const TMap<FName, AActor*>& ChildPrefabInstancesMap, const FString& PrefabTagPrefix)
		{
			TArray<AActor*> ChildInstances;
			ChildPrefabInstancesMap.GenerateValueArray(ChildInstances);
			EnsureActorsHavePrefabInstanceTag(ChildInstances, PrefabTagPrefix);

			for (TMap<FName, AActor*>::TConstIterator It(ChildPrefabInstancesMap); It; ++It)
			{
				AActor* ChildInstance = It.Value();
				FName InstanceTag = FPrefabTagUtil::GetPrefabInstanceTag(ChildInstance->Tags, PrefabTagPrefix);
				if ((InstanceTag != NAME_None) && !ParentPrefabInstancesMap.Contains(InstanceTag))
				{
					ParentPrefabInstancesMap.Add(InstanceTag, ChildInstance);
				}
			}
		}
	};

	TArray<AActor*> OldAttachedChildren;

	// Backup current level of owning world
	UWorld* OwningWorld = PrefabActor->GetLevel()->OwningWorld;
	ULevel* OldWorldCurrentLevel = OwningWorld->GetCurrentLevel();

	FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

	const bool bShouldRevert = bRevertOnlyIfPrefabChanged ? PrefabComponent->IsPrefabContentChanged(/*bRecursive =*/ true) : true;
	if (bShouldRevert)
	{
		PREFABTOOL_LOG(Display, TEXT("%sReverting %s."), *FString(TEXT("")).LeftPad(Depth * 2), *PrefabActor->GetActorLabel());
	}
	else
	{
		PREFABTOOL_LOG(Display, TEXT("%sAbort reverting %s, prefab content changed? %d"), *FString(TEXT("")).LeftPad(Depth * 2), *PrefabActor->GetActorLabel(), PrefabComponent->IsPrefabContentChanged(/*bRecursive =*/ true));
	}

	if (bShouldRevert)
	{
		PrefabComponent->Modify(); 

		// Backup PrefabComponent Mobility
		EComponentMobility::Type PrefabComponentMobility = PrefabComponent->Mobility;

		// Set PrefabComponent Mobility Temporarily to Static for attachment
		const bool bNeedUpdateComponentMobility = PrefabComponent->Mobility != EComponentMobility::Static;
		if (bNeedUpdateComponentMobility)
		{
			PrefabComponent->SetMobility(EComponentMobility::Static);
		}

		// Store old direct attached children
		TArray<AActor*> OldDirectAttachedInstances;
		PrefabComponent->GetDirectAttachedInstanceActors(OldDirectAttachedInstances);

		// Store old all attached children
		FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, OldAttachedChildren);

		// Set current level temporarily to PrefabActor's level to avoid spawning in current streaming level
		OwningWorld->SetCurrentLevel(PrefabActor->GetLevel());

		// Spawn prefab instances
		TArray<AActor*> SpawnInstances;
		TArray<AGroupActor*> NewGroupActors;
		SpawnPrefabInstances(Prefab, OwningWorld, SpawnInstances, &NewGroupActors);
		PostSpawnPrefabInstances(Prefab, SpawnInstances, PrefabActor, &NewGroupActors);

		// Gather new spawned prefab instances map
		TMap<FName, AActor*> SpawnPrefabInstancesMap;
		GatherPrefabInstancesMap(Prefab, PrefabActor, SpawnInstances, &SpawnPrefabInstancesMap, /*bDeleteInvalid=*/true);

		// Replace old instances according prefab instances map
		PrefabComponent->ValidatePrefabInstancesMap();
		TMap<FName, AActor*> OldPrefabInstancesMap = PrefabComponent->GetPrefabInstancesMap();
		ReplacePrefabInstances(Prefab, OldPrefabInstancesMap, SpawnPrefabInstancesMap, nullptr);

		// Delete old direct attached instances if not been replaced already
		// Todo: If covered by DeleteInvalidAttachedChildren?
		Local::DestroyActors(OldDirectAttachedInstances);

		// Assign new prefab instance map
		PrefabComponent->PrefabInstancesMap = SpawnPrefabInstancesMap;

		// Cache prefab hash
		PrefabComponent->CachedPrefabHash = Prefab->PrefabHash;

		// Restore PrefabComponent mobility
		if (bNeedUpdateComponentMobility)
		{
			PrefabComponent->SetMobility(PrefabComponentMobility);
		}

		// Delete invalid children if not in new prefab instances map
		PrefabComponent->DeleteInvalidAttachedChildren(/*bRecursive*/false);
	}

	// Process nested Prefab Actor
	const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
	const bool bNestedPrefabSupport = PrefabToolSettings->ShouldEnableNestedPrefabSupport();
	if (bNestedPrefabSupport)
	{
		++Depth;
		TArray<APrefabActor*> DirectAttachedPrefabActors;
		PrefabComponent->GetDirectAttachedPrefabActors(DirectAttachedPrefabActors);
		for (APrefabActor* ChildPrefabActor : DirectAttachedPrefabActors)
		{
			if (!ChildPrefabActor->GetPrefab())
			{
				continue;
			}

			if (RevertingPrefabs.Contains(ChildPrefabActor->GetPrefab()))
			{
				PREFABTOOL_LOG(Display, TEXT("[RevertPrefabActor] Circular Nested Prefab Found, abort reverting: %s, @Depth: %d, RevertingPrefabs.Num: %d"), *ChildPrefabActor->GetPrefab()->GetName(), Depth, RevertingPrefabs.Num());
				continue;
			}
			else
			{
				RevertingPrefabs.AddUnique(ChildPrefabActor->GetPrefab());
			}

			if (ChildPrefabActor->IsConnected())
			{
				struct FAttachData
				{
					FAttachData(AActor* InActor, AActor* InParentActor, FName InSocketName, FTransform& InRelativeTransform, FTransform& InWorldTransfrom)
						: Actor(InActor)
						, ParentActor(InParentActor)
						, SocketName(InSocketName)
						, RelativeTransfrom(InRelativeTransform)
						, WorldTransfrom(InWorldTransfrom)
					{}
					AActor* Actor;
					AActor* ParentActor;
					FName SocketName;
					FTransform RelativeTransfrom;
					FTransform WorldTransfrom;
				};

				// Detach
				TArray<FAttachData> AttachDatas;
				FName SocketName = ChildPrefabActor->GetAttachParentSocketName();
				AActor* ParentActor = ChildPrefabActor->GetAttachParentActor();
				FTransform RelativeTransform = ChildPrefabActor->GetRootComponent() ? ChildPrefabActor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
				FTransform WorldTransfrom = ChildPrefabActor->GetRootComponent() ? ChildPrefabActor->GetRootComponent()->GetComponentTransform() : FTransform::Identity;

				ChildPrefabActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				ChildPrefabActor->SetActorTransform(FTransform::Identity);
				AttachDatas.Emplace(ChildPrefabActor, ParentActor, SocketName, RelativeTransform, WorldTransfrom);

				// Revert
				DoRevertPrefabActor(ChildPrefabActor, bRevertingDisconnectedPrefab, bRevertOnlyIfPrefabChanged, Depth);

				// Re-attach
				for (int32 Index = 0; Index < AttachDatas.Num(); ++Index)
				{
					AttachDatas[Index].Actor->SetActorTransform(AttachDatas[Index].WorldTransfrom);

					if (AttachDatas[Index].ParentActor != nullptr)
					{
						AttachDatas[Index].Actor->AttachToActor(AttachDatas[Index].ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachDatas[Index].SocketName);
						AttachDatas[Index].Actor->SetActorRelativeTransform(AttachDatas[Index].RelativeTransfrom);

					}
					AttachDatas[Index].Actor->PostEditMove(true);
				}

				// Merge instances map back to parent prefab actor
				const TMap<FName, AActor*>& ChildPrefabInstancesMap = ChildPrefabActor->GetPrefabComponent()->GetPrefabInstancesMap();
				Local::MergeChildInstancesMap(PrefabComponent->PrefabInstancesMap, ChildPrefabInstancesMap, PrefabTagPrefix);
			}
		}

		if (DirectAttachedPrefabActors.Num() == 0)
		{
			RevertingPrefabs.Empty();
			RevertingPrefabs.Add(RootPrefab);
		}
	}

	if (bShouldRevert)
	{
		// Delete attached but not in prefab
		Local::DeleteNotAttached(PrefabActor, OldAttachedChildren);

		// Validate Prefab Instances
		PrefabComponent->ValidatePrefabInstancesMap(/*bRecursive=*/false);

		// Restore current level of owning world
		OwningWorld->SetCurrentLevel(OldWorldCurrentLevel);
	}

	EndSkipSelectionMonitor();
}

void FPrefabToolEditorUtil::ApplyPrefabActor(APrefabActor* InPrefabActor, FVector InNewPrefabPivot/* = FVector::ZeroVector*/)
{
	struct Local
	{
// 		static void ApplyPrefabToAllReferencedActors(UPrefabAsset* PrefabAssetToFind, APrefabActor* ExcludeActor)//, TArray<APrefabActor*>& ExcludedActors)
// 		{
// 			FScopedSlowTask SlowTask(3, LOCTEXT("ApplyPrefabInWorld", "Apply Prefab Changes to All Connected Prefab Actors"));
// 			SlowTask.MakeDialog();
// 
// 			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
// 
// 			TArray< TWeakObjectPtr<UObject> > OutObjects;
// 			WorldReferenceGenerator ObjRefGenerator;
// 
// 			SlowTask.EnterProgressFrame();
// 			ObjRefGenerator.BuildReferencingData();
// 
// 			SlowTask.EnterProgressFrame();
// 			ObjRefGenerator.MarkAllObjects();
// 			ObjRefGenerator.Generate(PrefabAssetToFind, OutObjects);
// 
// 			SlowTask.EnterProgressFrame();
// 
// 			if (OutObjects.Num() > 0)
// 			{
// 				// Update referencing actors
// 				for (int32 ActorIdx = 0; ActorIdx < OutObjects.Num(); ++ActorIdx)
// 				{
// 					if (APrefabActor* PrefabActor = CastChecked<APrefabActor>(OutObjects[ActorIdx].Get()))
// 					{
// 						if ((ExcludeActor == nullptr) || (ExcludeActor != nullptr && PrefabActor != ExcludeActor))
// 						{
// 							if (PrefabAssetToFind == PrefabActor->GetPrefabComponent()->Prefab) // avoid update nest prefab parent
// 							{
// 								RevertPrefabActor(PrefabActor);
// 							}
// 						}
// 					}
// 				}
// 			}
// 		}

// 		static void ApplyPrefabToAllReferencedActorsInWorld(UWorld* World, UPrefabAsset* PrefabAssetToFind, APrefabActor* ExcludeActor)//, TArray<APrefabActor*>& ExcludedActors)
// 		{
// 			for (FActorIterator It(World); It; ++It)
// 			{
// 				AActor* Actor = *It;
// 				if (Actor && !Actor->IsPendingKillPending())
// 				{
// 					if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
// 					{
// 						if ((ExcludeActor == nullptr) || (ExcludeActor != nullptr && PrefabActor != ExcludeActor))
// 						{
// 							if (PrefabAssetToFind == PrefabActor->GetPrefabComponent()->Prefab) // avoid update nest prefab parent
// 							{
// 								RevertPrefabActor(PrefabActor);
// 							}
// 						}
// 					}
// 				}
// 			}
// 		}

		static bool HasNestedPrefabs(APrefabActor* ParentPrefabActor, const TArray<AActor*>& ChildrenActors)
		{
			bool bFoundPrefabActor = false;
			for (AActor* ChildActor : ChildrenActors)
			{
				if (APrefabActor* ChildPrefabActor = Cast<APrefabActor>(ChildActor))
				{
					bFoundPrefabActor = true;
					break;
				}
			}

			return bFoundPrefabActor;
		}

		static bool HasCircuralNestedPrefab(APrefabActor* ParentPrefabActor, const TArray<AActor*>& ChildrenActors)
		{
			bool bCircuralPrefabFound = false;

			UPrefabAsset* ParentPrefab = ParentPrefabActor->GetPrefabComponent()->GetPrefab();

			TArray<AActor*> SortedChildrenActors(ChildrenActors);
			FPrefabActorUtil::SortActorsHierarchy(SortedChildrenActors, /*bFromParentToChild=*/false);
			for (AActor* ChildActor : SortedChildrenActors)
			{
				if (APrefabActor* ChildPrefabActor = Cast<APrefabActor>(ChildActor))
				{
					UPrefabAsset* ChildPrefab = ChildPrefabActor->GetPrefabComponent()->GetPrefab();

					bCircuralPrefabFound = ChildPrefab == ParentPrefab;
					if (bCircuralPrefabFound)
					{
						break;
					}

					APrefabActor* CurrentChildPrefabActor = ChildPrefabActor;
					while (APrefabActor* FirstParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(CurrentChildPrefabActor))
					{
						if (!SortedChildrenActors.Contains(FirstParentPrefabActor))
						{
							break;
						}

						bCircuralPrefabFound = FirstParentPrefabActor->GetPrefabComponent()->GetPrefab() == ChildPrefab;
						if (bCircuralPrefabFound)
						{
							break;
						}
						else
						{
							CurrentChildPrefabActor = FirstParentPrefabActor;
						}
					}
				}
			}

			return bCircuralPrefabFound;
		}
	};

	BeginSkipSelectionMonitor();

	UWorld* World = InPrefabActor->GetLevel()->OwningWorld;
	UPrefabComponent* PrefabComponent = InPrefabActor->GetPrefabComponent();
	if (PrefabComponent && PrefabComponent->GetConnected())
	{
		if (UPrefabAsset* Prefab = PrefabComponent->GetPrefab())
		{
			const float MessageDuration = 1.8f;
			const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();

			// Gather all potential children instance actors
			TArray<AActor*> PotentialInstanceActors;
			PrefabComponent->GetAllPotentialChildrenInstanceActors(PotentialInstanceActors);

			// Nested prefab validation
			const bool bNestedPrefabSupport = PrefabToolSettings->ShouldEnableNestedPrefabSupport();
			if (!bNestedPrefabSupport && Local::HasNestedPrefabs(InPrefabActor, PotentialInstanceActors))
			{
				NotifyMessage(FText::FromString(TEXT("Nested prefab support is disabled! Aborting!")), MessageDuration);
				return;
			}
			if (Local::HasCircuralNestedPrefab(InPrefabActor, PotentialInstanceActors))
			{
				NotifyMessage(FText::FromString(TEXT("Circular nested prefab detected! Aborting!")), MessageDuration);
				return;
			}

			// Select all potential children instance actors
			GEditor->SelectNone(/*bNoteSelectionChange*/false, true);
			for (AActor* InstanceActor : PotentialInstanceActors)
			{
				if (InstanceActor && !InstanceActor->IsPendingKillPending() )//&& IsActorSupported(InstanceActor))
				{
					GEditor->SelectActor(InstanceActor, /*bInSelected=*/true, /*bNotify=*/ false, /*bSelectEvenIfHidden=*/ true);
				}
			}

			// Deal with possible duplicated instances tags from duplicated actors
			ValidatePrefabInstancesTag(PotentialInstanceActors, InPrefabActor);

			EnsureActorsHavePrefabInstanceTag(PotentialInstanceActors, Prefab->GetPrefabTagPrefix());

			PrefabComponent->ValidatePrefabInstancesMap(/*bRecursive=*/true);

			TMap < AActor*, TArray<FName>> ActorTagsBackup;
			FPrefabTagUtil::BackupActorTags(PotentialInstanceActors, ActorTagsBackup);

			ValidatePrefabInstancesTag(PotentialInstanceActors, InPrefabActor);

			struct FAttachData
			{
				FAttachData(AActor* InActor, AActor* InParentActor, FName InSocketName, FTransform& InRelativeTransform, FTransform& InWorldTransfrom)
					: Actor(InActor)
					, ParentActor(InParentActor)
					, SocketName(InSocketName)
					, RelativeTransfrom(InRelativeTransform)
					, WorldTransfrom(InWorldTransfrom)
				{}
				AActor* Actor;
				AActor* ParentActor;
				FName SocketName;
				FTransform RelativeTransfrom;
				FTransform WorldTransfrom;
			};

			// Detach
			TArray<FAttachData> AttachDatas;
			for (AActor* InstanceActor : PotentialInstanceActors)
			{
				if (InstanceActor && !InstanceActor->IsPendingKillPending() //&& IsActorSupported(InstanceActor)
					&& InstanceActor->GetAttachParentActor() == InPrefabActor)
				{	
					FName SocketName = InstanceActor->GetAttachParentSocketName();
					AActor* ParentActor = InPrefabActor;

					FTransform RelativeTransform = InstanceActor->GetRootComponent() ? InstanceActor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
					FTransform WorldTransfrom = InstanceActor->GetRootComponent() ? InstanceActor->GetRootComponent()->GetComponentTransform() : FTransform::Identity;

					InstanceActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);

					// offset
					InstanceActor->TeleportTo(InstanceActor->GetActorLocation() - InNewPrefabPivot, InstanceActor->GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ true);

					AttachDatas.Emplace(InstanceActor, ParentActor, SocketName, RelativeTransform, WorldTransfrom);
				}
			}

			// Cache current level
			ULevel* OldWorldCurrentLevel = World->GetCurrentLevel();
			World->SetCurrentLevel(InPrefabActor->GetLevel());

			FString CopyData;
			GEditor->edactCopySelected(World, &CopyData);

			// Restore current level
			World->SetCurrentLevel(OldWorldCurrentLevel);

			// Re-attach
			for (int32 Index = 0; Index < AttachDatas.Num(); ++Index)
			{
				if (AttachDatas[Index].ParentActor != nullptr)
				{
					AttachDatas[Index].Actor->AttachToActor(AttachDatas[Index].ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachDatas[Index].SocketName);
				}
				AttachDatas[Index].Actor->PostEditMove(true);
			}

			FPrefabTagUtil::RestoreActorTags(PotentialInstanceActors, ActorTagsBackup);

			Prefab->Modify();

			// Save reference to generated blueprint
			if (PrefabComponent->GeneratedBlueprint != nullptr)
			{
				Prefab->GeneratedBlueprintAssetReference = FSoftObjectPath::GetOrCreateIDForObject(PrefabComponent->GeneratedBlueprint);
			}

			Prefab->SetPrefabContent(FText::FromString(CopyData));

// 			FVector PrefabPivotOffset = PrefabComponent->GetPrefabPivotOffset(/*bWorldSpace*/false);
// 			Prefab->SetPrefabPivot(PrefabPivotOffset);
// 			// Clear Actor Pivot Offset After Apply
// 			{
// 				InPrefabActor->Modify();
// 				InPrefabActor->SetPivotOffset(FVector::ZeroVector);
// 			}

			Prefab->MarkPackageDirty();

			const bool bUpdateAllPrefabActorsWhenApply = PrefabToolSettings->ShouldUpdateAllPrefabActorsWhenApply();
			if (bUpdateAllPrefabActorsWhenApply)
			{
				RevertAllReferencedActorsInWorld(World, Prefab, /*ExcludeActor=*/nullptr);
			}
			else
			{
				RevertPrefabActor(InPrefabActor);
			}
		}
	}

	EndSkipSelectionMonitor();
}

void FPrefabToolEditorUtil::RevertAllPrefabActorsInCurrentLevel(bool bRevertIfPrefabChanged /*=false*/)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	//UE_LOG(LogTemp, Display, TEXT("[RevertAllPrefabActorsInCurrentLevel]:GetMapName: %s"), *World->GetMapName());
	ULevel* CurrentLevel = World->GetCurrentLevel();

	if (CurrentLevel)
	{
		for (FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && !Actor->IsPendingKillPending())
			{
				if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
				{
					const bool bRootPrefabActor = (FPrefabActorUtil::GetFirstAttachedParentPrefabActor(PrefabActor) == nullptr);
					if (bRootPrefabActor)
					{
						RevertPrefabActor(PrefabActor, bRevertIfPrefabChanged);
					}
				}
			}
		}
	}
}

void FPrefabToolEditorUtil::RevertAllReferencedActorsInWorld(UWorld* World, UPrefabAsset* PrefabAssetToFind, APrefabActor* ExcludeActor)
{
	for (FActorIterator It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && !Actor->IsPendingKillPending())
		{
			if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
			{
				if ((ExcludeActor == nullptr) || (ExcludeActor != nullptr && PrefabActor != ExcludeActor))
				{
					if (PrefabAssetToFind == PrefabActor->GetPrefabComponent()->Prefab) // avoid update nest prefab parent
					{
						RevertPrefabActor(PrefabActor);
					}
				}
			}
		}
	}
}

void FPrefabToolEditorUtil::DestroyPrefabActor(class APrefabActor* PrefabActor, bool bDestroyInstanceActors /*= true*/)
{
	if (PrefabActor && !PrefabActor->IsPendingKillPending())
	{
		if (bDestroyInstanceActors)
		{
			DeletePrefabInstances(PrefabActor);
			FPrefabGEditorAdapter::EditorDestroyActor(PrefabActor, true);
		}
		else
		{
			UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
			if (PrefabComponent)
			{
				// Process nested Prefab Actor
				{
					TArray<APrefabActor*> DirectAttachedPrefabActors;
					PrefabComponent->GetDirectAttachedPrefabActors(DirectAttachedPrefabActors);

					for (APrefabActor* ChildPrefabActor : DirectAttachedPrefabActors)
					{
						DestroyPrefabActor(ChildPrefabActor, bDestroyInstanceActors);
					}
				}

				struct Local
				{
					static void DeletePrefabInstanceTagFromActor(const TArray<AActor*>& InActors, const FString& InPrefabTagPrefix)
					{
						for (AActor* Actor : InActors)
						{
							if (Actor && !Actor->IsPendingKillPending())
							{
								for (int32 Index = Actor->Tags.Num() - 1; Index >= 0; --Index)
								{
									if (Actor->Tags[Index].ToString().StartsWith(InPrefabTagPrefix))
									{
										Actor->Modify();
										Actor->Tags.RemoveAt(Index);
									}
								}
							}
						}
					}
				};

				// Clear Prefab Instance Tag
				UPrefabAsset* Prefab = PrefabComponent->GetPrefab();
				if (Prefab)
				{
					FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

					TArray<AActor*> AllAttachedChildren;
					FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, AllAttachedChildren);					
					Local::DeletePrefabInstanceTagFromActor(AllAttachedChildren, PrefabTagPrefix);
				}

				// Backup Children Actors for Re-Attach
				TArray<AActor*> DirectChildActors;
				PrefabActor->GetAttachedActors(DirectChildActors);
				AActor* Parent = PrefabActor->GetAttachParentActor();

				// Destroy Prefab Actor
				FPrefabGEditorAdapter::EditorDestroyActor(PrefabActor, true);

				// Re-Attach Children Actors if Necessary
				if (Parent)
				{
					// Re-Attach
					for (AActor* Child : DirectChildActors)
					{
						if (Child && !Child->IsPendingKillPending())
						{
							Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
							Child->PostEditMove(true);
						}
					}
				}
			}
		}
	}
}

void FPrefabToolEditorUtil::DeletePrefabInstances(class APrefabActor* PrefabActor)
{
	struct Local
	{
		static void DestroyActors(const TArray<AActor*>& ActorsToDestroy)
		{
			TArray<AActor*> SortedActors(ActorsToDestroy);
			FPrefabActorUtil::SortActorsHierarchy(SortedActors, /*bFromParentToChild=*/false);
			for (AActor* Actor : SortedActors)
			{
				if (Actor && !Actor->IsPendingKillPending())
				{
					FPrefabGEditorAdapter::EditorDestroyActor(Actor, true);
				}
			}
		}
	};

	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent)
	{
		UPrefabAsset* Prefab = PrefabComponent->GetPrefab();
		if (Prefab)
		{
			TArray<AActor*> AllAttachedChildren;
			FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, AllAttachedChildren);

			PrefabComponent->GetPrefabInstancesMap().Empty();
			Local::DestroyActors(AllAttachedChildren);
		}
	}
}

void FPrefabToolEditorUtil::DeleteAllTransientPrefabInstancesInCurrentLevel(UWorld* InWorld, uint32 SaveFlags)
{
	const bool bNoSaveFlag = (SaveFlags == SAVE_None);
	if (!bNoSaveFlag)
	{
		return;
	}

	if (InWorld)
	{
		//UE_LOG(LogTemp, Display, TEXT("[RevertAllPrefabActorsInCurrentLevel]:GetMapName: %s"), *World->GetMapName());
		ULevel* CurrentLevel = InWorld->GetCurrentLevel();

		if (CurrentLevel)
		{
			TArray<APrefabActor*> TransientPrefabActors;
			for (FActorIterator It(InWorld); It; ++It)
			{
				AActor* Actor = *It;
				if (Actor && !Actor->IsPendingKillPending())
				{
					if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
					{
						const bool bTransient = PrefabActor->IsTransient();
						if (bTransient)
						{
							TransientPrefabActors.Add(PrefabActor);
						}
					}
				}
			}

			for (int32 Index = TransientPrefabActors.Num() - 1; Index >= 0; --Index)
			{
				TArray<APrefabActor*> AllParentPrefabActors;
				FPrefabActorUtil::GetAllParentPrefabActors(TransientPrefabActors[Index], AllParentPrefabActors);
				bool bParentInTransientSet = false;
				for (APrefabActor* Actor : TransientPrefabActors)
				{
					if ((Actor != TransientPrefabActors[Index]) && (INDEX_NONE != AllParentPrefabActors.Find(Actor)))
					{
						bParentInTransientSet = true;
						break;
					}
				}
				if (bParentInTransientSet)
				{
					TransientPrefabActors.RemoveAt(Index);
				}
			}

			for (APrefabActor* PrefabActor : TransientPrefabActors)
			{
				DeletePrefabInstances(PrefabActor);
			}
		}
	}
}

void FPrefabToolEditorUtil::RestoreAllTransientPrefabInstancesInCurrentLevel(UWorld* InWorld, uint32 SaveFlags)
{
	const bool bNoSaveFlag = (SaveFlags == SAVE_None);
	if (!bNoSaveFlag)
	{
		return;
	}

	if (InWorld)
	{
		//UE_LOG(LogTemp, Display, TEXT("[RevertAllPrefabActorsInCurrentLevel]:GetMapName: %s"), *World->GetMapName());
		ULevel* CurrentLevel = InWorld->GetCurrentLevel();

		if (CurrentLevel)
		{
			TArray<APrefabActor*> TransientPrefabActors;
			for (FActorIterator It(InWorld); It; ++It)
			{
				AActor* Actor = *It;
				if (Actor && !Actor->IsPendingKillPending())
				{
					if (APrefabActor* PrefabActor = Cast<APrefabActor>(Actor))
					{
						const bool bTransient = PrefabActor->IsTransient();
						if (bTransient)
						{
							TransientPrefabActors.Add(PrefabActor);
						}
					}
				}
			}

			for (int32 Index = TransientPrefabActors.Num() - 1; Index >= 0; --Index)
			{
				TArray<APrefabActor*> AllParentPrefabActors;
				FPrefabActorUtil::GetAllParentPrefabActors(TransientPrefabActors[Index], AllParentPrefabActors);
				bool bParentInTransientSet = false;
				for (APrefabActor* Actor : TransientPrefabActors)
				{
					if ((Actor != TransientPrefabActors[Index]) && (INDEX_NONE != AllParentPrefabActors.Find(Actor)))
					{
						bParentInTransientSet = true;
						break;
					}
				}
				if (bParentInTransientSet)
				{
					TransientPrefabActors.RemoveAt(Index);
				}
			}

			for (APrefabActor* PrefabActor : TransientPrefabActors)
			{
				RevertPrefabActor(PrefabActor);
			}
		}
	}
}

void FPrefabToolEditorUtil::ConvertPrefabActorToNormalActor(class APrefabActor* PrefabActor)
{
	struct Local
	{
		static void DeletePrefabInstanceTagFromActor(const TArray<AActor*>& InActors, const FString& InPrefabTagPrefix)
		{
			for (AActor* Actor : InActors)
			{
				if (Actor && !Actor->IsPendingKillPending())
				{
					for (int32 Index = Actor->Tags.Num() - 1; Index >= 0; --Index)
					{
						if (Actor->Tags[Index].ToString().StartsWith(InPrefabTagPrefix))
						{
							Actor->Modify();
							Actor->Tags.RemoveAt(Index);
						}
					}
				}
			}
		}
	};

	const bool bNestedPrefabSupport = true;

	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent)
	{
		// Process nested Prefab Actor
		if (bNestedPrefabSupport)
		{
			TArray<APrefabActor*> DirectAttachedPrefabActors;
			PrefabComponent->GetDirectAttachedPrefabActors(DirectAttachedPrefabActors);

			for (APrefabActor* ChildPrefabActor : DirectAttachedPrefabActors)
			{
				ConvertPrefabActorToNormalActor(ChildPrefabActor);
			}
		}

		UPrefabAsset* Prefab = PrefabComponent->GetPrefab();
		if (Prefab)
		{
			FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

			TArray<AActor*> AllAttachedChildren;
			FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, AllAttachedChildren);
			Local::DeletePrefabInstanceTagFromActor(AllAttachedChildren, PrefabTagPrefix);
		}

		AActor* NewActor = FPrefabActorUtil::SpawnEmptyActor(PrefabActor->GetLevel(), PrefabComponent->GetComponentTransform(), NAME_None, EObjectFlags::RF_Transactional, PrefabComponent->Mobility);

		struct FAttachData
		{
			FAttachData() {}
			AActor* ParentActor;
			FName SocketName;
			FTransform RelativeTransform;
			FTransform WorldTransfrom;
		};

		// Backup AttachData
		FAttachData AttachData;
		AttachData.ParentActor = PrefabActor->GetAttachParentActor();
		AttachData.SocketName = PrefabActor->GetAttachParentSocketName();
		AttachData.RelativeTransform = PrefabActor->GetRootComponent() ? PrefabActor->GetRootComponent()->GetRelativeTransform() : FTransform::Identity;
		AttachData.WorldTransfrom = PrefabActor->GetRootComponent() ? PrefabActor->GetRootComponent()->GetComponentTransform() : FTransform::Identity;

		// Replace
		FPrefabToolEditorUtil::ReplaceActor(PrefabActor, NewActor);

		// Re-Attach
		NewActor->SetActorTransform(AttachData.WorldTransfrom);
		if (AttachData.ParentActor != nullptr)
		{
			NewActor->AttachToActor(AttachData.ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachData.SocketName);
			NewActor->SetActorRelativeTransform(AttachData.RelativeTransform);
		}
		NewActor->PostEditMove(true);
	}
}

void FPrefabToolEditorUtil::ParsePrefabText(const TCHAR*& Buffer, FFeedbackContext* InWarn, FPrefabMetaData& OutPrefabMeta)
{
	struct Local
	{
		static bool GetBEGIN(const TCHAR** Stream, const TCHAR* Match)
		{
			const TCHAR* Original = *Stream;
			if (FParse::Command(Stream, TEXT("BEGIN")) && FParse::Command(Stream, Match))
				return true;
			*Stream = Original;
			return false;
		}

		static bool GetEND(const TCHAR** Stream, const TCHAR* Match)
		{
			const TCHAR* Original = *Stream;
			if (FParse::Command(Stream, TEXT("END")) && FParse::Command(Stream, Match)) return 1; // Gotten.
			*Stream = Original;
			return false;
		}

		static bool GetREMOVE(const TCHAR** Stream, const TCHAR* Match)
		{
			const TCHAR* Original = *Stream;
			if (FParse::Command(Stream, TEXT("REMOVE")) && FParse::Command(Stream, Match))
				return true; // Gotten.
			*Stream = Original;
			return false;
		}

		static void TryAddToAssetReferencesMap(const FString& InObjectPropertyLine, TMap<FString, FSoftObjectPath>& OutAssetReferencesMap)
		{
			// Todo: Deal asset reference in sub object?
			// TheTimeline=(LengthMode=TL_TimelineLength,Length=10.000000,bLooping=True,InterpFloats=((FloatCurve=CurveFloat'/Game/Assets/Blueprints/BP_Butterfly.BP_Butterfly_C:CurveFloat_0_1__71EEED09',TrackName="Flapping Motion",FloatPropertyName="Butterfly_Resting_Flapping_Motion_96D01C4D45C603D99287B499F3A8EA19")),TimelinePostUpdateFunc=BP_Butterfly_2171.Butterfly Resting__UpdateFunc,TimelineFinishedFunc=BP_Butterfly_2171.Butterfly Resting__FinishedFunc,PropertySetObject=BP_Butterfly_C'BP_Butterfly_2171',DirectionPropertyName="Butterfly_Resting__Direction_96D01C4D45C603D99287B499F3A8EA19")

			//UE_LOG(LogTemp, Display, TEXT("[TryAddToAssetReferencesMap]%s"), *InObjectPropertyLine);

 			TArray<FString> PropertyLines;
 			const int32 NumProperties = InObjectPropertyLine.ParseIntoArray(PropertyLines, TEXT(","), /*InCullEmpty=*/true);
			for (TArray<FString>::TConstIterator It(PropertyLines); It; ++It)
			{
				const FString& PropertyLine = *It;
				TArray<FString> Property;
				PropertyLine.ParseIntoArray(Property, TEXT("="), /*InCullEmpty=*/true);
				if (Property.Num() > 0)
				{
					const FString& PropertyValue = Property.Last();
					//UE_LOG(LogTemp, Display, TEXT("\t%s => %s"), *PropertyLine, *Property.Last());
					FString AssetPath;
					if (FPrefabPropertyUtil::GetAssetPath(PropertyValue, &AssetPath))
					{
						FSoftObjectPath AssetRef(AssetPath);
						if (AssetRef.IsValid())
						{
							FString ValidAssetPath = AssetRef.ToString();
							if (nullptr == OutAssetReferencesMap.Find(ValidAssetPath))
							{
								//Warn->Logf(ELogVerbosity::Display, TEXT("          => [FSoftObjectPath]: AssetPath: %s, AssetRef: %s"), *AssetPath, *(AssetRef.ToString()));
								OutAssetReferencesMap.Emplace(ValidAssetPath, AssetRef);
							}
						}
					}
				}
			}
		}

		static const TCHAR* ParseObjectProperties(const TCHAR* SourceText, FFeedbackContext* Warn, int32 Depth, TMap<FString, FSoftObjectPath>& OutAssetReferences)
		{
			if (SourceText == NULL)
				return NULL;

			// Parse all objects stored in the actor.
			FString StrLine;
			int32 LinesConsumed = 0;
			while (FParse::LineExtended(&SourceText, StrLine, LinesConsumed, true))
			{
				// remove extra whitespace and optional semicolon from the end of the line
				{
					int32 Length = StrLine.Len();
					while (Length > 0 &&
						(StrLine[Length - 1] == TCHAR(';') || StrLine[Length - 1] == TCHAR(' ') || StrLine[Length - 1] == 9))
					{
						Length--;
					}
					if (Length != StrLine.Len())
					{
						StrLine = StrLine.Left(Length);
					}
				}

				if (StrLine.Len() == 0)
				{
					continue;
				}

				const TCHAR* Str = *StrLine;

				int32 NewLineNumber;
				if (FParse::Value(Str, TEXT("linenumber="), NewLineNumber))
				{
				}
				else if (GetBEGIN(&Str, TEXT("Brush")))
				{
				}
				else if (GetBEGIN(&Str, TEXT("Foliage")))
				{
				}
				else if (GetBEGIN(&Str, TEXT("Object")))
				{
					// Parse subobject default properties.
					// Note: default properties subobjects have compiled class as their Outer (used for localization).
					FName	TemplateClassName  = NAME_None;
					FParse::Value(Str, TEXT("Class="), TemplateClassName);

					// parse the name of the template
					FName	TemplateName = NAME_None;
					FParse::Value(Str, TEXT("Name="), TemplateName);
					if (TemplateName == NAME_None)
					{
						Warn->Logf(ELogVerbosity::Error, TEXT("BEGIN OBJECT: Must specify valid name for subobject/component: %s"), *StrLine);
						return NULL;
					}
					// import the properties for the subobject
					SourceText = Local::ParseObjectProperties(
						SourceText,
						Warn,
						Depth + 1,
						OutAssetReferences
					);
				}
				else if (FParse::Command(&Str, TEXT("CustomProperties")))
				{
				}
				else if (Local::GetEND(&Str, TEXT("Actor")) || GetEND(&Str, TEXT("DefaultProperties")) || GetEND(&Str, TEXT("structdefaultproperties")) || (GetEND(&Str, TEXT("Object")) && Depth))
				{
					// End of properties.
					break;
				}
				else if (Local::GetREMOVE(&Str, TEXT("Component")))
				{
				}
				else
				{
					// Process object property line
					const TCHAR* StrCopy = Str;
					FPrefabPropertyUtil::SkipAfterEqual(StrCopy);
					if (*StrCopy)
					{
						Local::TryAddToAssetReferencesMap(StrCopy, OutAssetReferences);
					}
				}
			}

			return SourceText;
		}
	};

	OutPrefabMeta.Reset();

	GEditor->IsImportingT3D = false;
	GIsImportingT3D = GEditor->IsImportingT3D;

	TSet<FName> NewGroupsFNames;
	TSet<FName> NewActorsFNames;

	FString StrLine;
	while (FParse::Line(&Buffer, StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (Local::GetBEGIN(&Str, TEXT("MAP")))
		{
		}
		else if (Local::GetEND(&Str, TEXT("MAP")))
		{
		}
		else if (Local::GetBEGIN(&Str, TEXT("LEVEL")))
		{
		}
		else if (Local::GetEND(&Str, TEXT("LEVEL")))
		{
		}
		else if (Local::GetBEGIN(&Str, TEXT("ACTOR")))
		{
			FName ActorClassName;
			if (FParse::Value(Str, TEXT("CLASS="), ActorClassName))
			{
				FName ActorSourceName(NAME_None);
				FParse::Value(Str, TEXT("NAME="), ActorSourceName);

				FName ActorParentName(NAME_None);
				FParse::Value(Str, TEXT("ParentActor="), ActorParentName);

				FName GroupName;
				if (FParse::Value(Str, TEXT("GroupActor="), GroupName))
				{
					NewGroupsFNames.Add(GroupName);
				}

				FString ArchetypeName;
				FParse::Value(Str, TEXT("Archetype="), ArchetypeName);

				if (ActorClassName.ToString().EndsWith(TEXT("_C")))
				{	
					UClass* BlueprintClass = NULL;
					bool bInvalidClass = false;
					ParseObject<UClass>(Str, TEXT("Class="), BlueprintClass, ANY_PACKAGE, &bInvalidClass);
					if (bInvalidClass)
					{
						PREFABTOOL_LOG(Display, TEXT("[ParsePrefabtext] Blueprint Class: %s is invalid!"), *ActorClassName.ToString());
					}
					else
					{
						if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
						{
							FString BlueprintPath = Blueprint->GetPathName();
							FString BluerpintFullPath = FString::Printf(TEXT("Blueprint'%s'"), *BlueprintPath);
							UObject* BlueprintCDO = Blueprint->GeneratedClass->ClassDefaultObject;
							PREFABTOOL_LOG(Display, TEXT("[ParsePrefabtext] Blueprint Found: %s, Path: %s"), *ActorClassName.ToString(), *BluerpintFullPath);
							PREFABTOOL_LOG(Display, TEXT(" => CDO: %s"), *BlueprintCDO->GetPathName());
							Local::TryAddToAssetReferencesMap(BluerpintFullPath, OutPrefabMeta.AssetReferences);
						}
					}
				}

				FPrefabMetaActorRecord ActorRecord;
				ActorRecord.Name = ActorSourceName;
				ActorRecord.Class = ActorClassName;
				ActorRecord.Archetype = ArchetypeName;
				ActorRecord.GroupActor = GroupName;
				OutPrefabMeta.Actors.Add(ActorRecord);

				// Get property text.
				FString PropText, PropertyLine;
				while
					(Local::GetEND(&Buffer, TEXT("ACTOR")) == 0 && FParse::Line(&Buffer, PropertyLine))
				{
					PropText += *PropertyLine;
					PropText += TEXT("\r\n");
				}
				if (!PropText.IsEmpty())
				{
					Local::ParseObjectProperties(*PropText, InWarn, /*Depath=*/0, OutPrefabMeta.AssetReferences);
				}
			}
		}
		else if (Local::GetBEGIN(&Str, TEXT("SURFACE")))
		{
// SURFACE Not Supported yet, need investigation for future support
#if 0
			bool bJustParsedTextureName = false;
			bool bFoundSurfaceEnd = false;
			bool bParsedLineSuccessfully = false;

			do
			{
				if (Local::GetEND(&Buffer, TEXT("SURFACE")))
				{
					bFoundSurfaceEnd = true;
					bParsedLineSuccessfully = true;
				}
				else if (FParse::Command(&Buffer, TEXT("TEXTURE")))
				{
					Buffer++;	// Move past the '=' sign

					FString TextureName;
					bParsedLineSuccessfully = FParse::Line(&Buffer, TextureName, true);
					if (TextureName != TEXT("None"))
					{
						//SrcMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *TextureName, nullptr, LOAD_NoWarn, nullptr));
					}
					bJustParsedTextureName = true;
				}

				// Parse to the next line only if the texture name wasn't just parsed or if the 
				// end of surface isn't parsed. Don't parse to the next line for the texture 
				// name because a FParse::Line() is called when retrieving the texture name. 
				// Doing another FParse::Line() would skip past a necessary surface property.
				if (!bJustParsedTextureName && !bFoundSurfaceEnd)
				{
					FString DummyLine;
					bParsedLineSuccessfully = FParse::Line(&Buffer, DummyLine);
				}

				// Reset this bool so that we can parse lines starting during next iteration.
				bJustParsedTextureName = false;
			} while (!bFoundSurfaceEnd && bParsedLineSuccessfully);
#endif
		}
	}

	GEditor->IsImportingT3D = 0;
	GIsImportingT3D = false;
}

void FPrefabToolEditorUtil::ReplaceSelectedActorsAfterPrefabCreated(UPrefabAsset* PrefabAsset, const FTransform& NewActorTransform, AActor* AttachParent, FName AttachSocketName)
{
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	if (SelectedActors.Num() < 1 || !PrefabAsset)
	{
		return;
	}

	if (UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(APrefabActor::StaticClass()))
	{
		FText ErrorMessage;
		FAssetData PrefabAssetData(PrefabAsset);
		if (ActorFactory->CanCreateActorFrom(PrefabAssetData, ErrorMessage))
		{
			const FScopedTransaction Transaction(LOCTEXT("ReplaceSelectedActorsWithPrefab", "Replace Selected Actors With Prefab"));

			// Destroy selected actors
			for (int32 ActorIdx = 0; ActorIdx < SelectedActors.Num(); ++ActorIdx)
			{
				AActor* OldActor = SelectedActors[ActorIdx];
				GEditor->SelectActor(OldActor, /*bInSelected*/false, /*bNotify=*/false);
				GEditor->Layers->DisassociateActorFromLayers(OldActor);
				OldActor->GetWorld()->EditorDestroyActor(OldActor, true);
			}

			AActor* NewActor = GEditor->UseActorFactory(ActorFactory, PrefabAssetData, &NewActorTransform);
			PREFABTOOL_LOG(Display, TEXT("ReplaceSelectedActorsWithPrefab at Location: %s"), *NewActorTransform.GetLocation().ToString());

			FText ReasonText;
			if (AttachParent && FPrefabGEditorAdapter::GEditor_CanParentActors(AttachParent, NewActor, &ReasonText))
			{
				NewActor->AttachToActor(AttachParent, FAttachmentTransformRules::KeepWorldTransform, AttachSocketName);
			}
		}
	}
}

void FPrefabToolEditorUtil::ValidatePrefabInstancesTag(const TArray<AActor*>& InActors, const APrefabActor* RootPrefabActor) 
{
	struct Local
	{
		static void BuildPrefabActorPrefabPrefixMap(const TArray<APrefabActor*>& PrefabActors, TMap<FString, APrefabActor*>& PrefabPrefixMap)
		{
			for (APrefabActor* PrefabActor : PrefabActors)
			{
				if (UPrefabAsset* PrefabAsset = PrefabActor->GetPrefabComponent()->GetPrefab())
				{
					PrefabPrefixMap.Add(PrefabAsset->GetPrefabTagPrefix(), PrefabActor);
				}
			}
		}
	};

	const bool bUseRootPrefabActor = RootPrefabActor != nullptr;
	FString RootPrefabTagPrefix;
	if (bUseRootPrefabActor && RootPrefabActor->GetPrefabComponent()->GetPrefab())
	{
		RootPrefabTagPrefix = RootPrefabActor->GetPrefabComponent()->GetPrefab()->GetPrefabTagPrefix();
	}

	TArray<FName> UniquePrefabInstanceTags;
	for (AActor* Actor : InActors)
	{
		TArray<APrefabActor*> ValidParentPrefabActors;
		FPrefabActorUtil::GetAllParentPrefabActors(Actor, ValidParentPrefabActors, RootPrefabActor);

		const bool bParentPrefabActorsShouldInGivenActors = !bUseRootPrefabActor;
		if (bParentPrefabActorsShouldInGivenActors)
		{
			for (int32 Index = ValidParentPrefabActors.Num() - 1; Index >= 0; --Index)
			{
				if (INDEX_NONE == InActors.Find(ValidParentPrefabActors[Index]))
				{
					ValidParentPrefabActors.RemoveAt(Index);
				}
			}
		}

		TMap<FString, APrefabActor*> PrefabPrefixMap;
		Local::BuildPrefabActorPrefabPrefixMap(ValidParentPrefabActors, PrefabPrefixMap);

		TArray<FString> ParentPrefabTagPrefixes;
		for (APrefabActor* ParentPrefabActor : ValidParentPrefabActors)
		{
			if (UPrefabAsset* ParentPrefab = ParentPrefabActor->GetPrefabComponent()->GetPrefab())
			{
				ParentPrefabTagPrefixes.AddUnique(ParentPrefab->GetPrefabTagPrefix());
			}
		}

		TArray<FString> UniquePrefabPrefixs;
		for (int32 Index = Actor->Tags.Num() - 1; Index >= 0; --Index)
		{
			FName Tag = Actor->Tags[Index];
			FString TagString = Tag.ToString();
			FString TagPrefabPrefix = FPrefabTagUtil::GetPrefabPrefixFromTag(TagString);

			const bool bPrefabTag = TagString.StartsWith(UPrefabAsset::PREFAG_TAG_PREFIX);

			if (bPrefabTag)
			{
				const bool bDuplicatedTag = FPrefabTagUtil::GetTagCount(Actor->Tags, Tag) > 1; 
				const bool bIsParentPrefabTag = FPrefabTagUtil::GetPrefixCount(TagString, ParentPrefabTagPrefixes) >= 1;
				const bool bUniquePrefabPrefix = (INDEX_NONE == UniquePrefabPrefixs.Find(TagPrefabPrefix));
				
				if (bDuplicatedTag || !bIsParentPrefabTag || !bUniquePrefabPrefix)
				{
					//UE_LOG(LogTemp, Display, TEXT("[ValidatePrefabInstancesTag]%s: Remove: %s, IsParentPrefabTag:%d DuplicatedTag:%d UniquePrefabPrefix:%d"), *Actor->GetActorLabel(), *TagString, bIsParentPrefabTag, bDuplicatedTag, bUniquePrefabPrefix);
					Actor->Modify();
					Actor->Tags.RemoveAt(Index);
				}
				else
				{
					bool bDuplicatedPrefbPrefixInSameActor = FPrefabTagUtil::GetTagPrefixCount(Actor->Tags, TagPrefabPrefix) > 1;
					// More than one instance tags found for same prefab
					if (bDuplicatedPrefbPrefixInSameActor)
					{
						bool bInExistingInstanceMap = false;
						APrefabActor** ParentPrefabActorPtr = PrefabPrefixMap.Find(TagPrefabPrefix);
						if (ParentPrefabActorPtr && *ParentPrefabActorPtr)
						{
							const TMap<FName, AActor*>& ParentPrefabInstancesMap = (*ParentPrefabActorPtr)->GetPrefabComponent()->GetPrefabInstancesMap();
							bInExistingInstanceMap = ParentPrefabInstancesMap.Contains(Tag);
						}

						if (bInExistingInstanceMap)
						{
							// if More than one tag for same prefab, the one already exist in instances map win
							UniquePrefabPrefixs.Add(TagPrefabPrefix);
						}
						else
						{
							Actor->Modify();
							Actor->Tags.RemoveAt(Index);
						}
					}
					else
					{
						// One instance tag for one prefab
						UniquePrefabPrefixs.Add(TagPrefabPrefix);
					}

					if (bUseRootPrefabActor)
					{
						FName InstanceTag = FPrefabTagUtil::GetPrefabInstanceTag(Actor->Tags, RootPrefabTagPrefix);
						if (InstanceTag != NAME_None && Tag == InstanceTag)
						{
							const TMap<FName, AActor*>& RootPrefabInstancesMap = RootPrefabActor->GetPrefabComponent()->GetPrefabInstancesMap();
							bool bInExistingRootInstanceMap = false;
							if (AActor* const* InstanceActorPtr = RootPrefabInstancesMap.Find(InstanceTag))
							{
								bInExistingRootInstanceMap = (Actor == (*InstanceActorPtr));
							}

							if (bInExistingRootInstanceMap)
							{
								UniquePrefabInstanceTags.AddUnique(InstanceTag);
							}
							else
							{
								// not in instances map, and there's more than one in actors, remove it
								if (FPrefabTagUtil::GetTagCountInActors(InActors, InstanceTag) > 1)
								{
									Actor->Modify();
									Actor->Tags.RemoveAt(Index);
								}
							}
						}
					}
				}
			}
		} // End of Actor->Tags loop
	}
}

void FPrefabToolEditorUtil::EnsureActorsHavePrefabInstanceTag(const TArray<AActor*>& InActors, const FString& InPrefabTagPrefix)
{
	for (AActor* Actor : InActors)
	{
		if (Actor && !Actor->IsPendingKillPending())
		{
			FName PrefabInstanceTag = FPrefabTagUtil::GetPrefabInstanceTag(Actor->Tags, InPrefabTagPrefix);

			if (PrefabInstanceTag == NAME_None)
			{
				FString PrefabInstanceId = UPrefabAsset::NewPrefabInstanceId(InPrefabTagPrefix);
				PrefabInstanceTag = FName(*PrefabInstanceId);
				Actor->Tags.Add(PrefabInstanceTag);
			}
		}
	}
}

void FPrefabToolEditorUtil::DeletePrefabInstancesTag(class APrefabActor* PrefabActor)
{
	struct Local
	{
		static void MergeActorArray(const TArray<AActor*>& InActorArray1, const TArray<AActor*>& InActorArray2, TArray<AActor*>& OutMergedActorArray)
		{
			for (AActor* Actor : InActorArray1)
			{
				OutMergedActorArray.AddUnique(Actor);
			}
			for (AActor* Actor : InActorArray2)
			{
				OutMergedActorArray.AddUnique(Actor);
			}
		}
	};

	UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent();
	if (PrefabComponent)
	{
		UPrefabAsset* Prefab = PrefabComponent->GetPrefab();
		if (Prefab)
		{
			FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

			TArray<AActor*> AllAttachedChildren;
			FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, AllAttachedChildren);

			for (AActor* Child : AllAttachedChildren)
			{
				FPrefabTagUtil::DeleteActorTagByPrefix(Child, PrefabTagPrefix);
			}
		}
	}
}

void FPrefabToolEditorUtil::GatherPrefabInstancesMap(UPrefabAsset* Prefab, APrefabActor* RootPrefabActor, const TArray<AActor*>& InPrefabInstances, TMap<FName, AActor*>* OutPrefabInstancesMapPtr, bool bDeleteInvalid /*= true*/)
{
	TArray<AActor*> NotInPrefabInstancesMapActors;

	FString PrefabTagPrefix = Prefab->GetPrefabTagPrefix();

	for (AActor* Actor : InPrefabInstances)
	{
		if (Actor && !Actor->IsPendingKillPending())
		{
			bool bValid = false;

			if (Actor->IsAttachedTo(RootPrefabActor))
			{
				FName PrefabInstanceTag = FPrefabTagUtil::GetPrefabInstanceTag(Actor->Tags, PrefabTagPrefix);

				if (PrefabInstanceTag != NAME_None)
				{
					bValid = true;
					if (OutPrefabInstancesMapPtr)
					{
						(*OutPrefabInstancesMapPtr).Add(PrefabInstanceTag, Actor);
					}
				}
			}
			if (!bValid)
			{
				NotInPrefabInstancesMapActors.AddUnique(Actor);
			}
		}
	}

	// Delete non-attached spawn instances (possible cause by duplicated name in prefab)
	for (AActor* Actor : NotInPrefabInstancesMapActors)
	{
		FPrefabGEditorAdapter::EditorDestroyActor(Actor, false);
	}
}

void FPrefabToolEditorUtil::CenterGroupActors(TArray<AGroupActor*>& InGroupActors)
{
	for (AGroupActor* GroupActor : InGroupActors)
	{
		if (GroupActor)
		{
			GroupActor->CenterGroupLocation();
		}
	}
}

void FPrefabToolEditorUtil::ReplaceActor(AActor* InOldActor, AActor* NewActor)
{
	if (InOldActor == nullptr || NewActor == nullptr)
	{
		return;
	}

	// Cache for attachment info of all actors being converted.
	TArray<ReattachActorsHelper::FActorAttachmentCache> AttachmentInfo;

	TArray<AActor*> ActorsToReplace;
	ActorsToReplace.Add(InOldActor);

	// Cache the current attachment states.
	ReattachActorsHelper::CacheAttachments(ActorsToReplace, AttachmentInfo);

	// Maps actors from old to new for quick look-up.
	TMap<AActor*, AActor*> ConvertedMap;

	TArray<AActor*> OldActorsToDestroy;
	for (int32 ActorIdx = 0; ActorIdx < ActorsToReplace.Num(); ++ActorIdx)
	{
		AActor* OldActor = ActorsToReplace[ActorIdx];
		UWorld* World = OldActor->GetWorld();
		ULevel* Level = OldActor->GetLevel();

		// rename old actor to "*_REPLACED"
		const FName OldActorName = OldActor->GetFName();
		const FName OldActorReplacedNamed = MakeUniqueObjectName(OldActor->GetOuter(), OldActor->GetClass(), *FString::Printf(TEXT("%s_REPLACED"), *OldActorName.ToString()));
		OldActor->Rename(*OldActorReplacedNamed.ToString());

		// For blueprints, try to copy over properties
		if (UObject* ClassGeneratedBy = OldActor->GetClass()->ClassGeneratedBy)
		{
			if (UBlueprint* Blueprint = CastChecked<UBlueprint>(ClassGeneratedBy))
			{
				// Only try to copy properties if this blueprint is based on the actor
				UClass* OldActorClass = OldActor->GetClass();
				if (Blueprint->GeneratedClass->IsChildOf(OldActorClass) && NewActor != NULL)
				{
					NewActor->UnregisterAllComponents();
					UEditorEngine::CopyPropertiesForUnrelatedObjects(OldActor, NewActor);
					NewActor->RegisterAllComponents();
				}
			}
		}
		else
		{
			NewActor->UnregisterAllComponents();
			UEditorEngine::CopyPropertiesForUnrelatedObjects(OldActor, NewActor);
			NewActor->RegisterAllComponents();
		}

		// Rename new actor to old actor's name
		NewActor->Rename(*OldActorName.ToString());

		// The new actor might not have a root component
		USceneComponent* const NewActorRootComponent = NewActor->GetRootComponent();
		if (NewActorRootComponent)
		{
			//if (!GetDefault<ULevelEditorMiscSettings>()->bReplaceRespectsScale || OldActor->GetRootComponent() == NULL)
			if (OldActor->GetRootComponent() == NULL)
			{
				NewActorRootComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
			}
			else
			{
				NewActorRootComponent->SetRelativeScale3D(OldActor->GetRootComponent()->RelativeScale3D);
			}
		}

		NewActor->Layers.Empty();
		GEditor->Layers->AddActorToLayers(NewActor, OldActor->Layers);

		// Preserve the label and tags from the old actor
		NewActor->SetActorLabel(OldActor->GetActorLabel());
		NewActor->Tags = OldActor->Tags;

		//Allow actor derived classes a chance to replace properties.
		NewActor->EditorReplacedActor(OldActor);

		// Caches information for finding the new actor using the pre-converted actor.
		ReattachActorsHelper::CacheActorConvert(OldActor, NewActor, ConvertedMap, AttachmentInfo[ActorIdx]);

		// Find compatible static mesh components and copy instance colors between them.
		UStaticMeshComponent* NewActorStaticMeshComponent = NewActor->FindComponentByClass<UStaticMeshComponent>();
		UStaticMeshComponent* OldActorStaticMeshComponent = OldActor->FindComponentByClass<UStaticMeshComponent>();
		if (NewActorStaticMeshComponent != NULL && OldActorStaticMeshComponent != NULL)
		{
			NewActorStaticMeshComponent->CopyInstanceVertexColorsIfCompatible(OldActorStaticMeshComponent);
		}

		NewActor->InvalidateLightingCache();
		NewActor->PostEditMove(true);
		NewActor->MarkPackageDirty();

		// Replace references in the level script Blueprint with the new Actor
		const bool bDontCreate = true;
		ULevelScriptBlueprint* LSB = NewActor->GetLevel()->GetLevelScriptBlueprint(bDontCreate);
		if (LSB)
		{
			// Only if the level script blueprint exists would there be references.  
			FBlueprintEditorUtils::ReplaceAllActorRefrences(LSB, OldActor, NewActor);
		}

		if (GEditor->GetSelectedActors()->IsSelected(OldActor))
		{
			GEditor->SelectActor(NewActor, true, /*bNotify=*/false);
		}

		FPrefabGEditorAdapter::EditorDestroyActor(OldActor, true);
	}

	if (ConvertedMap.Num() > 0)
	{
		// Reattaches actors based on their previous parent child relationship.
		ReattachActorsHelper::ReattachActors(ConvertedMap, AttachmentInfo);

		// Perform reference replacement on all Actors referenced by World
		UWorld* CurrentEditorWorld = GEditor->GetEditorWorldContext().World();
		FArchiveReplaceObjectRef<AActor> Ar(CurrentEditorWorld, ConvertedMap, /*bNullPrivateRefs*/false, /*bIgnoreOuterRef*/true, /*bIgnoreArchetypeRef*/false);

		// Go through modified objects, marking their packages as dirty and informing them of property changes
		for (const auto& MapItem : Ar.GetReplacedReferences())
		{
			UObject* ModifiedObject = MapItem.Key;

			if (!ModifiedObject->HasAnyFlags(RF_Transient) && ModifiedObject->GetOutermost() != GetTransientPackage() && !ModifiedObject->RootPackageHasAnyFlags(PKG_CompiledIn))
			{
				ModifiedObject->MarkPackageDirty();
			}

			for (UProperty* Property : MapItem.Value)
			{
				FPropertyChangedEvent PropertyEvent(Property);
				ModifiedObject->PostEditChangeProperty(PropertyEvent);
			}
		}
	}
}

void FPrefabToolEditorUtil::NotifyMessage(const FText& Message, float InDuration /*= 1.0f*/)
{
	if (!Message.IsEmpty())
	{
		FNotificationInfo NotificationInfo(Message);
		NotificationInfo.ExpireDuration = InDuration;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	}
}

///////////////////////////////////////////////////////
// FPrefabTagUtil
//

void FPrefabTagUtil::BackupActorTags(const TArray<AActor*>& InActors, TMap<AActor*, TArray<FName>>& OutActorTags)
{
	for (AActor* Actor : InActors)
	{
		OutActorTags.Add(Actor, Actor->Tags);
	}
}

void FPrefabTagUtil::RestoreActorTags(TArray<AActor*>& InActors, const TMap < AActor*, TArray<FName>>& InActorTags)
{
	for (AActor* Actor : InActors)
	{
		if (const TArray<FName>* Tags = InActorTags.Find(Actor))
		{
			Actor->Tags = *Tags;
		}
	}
}

int32 FPrefabTagUtil::GetTagCount(const TArray<FName>& InTags, const FName& InTag)
{
	int32 Count = 0;
	for (const FName& Tag : InTags)
	{
		if (Tag == InTag)
		{
			++Count;
		}
	}
	return Count;
}

int32 FPrefabTagUtil::GetTagCountInActors(const TArray<AActor*>& InActors, const FName& InTag)
{
	int32 Count = 0;
	for (AActor* Actor : InActors)
	{
		for (const FName& Tag : Actor->Tags)
		{
			if (Tag == InTag)
			{
				++Count;
			}
		}
	}
	
	return Count;
}

int32 FPrefabTagUtil::GetPrefixCount(const FString& InTagString, const TArray<FString>& InValidPrefixs)
{
	int32 Count = 0;

	for (const FString& Prefix : InValidPrefixs)
	{
		if (InTagString.StartsWith(Prefix))
		{
			++Count;
		}
	}
	return Count;
}

int32 FPrefabTagUtil::GetTagPrefixCount(const TArray<FName>& InTags, const FString& InPrefix)
{
	int32 Count = 0;
	for (const FName& Tag : InTags)
	{
		if (Tag.ToString().StartsWith(InPrefix))
		{
			++Count;
		}
	}
	return Count;
}

FString FPrefabTagUtil::GetPrefabPrefixFromTag(const FString& TagString)
{
	FString PrefabPrefix, InstancesID;
	if (TagString.StartsWith(UPrefabAsset::PREFAG_TAG_PREFIX) && TagString.Split(TEXT(":"), &PrefabPrefix, &InstancesID, ESearchCase::CaseSensitive))
	{
		PrefabPrefix.Append(TEXT(":"));
	}
	return PrefabPrefix;
}

FName FPrefabTagUtil::GetPrefabInstanceTag(const TArray<FName>& InTags, const FString& InPrefabPrefix)
{
	FName Result = NAME_None;
	for (const FName& Tag : InTags)
	{
		if (Tag.ToString().StartsWith(InPrefabPrefix))
		{
			Result = Tag;
			break;
		}
	}
	return Result;
}

void FPrefabTagUtil::MergeActorInstanceTags(const TArray<FName>& OldActorTags, TArray<FName>& NewActorTags, const FString& PrefabTagPrefixToKeepInNewActors)
{
	for (const FName& Tag : OldActorTags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(UPrefabAsset::PREFAG_TAG_PREFIX))
		{
			if (!TagString.StartsWith(PrefabTagPrefixToKeepInNewActors) && !NewActorTags.Contains(Tag))
			{
				NewActorTags.Add(Tag);
			}
		}
	}
}

void FPrefabTagUtil::DeleteActorTagByPrefix(AActor* Actor, const FString& InTagPrefix)
{
	for (int32 Index = Actor->Tags.Num() - 1; Index >= 0; --Index)
	{
		FString TagString = Actor->Tags[Index].ToString();
		if (TagString.StartsWith(InTagPrefix))
		{
			Actor->Modify();
			Actor->Tags.RemoveAt(Index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FCopyActorPropertyHelper
// Duplicated from EditorUtilities::CopyActorProperties to avoid modifying Archetype of Target Actor's Component
// when copying properties to ChildActorComponent's ComponentTemplate which is happen to be a blueprint actor
// 
struct FCopyActorPropertyHelper
{
	// Searches through the target components array of the target actor for the source component
	// TargetComponents array is passed in populated to avoid repeated refetching and StartIndex 
	// is updated as an optimization based on the assumption that the standard use case is iterating 
	// over two component arrays that will be parallel in order
	template<class AllocatorType = FDefaultAllocator>
	static UActorComponent* FindMatchingComponentInstances(UActorComponent* SourceComponent, AActor* TargetActor, const TArray<UActorComponent*, AllocatorType>& TargetComponents, int32& StartIndex)
	{
		UActorComponent* TargetComponent = StartIndex < TargetComponents.Num() ? TargetComponents[StartIndex] : nullptr;

		// If the source and target components do not match (e.g. context-specific), attempt to find a match in the target's array elsewhere
		const int32 NumTargetComponents = TargetComponents.Num();
		if ((SourceComponent != nullptr)
			&& ((TargetComponent == nullptr)
				|| (SourceComponent->GetFName() != TargetComponent->GetFName())))
		{
			const bool bSourceIsArchetype = SourceComponent->HasAnyFlags(RF_ArchetypeObject);
			// Reset the target component since it doesn't match the source
			TargetComponent = nullptr;

			if (NumTargetComponents > 0)
			{
				// Attempt to locate a match elsewhere in the target's component list
				const int32 StartingIndex = (bSourceIsArchetype ? StartIndex : StartIndex + 1);
				int32 FindTargetComponentIndex = (StartingIndex >= NumTargetComponents) ? 0 : StartingIndex;
				do
				{
					UActorComponent* FindTargetComponent = TargetComponents[FindTargetComponentIndex];

					if (FindTargetComponent->GetClass() == SourceComponent->GetClass())
					{
						// In the case that the SourceComponent is an Archetype there is a better than even chance the name won't match due to the way the SCS
						// is set up, so we're actually going to reverse search the archetype chain
						if (bSourceIsArchetype)
						{
							UActorComponent* CheckComponent = FindTargetComponent;
							while (CheckComponent)
							{
								if (SourceComponent == CheckComponent->GetArchetype())
								{
									TargetComponent = FindTargetComponent;
									StartIndex = FindTargetComponentIndex;
									break;
								}
								CheckComponent = Cast<UActorComponent>(CheckComponent->GetArchetype());
							}
							if (TargetComponent)
							{
								break;
							}
						}
						else
						{
							// If we found a match, update the target component and adjust the target index to the matching position
							if (FindTargetComponent != NULL && SourceComponent->GetFName() == FindTargetComponent->GetFName())
							{
								TargetComponent = FindTargetComponent;
								StartIndex = FindTargetComponentIndex;
								break;
							}
						}
					}

					// Increment the index counter, and loop back to 0 if necessary
					if (++FindTargetComponentIndex >= NumTargetComponents)
					{
						FindTargetComponentIndex = 0;
					}

				} while (FindTargetComponentIndex != StartIndex);
			}

			// If we still haven't found a match and we're targeting a class default object what we're really looking
			// for is an Archetype
#if 0
			if (TargetComponent == nullptr && TargetActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
			{
				if (bSourceIsArchetype)
				{
					UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(SourceComponent->GetOuter());

					// If the target actor's class is a child of our owner and we're both archetypes, then we're actually looking for an overridden version of ourselves
					if (BPGC && TargetActor->GetClass()->IsChildOf(BPGC))
					{
						TargetComponent = Cast<UActorComponent>(TargetActor->GetClass()->FindArchetype(SourceComponent->GetClass(), SourceComponent->GetFName()));

						// If it is us, then we're done, we don't need to find this
						if (TargetComponent == SourceComponent)
						{
							TargetComponent = nullptr;
						}
					}
				}
				else
				{
					TargetComponent = CastChecked<UActorComponent>(SourceComponent->GetArchetype(), ECastCheckedType::NullAllowed);

					// If the returned target component is not from the direct class of the actor we're targeting, we need to insert an inheritable component
					if (TargetComponent && (TargetComponent->GetOuter() != TargetActor->GetClass()))
					{
						// This component doesn't exist in the hierarchy anywhere and we're not going to modify the CDO, so we'll drop it
						if (TargetComponent->HasAnyFlags(RF_ClassDefaultObject))
						{
							TargetComponent = nullptr;
						}
						else
						{
							UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(TargetActor->GetClass());
							UBlueprint* Blueprint = CastChecked<UBlueprint>(BPGC->ClassGeneratedBy);
							UInheritableComponentHandler* InheritableComponentHandler = Blueprint->GetInheritableComponentHandler(true);
							if (InheritableComponentHandler)
							{
								FComponentKey Key;
								FName const SourceComponentName = SourceComponent->GetFName();

								BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
								while (!Key.IsValid() && BPGC)
								{
									USCS_Node* SCSNode = BPGC->SimpleConstructionScript->FindSCSNode(SourceComponentName);
									if (!SCSNode)
									{
										UBlueprint* SuperBlueprint = CastChecked<UBlueprint>(BPGC->ClassGeneratedBy);
										for (UActorComponent* ComponentTemplate : BPGC->ComponentTemplates)
										{
											if (ComponentTemplate->GetFName() == SourceComponentName)
											{
												if (UEdGraph* UCSGraph = FBlueprintEditorUtils::FindUserConstructionScript(SuperBlueprint))
												{
													TArray<UK2Node_AddComponent*> ComponentNodes;
													UCSGraph->GetNodesOfClass<UK2Node_AddComponent>(ComponentNodes);

													for (UK2Node_AddComponent* UCSNode : ComponentNodes)
													{
														if (ComponentTemplate == UCSNode->GetTemplateFromNode())
														{
															Key = FComponentKey(SuperBlueprint, FUCSComponentId(UCSNode));
															break;
														}
													}
												}
												break;
											}
										}
									}
									else
									{
										Key = FComponentKey(SCSNode);
										break;
									}
									BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
								}

								if (ensure(Key.IsValid()))
								{
									check(InheritableComponentHandler->GetOverridenComponentTemplate(Key) == nullptr);
									TargetComponent = InheritableComponentHandler->CreateOverridenComponentTemplate(Key);
								}
								else
								{
									TargetComponent = nullptr;
								}
							}
						}
					}
				}
			}
#endif
		}

		return TargetComponent;
	}

	static int32 CopyActorProperties(AActor* SourceActor, AActor* TargetActor, const EditorUtilities::FCopyOptions& Options, bool bCopyComponentPropertiesOnly = false)
	{
		using namespace EditorUtilities;

		check(SourceActor != nullptr && TargetActor != nullptr);

		const bool bIsPreviewing = (Options.Flags & ECopyOptions::PreviewOnly) != 0;

		int32 CopiedPropertyCount = 0;

		// The actor's classes should be compatible, right?
		UClass* ActorClass = SourceActor->GetClass();
		check(TargetActor->GetClass()->IsChildOf(ActorClass));

		// Get archetype instances for propagation (if requested)
		TArray<AActor*> ArchetypeInstances;
		if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
		{
			TArray<UObject*> ObjectArchetypeInstances;
			TargetActor->GetArchetypeInstances(ObjectArchetypeInstances);

			for (UObject* ObjectArchetype : ObjectArchetypeInstances)
			{
				if (AActor* ActorArchetype = Cast<AActor>(ObjectArchetype))
				{
					ArchetypeInstances.Add(ActorArchetype);
				}
			}
		}

		bool bTransformChanged = false;

		// Copy non-component properties from the old actor to the new actor
		// @todo sequencer: Most of this block of code was borrowed (pasted) from UEditorEngine::ConvertActors().  If we end up being able to share these code bodies, that would be nice!
		TSet<UObject*> ModifiedObjects;
		for (UProperty* Property = ActorClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			const bool bIsTransient = !!(Property->PropertyFlags & CPF_Transient);
			const bool bIsComponentContainer = !!(Property->PropertyFlags & CPF_ContainsInstancedReference);
			const bool bIsComponentProp = !!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference));
			const bool bIsBlueprintReadonly = !!(Options.Flags & ECopyOptions::FilterBlueprintReadOnly) && !!(Property->PropertyFlags & CPF_BlueprintReadOnly);
			const bool bIsIdentical = Property->Identical_InContainer(SourceActor, TargetActor);

			if (!bIsTransient && !bIsIdentical && !bIsComponentContainer && !bIsComponentProp && !bIsBlueprintReadonly)
			{
				const bool bIsSafeToCopy = !(Options.Flags & ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
				if (bIsSafeToCopy)
				{
					if (!Options.CanCopyProperty(*Property, *SourceActor))
					{
						continue;
					}

					if (!bIsPreviewing)
					{
						if (!ModifiedObjects.Contains(TargetActor))
						{
							// Start modifying the target object
							TargetActor->Modify();
							ModifiedObjects.Add(TargetActor);
						}

						if (Options.Flags & ECopyOptions::CallPostEditChangeProperty)
						{
							TargetActor->PreEditChange(Property);
						}

						// Determine which archetype instances match the current property value of the target actor (before it gets changed). We only want to propagate the change to those instances.
						TArray<UObject*> ArchetypeInstancesToChange;
						if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
						{
							for (AActor* ArchetypeInstance : ArchetypeInstances)
							{
								if (ArchetypeInstance != nullptr && Property->Identical_InContainer(ArchetypeInstance, TargetActor))
								{
									ArchetypeInstancesToChange.Add(ArchetypeInstance);
								}
							}
						}

						CopySingleProperty(SourceActor, TargetActor, Property);

						if (Options.Flags & ECopyOptions::CallPostEditChangeProperty)
						{
							FPropertyChangedEvent PropertyChangedEvent(Property);
							TargetActor->PostEditChangeProperty(PropertyChangedEvent);
						}

						if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
						{
							for (int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstancesToChange.Num(); ++InstanceIndex)
							{
								UObject* ArchetypeInstance = ArchetypeInstancesToChange[InstanceIndex];
								if (ArchetypeInstance != nullptr)
								{
									if (!ModifiedObjects.Contains(ArchetypeInstance))
									{
										ArchetypeInstance->Modify();
										ModifiedObjects.Add(ArchetypeInstance);
									}

									CopySingleProperty(TargetActor, ArchetypeInstance, Property);
								}
							}
						}
					}

					++CopiedPropertyCount;
				}
			}
		}

		// Copy component properties from source to target if they match. Note that the component lists may not be 1-1 due to context-specific components (e.g. editor-only sprites, etc.).
		TInlineComponentArray<UActorComponent*> SourceComponents;
		TInlineComponentArray<UActorComponent*> TargetComponents;

		SourceActor->GetComponents(SourceComponents);
		TargetActor->GetComponents(TargetComponents);

		int32 TargetComponentIndex = 0;
		for (UActorComponent* SourceComponent : SourceComponents)
		{
			if (SourceComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript)
			{
				continue;
			}
			UActorComponent* TargetComponent = FindMatchingComponentInstances(SourceComponent, TargetActor, TargetComponents, TargetComponentIndex);

			if (TargetComponent != nullptr)
			{
				UClass* ComponentClass = SourceComponent->GetClass();
				check(ComponentClass == TargetComponent->GetClass());

				// Build a list of matching component archetype instances for propagation (if requested)
				TArray<UActorComponent*> ComponentArchetypeInstances;
				if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
				{
					for (AActor* ArchetypeInstance : ArchetypeInstances)
					{
						if (ArchetypeInstance != nullptr)
						{
							UActorComponent* ComponentArchetypeInstance = FindMatchingComponentInstance(TargetComponent, ArchetypeInstance);
							if (ComponentArchetypeInstance != nullptr)
							{
								ComponentArchetypeInstances.AddUnique(ComponentArchetypeInstance);
							}
						}
					}
				}

				TSet<const UProperty*> SourceUCSModifiedProperties;
				SourceComponent->GetUCSModifiedProperties(SourceUCSModifiedProperties);

				TArray<UActorComponent*> ComponentInstancesToReregister;

				// Copy component properties
				for (UProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
				{
					const bool bIsTransient = !!(Property->PropertyFlags & CPF_Transient);
					const bool bIsIdentical = Property->Identical_InContainer(SourceComponent, TargetComponent);
					const bool bIsComponent = !!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference));
					const bool bIsTransform =
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);

					if (!bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
						&& (!bIsTransform || SourceComponent != SourceActor->GetRootComponent() || (!SourceActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !TargetActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))))
					{
						const bool bIsSafeToCopy = !(Options.Flags & ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
						if (bIsSafeToCopy)
						{
							if (!Options.CanCopyProperty(*Property, *SourceActor))
							{
								continue;
							}

							if (!bIsPreviewing)
							{
								if (!ModifiedObjects.Contains(TargetComponent))
								{
									TargetComponent->SetFlags(RF_Transactional);
									TargetComponent->Modify();
									ModifiedObjects.Add(TargetComponent);
								}

								if (Options.Flags & ECopyOptions::CallPostEditChangeProperty)
								{
									// @todo simulate: Should we be calling this on the component instead?
									TargetActor->PreEditChange(Property);
								}

								// Determine which component archetype instances match the current property value of the target component (before it gets changed). We only want to propagate the change to those instances.
								TArray<UActorComponent*> ComponentArchetypeInstancesToChange;
								if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
								{
									for (UActorComponent* ComponentArchetypeInstance : ComponentArchetypeInstances)
									{
										if (ComponentArchetypeInstance != nullptr && Property->Identical_InContainer(ComponentArchetypeInstance, TargetComponent))
										{
											bool bAdd = true;
											// We also need to double check that either the direct archetype of the target is also identical
											if (ComponentArchetypeInstance->GetArchetype() != TargetComponent)
											{
												UActorComponent* CheckComponent = CastChecked<UActorComponent>(ComponentArchetypeInstance->GetArchetype());
												while (CheckComponent != ComponentArchetypeInstance)
												{
													if (!Property->Identical_InContainer(CheckComponent, TargetComponent))
													{
														bAdd = false;
														break;
													}
													CheckComponent = CastChecked<UActorComponent>(CheckComponent->GetArchetype());
												}
											}

											if (bAdd)
											{
												ComponentArchetypeInstancesToChange.Add(ComponentArchetypeInstance);
											}
										}
									}
								}

								CopySingleProperty(SourceComponent, TargetComponent, Property);

								if (Options.Flags & ECopyOptions::CallPostEditChangeProperty)
								{
									FPropertyChangedEvent PropertyChangedEvent(Property);
									TargetActor->PostEditChangeProperty(PropertyChangedEvent);
								}

								if (Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances)
								{
									for (int32 InstanceIndex = 0; InstanceIndex < ComponentArchetypeInstancesToChange.Num(); ++InstanceIndex)
									{
										UActorComponent* ComponentArchetypeInstance = ComponentArchetypeInstancesToChange[InstanceIndex];
										if (ComponentArchetypeInstance != nullptr)
										{
											if (!ModifiedObjects.Contains(ComponentArchetypeInstance))
											{
												// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
												// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
												if (!ComponentArchetypeInstance->IsCreatedByConstructionScript())
												{
													ComponentArchetypeInstance->SetFlags(RF_Transactional);
													ComponentArchetypeInstance->Modify();
													ModifiedObjects.Add(ComponentArchetypeInstance);
												}

												// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
												AActor* Owner = ComponentArchetypeInstance->GetOwner();
												if (Owner != nullptr && !ModifiedObjects.Contains(Owner))
												{
													Owner->Modify();
													ModifiedObjects.Add(Owner);
												}
											}

											if (ComponentArchetypeInstance->IsRegistered())
											{
												ComponentArchetypeInstance->UnregisterComponent();
												ComponentInstancesToReregister.Add(ComponentArchetypeInstance);
											}

											CopySingleProperty(TargetComponent, ComponentArchetypeInstance, Property);
										}
									}
								}
							}

							++CopiedPropertyCount;

							if (bIsTransform)
							{
								bTransformChanged = true;
							}
						}
					}
				}
				PREFABTOOL_LOG(Display, TEXT("ComponentInstancesToReregister: %d"), ComponentInstancesToReregister.Num());
				for (UActorComponent* ModifiedComponentInstance : ComponentInstancesToReregister)
				{
					ModifiedComponentInstance->RegisterComponent();
				}
			}
		}
#if 0
		if (!bIsPreviewing && CopiedPropertyCount > 0 && TargetActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && TargetActor->GetClass()->HasAllClassFlags(CLASS_CompiledFromBlueprint))
		{
			FBlueprintEditorUtils::PostEditChangeBlueprintActors(CastChecked<UBlueprint>(TargetActor->GetClass()->ClassGeneratedBy));
		}
#endif
		// If one of the changed properties was part of the actor's transformation, then we'll call PostEditMove too.
		if (!bIsPreviewing && bTransformChanged)
		{
			if (Options.Flags & ECopyOptions::CallPostEditMove)
			{
				const bool bFinishedMove = true;
				TargetActor->PostEditMove(bFinishedMove);
			}
		}

		return CopiedPropertyCount;
	}

	// Stores information (such as modified properties) for an instanced object (component or subobject)
	// in the old CDO, to allow them to be reapplied to the new instance under the new CDO
	struct FInstancedObjectRecord
	{
		TArray<uint8> SavedProperties;
		UObject* OldInstance;
	};

	/*
	* Houses base functionality shared between CPFUO archivers (FCPFUOWriter/FCPFUOReader)
	* Used to track whether tagged data is being processed (and whether we should be serializing it).
	*/
	struct FCPFUOArchive
	{
	public:
		FCPFUOArchive(bool bIncludeUntaggedDataIn)
			: bIncludeUntaggedData(bIncludeUntaggedDataIn)
			, TaggedDataScope(0)
		{}

		FCPFUOArchive(const FCPFUOArchive& DataSrc)
			: bIncludeUntaggedData(DataSrc.bIncludeUntaggedData)
			, TaggedDataScope(0)
		{}

	protected:
		FORCEINLINE void OpenTaggedDataScope() { ++TaggedDataScope; }
		FORCEINLINE void CloseTaggedDataScope() { --TaggedDataScope; }

		FORCEINLINE bool IsSerializationEnabled()
		{
			return bIncludeUntaggedData || (TaggedDataScope > 0);
		}

		bool bIncludeUntaggedData;
	private:
		int32 TaggedDataScope;
	};

	/* Serializes and stores property data from a specified 'source' object. Only stores data compatible with a target destination object. */
	struct FCPFUOWriter : public FObjectWriter, public FCPFUOArchive
	{
	public:
		/* Contains the source object's serialized data */
		TArray<uint8> SavedPropertyData;

	public:
		FCPFUOWriter(UObject* SrcObject, UObject* DstObject, const UEngine::FCopyPropertiesForUnrelatedObjectsParams& Params)
			: FObjectWriter(SavedPropertyData)
			// if the two objects don't share a common native base class, then they may have different
			// serialization methods, which is dangerous (the data is not guaranteed to be homogeneous)
			// in that case, we have to stick with tagged properties only
			, FCPFUOArchive(FindNativeSuperClass(SrcObject) == FindNativeSuperClass(DstObject))
			, bSkipCompilerGeneratedDefaults(Params.bSkipCompilerGeneratedDefaults)
		{
			ArIgnoreArchetypeRef = true;
			ArNoDelta = !Params.bDoDelta;
			ArIgnoreClassRef = true;
			ArPortFlags |= Params.bCopyDeprecatedProperties ? PPF_UseDeprecatedProperties : PPF_None;

#if USE_STABLE_LOCALIZATION_KEYS
			if (GIsEditor && !(ArPortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
			{
				SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(DstObject));
			}
#endif // USE_STABLE_LOCALIZATION_KEYS

			SrcObject->Serialize(*this);
		}

		//~ Begin FArchive Interface
		virtual void Serialize(void* Data, int64 Num) override
		{
			if (IsSerializationEnabled())
			{
				FObjectWriter::Serialize(Data, Num);
			}
		}

		virtual void MarkScriptSerializationStart(const UObject* Object) override { OpenTaggedDataScope(); }
		virtual void MarkScriptSerializationEnd(const UObject* Object) override { CloseTaggedDataScope(); }

#if WITH_EDITOR
		virtual bool ShouldSkipProperty(const class UProperty* InProperty) const override
		{
			static FName BlueprintCompilerGeneratedDefaultsName(TEXT("BlueprintCompilerGeneratedDefaults"));
			return bSkipCompilerGeneratedDefaults && InProperty->HasMetaData(BlueprintCompilerGeneratedDefaultsName);
		}
#endif 
		//~ End FArchive Interface

	private:
		static UClass* FindNativeSuperClass(UObject* Object)
		{
			UClass* Class = Object->GetClass();
			for (; Class; Class = Class->GetSuperClass())
			{
				if ((Class->ClassFlags & CLASS_Native) != 0)
				{
					break;
				}
			}
			return Class;
		}

		bool bSkipCompilerGeneratedDefaults;
	};

	/* Responsible for applying the saved property data from a FCPFUOWriter to a specified object */
	struct FCPFUOReader : public FObjectReader, public FCPFUOArchive
	{
	public:
		FCPFUOReader(FCPFUOWriter& DataSrc, UObject* DstObject)
			: FObjectReader(DataSrc.SavedPropertyData)
			, FCPFUOArchive(DataSrc)
		{
			ArIgnoreArchetypeRef = true;
			ArIgnoreClassRef = true;

#if USE_STABLE_LOCALIZATION_KEYS
			if (GIsEditor && !(ArPortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
			{
				SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(DstObject));
			}
#endif // USE_STABLE_LOCALIZATION_KEYS

			DstObject->Serialize(*this);
		}

		//~ Begin FArchive Interface
		virtual void Serialize(void* Data, int64 Num) override
		{
			if (IsSerializationEnabled())
			{
				FObjectReader::Serialize(Data, Num);
			}
		}

		virtual void MarkScriptSerializationStart(const UObject* Object) override { OpenTaggedDataScope(); }
		virtual void MarkScriptSerializationEnd(const UObject* Object) override { CloseTaggedDataScope(); }
		// ~End FArchive Interface
	};

	static void CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams Params)
	{

		check(OldObject && NewObject);

		// Bad idea to write data to an actor while its components are registered
		AActor* NewActor = Cast<AActor>(NewObject);
		if (NewActor != nullptr)
		{
			TInlineComponentArray<UActorComponent*> Components;
			NewActor->GetComponents(Components);

			for (int32 i = 0; i < Components.Num(); i++)
			{
				ensure(!Components[i]->IsRegistered());
			}
		}

		// If the new object is an Actor, save the root component reference, to be restored later
		USceneComponent* SavedRootComponent = nullptr;
		UObjectProperty* RootComponentProperty = nullptr;
		if (NewActor != nullptr && Params.bPreserveRootComponent)
		{
			RootComponentProperty = FindField<UObjectProperty>(NewActor->GetClass(), "RootComponent");
			if (RootComponentProperty != nullptr)
			{
				SavedRootComponent = Cast<USceneComponent>(RootComponentProperty->GetObjectPropertyValue_InContainer(NewActor));
			}
		}

		// Serialize out the modified properties on the old default object
		TIndirectArray<FInstancedObjectRecord> SavedInstances;
		TMap<FString, int32> OldInstanceMap;
		// Save the modified properties of the old CDO
		FCPFUOWriter Writer(OldObject, NewObject, Params);

		{
			// Find all instanced objects of the old CDO, and save off their modified properties to be later applied to the newly instanced objects of the new CDO
			TArray<UObject*> Components;
			OldObject->CollectDefaultSubobjects(Components, true);

			for (UObject* OldInstance : Components)
			{
				FInstancedObjectRecord* pRecord = new(SavedInstances) FInstancedObjectRecord();
				pRecord->OldInstance = OldInstance;
				OldInstanceMap.Add(OldInstance->GetPathName(OldObject), SavedInstances.Num() - 1);
				const uint32 AdditionalPortFlags = Params.bCopyDeprecatedProperties ? PPF_UseDeprecatedProperties : PPF_None;
				FObjectWriter SubObjWriter(OldInstance, pRecord->SavedProperties, true, true, Params.bDoDelta, AdditionalPortFlags);
			}
		}

		// Gather references to old instances or objects that need to be replaced after we serialize in saved data
		TMap<UObject*, UObject*> ReferenceReplacementMap;
		ReferenceReplacementMap.Add(OldObject, NewObject);
		ReferenceReplacementMap.Add(OldObject->GetArchetype(), NewObject->GetArchetype());
		if (Params.bReplaceObjectClassReferences)
		{
			ReferenceReplacementMap.Add(OldObject->GetClass(), NewObject->GetClass());
		}
		ReferenceReplacementMap.Add(OldObject->GetClass()->GetDefaultObject(), NewObject->GetClass()->GetDefaultObject());

		TArray<UObject*> ComponentsOnNewObject;
		{
			TArray<UObject*> EditInlineSubobjectsOfComponents;
			NewObject->CollectDefaultSubobjects(ComponentsOnNewObject, true);

			// populate the ReferenceReplacementMap 
			for (int32 Index = 0; Index < ComponentsOnNewObject.Num(); Index++)
			{
				UObject* NewInstance = ComponentsOnNewObject[Index];
				if (int32* pOldInstanceIndex = OldInstanceMap.Find(NewInstance->GetPathName(NewObject)))
				{
					FInstancedObjectRecord& Record = SavedInstances[*pOldInstanceIndex];
					ReferenceReplacementMap.Add(Record.OldInstance, NewInstance);
					if (Params.bAggressiveDefaultSubobjectReplacement)
					{
						UClass* Class = OldObject->GetClass()->GetSuperClass();
						if (Class)
						{
							UObject *CDOInst = Class->GetDefaultSubobjectByName(NewInstance->GetFName());
							if (CDOInst)
							{
								ReferenceReplacementMap.Add(CDOInst, NewInstance);
#if WITH_EDITOR
								if (Class->ClassGeneratedBy && Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass)
								{
									UObject *CDOInstS = Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass->GetDefaultSubobjectByName(NewInstance->GetFName());
									if (CDOInstS)
									{
										ReferenceReplacementMap.Add(CDOInstS, NewInstance);
									}

								}
#endif // WITH_EDITOR
							}
						}
					}
				}
				else
				{
					bool bContainedInsideNewInstance = false;
					for (UObject* Parent = NewInstance->GetOuter(); Parent != NULL; Parent = Parent->GetOuter())
					{
						if (Parent == NewObject)
						{
							bContainedInsideNewInstance = true;
							break;
						}
					}

					if (!bContainedInsideNewInstance)
					{
						// A bad thing has happened and cannot be reasonably fixed at this point
//						UE_LOG(LogEngine, Log, TEXT("Warning: The CDO '%s' references a component that does not have the CDO in its outer chain!"), *NewObject->GetFullName(), *NewInstance->GetFullName());
					}
				}
			}

			// Serialize in the modified properties from the old CDO to the new CDO
			if (Writer.SavedPropertyData.Num() > 0)
			{
				FCPFUOReader Reader(Writer, NewObject);
			}

			for (int32 Index = 0; Index < ComponentsOnNewObject.Num(); Index++)
			{
				UObject* NewInstance = ComponentsOnNewObject[Index];
				if (int32* pOldInstanceIndex = OldInstanceMap.Find(NewInstance->GetPathName(NewObject)))
				{
					// Restore modified properties into the new instance
					FInstancedObjectRecord& Record = SavedInstances[*pOldInstanceIndex];
					FObjectReader Reader(NewInstance, Record.SavedProperties, true, true);
					FFindInstancedReferenceSubobjectHelper::Duplicate(Record.OldInstance, NewInstance, ReferenceReplacementMap, EditInlineSubobjectsOfComponents);
				}
			}
			ComponentsOnNewObject.Append(EditInlineSubobjectsOfComponents);
		}

		FFindInstancedReferenceSubobjectHelper::Duplicate(OldObject, NewObject, ReferenceReplacementMap, ComponentsOnNewObject);

		// Replace anything with an outer of the old object with NULL, unless it already has a replacement
		ForEachObjectWithOuter(OldObject, [&ReferenceReplacementMap](UObject* ObjectInOuter)
		{
			if (!ReferenceReplacementMap.Contains(ObjectInOuter))
			{
				ReferenceReplacementMap.Add(ObjectInOuter, nullptr);
			}
		});

		if (Params.bClearReferences)
		{
			UPackage* NewPackage = NewObject->GetOutermost();
			// Replace references to old classes and instances on this object with the corresponding new ones
			FArchiveReplaceOrClearExternalReferences<UObject> ReplaceInCDOAr(NewObject, ReferenceReplacementMap, NewPackage);

			// Replace references inside each individual component. This is always required because if something is in ReferenceReplacementMap, the above replace code will skip fixing child properties
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentsOnNewObject.Num(); ++ComponentIndex)
			{
				UObject* NewComponent = ComponentsOnNewObject[ComponentIndex];
				FArchiveReplaceOrClearExternalReferences<UObject> ReplaceInComponentAr(NewComponent, ReferenceReplacementMap, NewPackage);
			}
		}

		// Restore the root component reference
		if (NewActor != nullptr && Params.bPreserveRootComponent)
		{
			if (RootComponentProperty != nullptr)
			{
				RootComponentProperty->SetObjectPropertyValue_InContainer(NewActor, SavedRootComponent);
			}

			NewActor->ResetOwnedComponents();
		}

		bool bDumpProperties = false;// CVarDumpCopyPropertiesForUnrelatedObjects.GetValueOnAnyThread() != 0;
		// Uncomment the next line to debug CPFUO for a specific object:
		// bDumpProperties |= (NewObject->GetName().Find(TEXT("SpinTree")) != INDEX_NONE);
		if (bDumpProperties)
		{
			DumpObject(*FString::Printf(TEXT("CopyPropertiesForUnrelatedObjects: Old (%s)"), *OldObject->GetFullName()), OldObject);
			DumpObject(*FString::Printf(TEXT("CopyPropertiesForUnrelatedObjects: New (%s)"), *NewObject->GetFullName()), NewObject);
		}

		// Now notify any tools that aren't already updated via the FArchiveReplaceObjectRef path
		if (Params.bNotifyObjectReplacement && GEngine != nullptr)
		{
			GEngine->NotifyToolsOfObjectReplacement(ReferenceReplacementMap);
		}
	}
};

///////////////////////////////////////////////////////
// FCreateBlueprintFromPrefabActor
//

UBlueprint* FCreateBlueprintFromPrefabActor::CreateBlueprint(FString Path, APrefabActor* PrefabActor, bool bReplaceInWorld)
{
	if (PrefabActor && !PrefabActor->IsPendingKillPending() && PrefabActor->GetPrefabComponent())
	{
		// Create a blueprint
		FString PackageName = Path;
		FString AssetName = FPackageName::GetLongPackageAssetName(Path);
		FString BasePath = PackageName + TEXT("/") + AssetName;

		// If no AssetName was found, generate a unique asset name.
		if (AssetName.Len() == 0)
		{
			BasePath = PackageName + TEXT("/") + LOCTEXT("BlueprintName_Default", "NewBlueprint").ToString();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);
		}

		UPackage* Package = CreatePackage(nullptr, *PackageName);
		Blueprint = NewBlueprint(PrefabActor, Package, *AssetName);

		// Regenerate skeleton class as components have been added since initial generation
		FKismetEditorUtilities::GenerateBlueprintSkeleton(Blueprint, /*bForceRegeneration=*/ true);
		// KismetEditorUtilities::CompileBlueprint(Blueprint);
		// CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Blueprint);

		// Mark the package dirty
		Package->MarkPackageDirty();

		// Delete the old actors and create a new instance in the map
// 		if (bReplaceInWorld)
// 		{
// 			FVector Location = NewActorTransform.GetLocation();
// 			FRotator Rotator = NewActorTransform.Rotator();
// 
// 			FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, PrefabActor, Location, Rotator);
// 		}

		// Save reference to generated blueprint in prefab asset
		if (UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent())
		{
			if (UPrefabAsset* Prefab = PrefabComponent->GetPrefab())
			{
				Prefab->Modify();

				Prefab->SetGeneratedBlueprintAssetReference(FSoftObjectPath::GetOrCreateIDForObject(Blueprint));

				Prefab->MarkPackageDirty();

				FPrefabToolEditorUtil::RevertPrefabActor(PrefabActor);
			}
		}

		// Open the editor for the new blueprint
		FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);

		return Blueprint;
	}
	return nullptr;
}

UBlueprint* FCreateBlueprintFromPrefabActor::UpdateBlueprint(UBlueprint* ExistingBlueprint, APrefabActor* PrefabActor, bool bReplaceInWorld)
{
	struct Local
	{
		static void RemoveChildNodeRecursively(USimpleConstructionScript* SimpleConstructionScript, USCS_Node* ScsNode)
		{
			TArray<USCS_Node*> ChildNodes = ScsNode->GetChildNodes();
			for (USCS_Node* ChildNode : ChildNodes)
			{
				RemoveChildNodeRecursively(SimpleConstructionScript, ChildNode);
			}
			SimpleConstructionScript->RemoveNode(ScsNode);
		}
	};

	if (ExistingBlueprint && PrefabActor && !PrefabActor->IsPendingKillPending() && PrefabActor->GetPrefabComponent())
	{
		ExistingBlueprint->Modify();

		const TArray<USCS_Node*> RootNodes = ExistingBlueprint->SimpleConstructionScript->GetRootNodes();
		for (int32 i = RootNodes.Num() - 1; i >= 0; --i)
		{
			Local::RemoveChildNodeRecursively(ExistingBlueprint->SimpleConstructionScript, RootNodes[i]);
		}
		//We want to avoid name reservation so we compile the blueprint after removing all node
		FKismetEditorUtilities::CompileBlueprint(ExistingBlueprint);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		TMap<USceneComponent*, USCS_Node*> NodeMap;
		AddChildNodes(PrefabActor, ExistingBlueprint->SimpleConstructionScript, NodeMap);
		
		FBlueprintEditorUtils::MarkBlueprintAsModified(ExistingBlueprint);
		FKismetEditorUtilities::CompileBlueprint(ExistingBlueprint);

		// Mark the package dirty

		if (UPackage* Package = Cast<UPackage>(ExistingBlueprint->GetOutermost()))
		{
			Package->MarkPackageDirty();
		}

		// Delete the old actors and create a new instance in the map
// 		if (bReplaceInWorld)
// 		{
// 			FVector Location = NewActorTransform.GetLocation();
// 			FRotator Rotator = NewActorTransform.Rotator();
// 
// 			FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, PrefabActor, Location, Rotator);
// 		}

		// Open the editor for the new blueprint
		//FAssetEditorManager::Get().OpenEditorForAsset(ExistingBlueprint);

		return ExistingBlueprint;
	}
	return nullptr;
}

UBlueprint* FCreateBlueprintFromPrefabActor::NewBlueprint(APrefabActor* PrefabActor, UPackage* Package, FName BlueprintName)
{
	UBlueprint* NewBlueprint = nullptr;

	if (PrefabActor && !PrefabActor->IsPendingKillPending() && PrefabActor->GetPrefabComponent())
	{
		UPrefabComponent* PrefabComponent = PrefabActor->GetPrefabComponent(); 
		
		NewBlueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, BlueprintName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("GeneratedBlueprintFromPrefabActor"));

		check(NewBlueprint->SimpleConstructionScript != NULL);
		USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;

		TMap<USceneComponent*, USCS_Node*> NodeMap;
		AddChildNodes(PrefabActor, SCS, NodeMap);

		return NewBlueprint;
	}
	return nullptr;
}

void FCreateBlueprintFromPrefabActor::AddChildNodes(APrefabActor* PrefabActor, USimpleConstructionScript* SCS, TMap<USceneComponent*, USCS_Node*>& NodeMap)
{
	struct Local
	{
		//@Reference: FKismetEditorUtilities::AddComponentsToBlueprint
		static USCS_Node* AddComponentsToBlueprint(USimpleConstructionScript* SCS, const TArray<UActorComponent*>& Components, bool bHarvesting, USCS_Node* OptionalNewRootNode, bool bKeepMobility, bool bReplaceRootNode = true)
		{
			USCS_Node* OutputNewRootNode = nullptr;

			TArray<UBlueprint*> ParentBPStack;
			UBlueprint::GetBlueprintHierarchyFromClass(SCS->GetBlueprint()->GeneratedClass, ParentBPStack);

			TMap<USceneComponent*, USCS_Node*> SceneComponentsToAdd;
			TMap<USceneComponent*, USCS_Node*> InstanceComponentToNodeMap;

			auto AddChildToSCSRootNodeLambda = [SCS, OptionalNewRootNode](USCS_Node* InSCSNode)
			{
				if (OptionalNewRootNode != nullptr)
				{
					OptionalNewRootNode->AddChildNode(InSCSNode);
				}
				else
				{
					// Continuation of convention from FCreateConstructionScriptFromSelectedActors::Execute, perhaps more elegant
					// to provide OptionalNewRootNode in both cases.
					SCS->GetRootNodes()[0]->AddChildNode(InSCSNode);
				}
			};

			struct FAddComponentsToBlueprintImpl
			{
				/**
				* Creates a new USCS_Node in the TargetSCS, duplicating the specified
				* component (leaving the new node unattached). If a copy was already
				* made (found  in NewSceneComponents) then that will be returned instead.
				*/
				static USCS_Node* MakeComponentCopy(UActorComponent* ActorComponent, USimpleConstructionScript* TargetSCS, TMap<USceneComponent*, USCS_Node*>& NewSceneComponents, bool bInternalKeepMobility)
				{
					USceneComponent* AsSceneComponent = Cast<USceneComponent>(ActorComponent);
					if (AsSceneComponent != nullptr)
					{
						USCS_Node** ExistingCopy = NewSceneComponents.Find(AsSceneComponent);
						if (ExistingCopy != nullptr)
						{
							return *ExistingCopy;
						}
					}

					USCS_Node* NewSCSNode = TargetSCS->CreateNode(ActorComponent->GetClass(), ActorComponent->GetFName());
					UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
					Params.bDoDelta = false; // We need a deep copy of parameters here so the CDO values get copied as well
					UEditorEngine::CopyPropertiesForUnrelatedObjects(ActorComponent, NewSCSNode->ComponentTemplate, Params);

					// Clear the instance component flag
					NewSCSNode->ComponentTemplate->CreationMethod = EComponentCreationMethod::Native;

					if (AsSceneComponent != nullptr)
					{
						NewSceneComponents.Add(AsSceneComponent, NewSCSNode);
						if (!bInternalKeepMobility)
						{
							Cast<USceneComponent>(NewSCSNode->ComponentTemplate)->SetMobility(EComponentMobility::Movable);
						}
					}
					return NewSCSNode;
				}
			};

			AActor* Actor = nullptr;

			for (UActorComponent* ActorComponent : Components)
			{
				if (ActorComponent)
				{
					if (Actor)
					{
						check(Actor == ActorComponent->GetOwner());
					}
					else
					{
						Actor = ActorComponent->GetOwner();
						check(Actor);
					}

					if (!ActorComponent->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
					{
						continue;
					}

					USCS_Node* SCSNode = FAddComponentsToBlueprintImpl::MakeComponentCopy(ActorComponent, SCS, InstanceComponentToNodeMap, bKeepMobility);

					USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent);
					// The easy part is non-scene component or the Root simply add it
					if (SceneComponent == nullptr)
					{
						SCS->AddNode(SCSNode);
					}
					else
					{
						if (ActorComponent == Actor->GetRootComponent())
						{
							if (OptionalNewRootNode != nullptr)
							{
								if (bReplaceRootNode)
								{
									InstanceComponentToNodeMap.Add(Cast<USceneComponent>(ActorComponent), OptionalNewRootNode);
								}
								else
								{
									OptionalNewRootNode->AddChildNode(SCSNode);
									InstanceComponentToNodeMap.Add(Cast<USceneComponent>(ActorComponent), SCSNode);
								}
							}
							else
							{
								if (SceneComponent->GetAttachParent() == nullptr)
								{
									AddChildToSCSRootNodeLambda(SCSNode);
								}
								InstanceComponentToNodeMap.Add(Cast<USceneComponent>(ActorComponent), SCSNode);
								OutputNewRootNode = SCSNode;
							}
						}
						// If we're not attached to a blueprint component, add ourself to the root node or the SCS root component:
						else if (SceneComponent->GetAttachParent() == nullptr)
						{
							AddChildToSCSRootNodeLambda(SCSNode);
						}
#if 0
						// If we're attached to a blueprint component look it up as the variable name is the component name
						else if (SceneComponent->GetAttachParent()->IsCreatedByConstructionScript())
						{
							USCS_Node* ParentSCSNode = nullptr;
							if (USCS_Node** ParentSCSNodePtr = InstanceComponentToNodeMap.Find(SceneComponent->GetAttachParent()))
							{
								ParentSCSNode = *ParentSCSNodePtr;
							}
							else if (Components.Contains(SceneComponent->GetAttachParent()))
							{
								// since you cannot rely on the order of the supplied  
								// Components array, we might be looking for a parent 
								// that hasn't been added yet
								ParentSCSNode = FAddComponentsToBlueprintImpl::MakeComponentCopy(SceneComponent->GetAttachParent(), SCS, InstanceComponentToNodeMap, bKeepMobility);
							}
							else
							{
								for (UBlueprint* ParentBlueprint : ParentBPStack)
								{
									ParentSCSNode = ParentBlueprint->SimpleConstructionScript->FindSCSNode(SceneComponent->GetAttachParent()->GetFName());
									if (ParentSCSNode)
									{
										break;
									}
								}
							}
							check(ParentSCSNode);

							if (ParentSCSNode->GetSCS() != SCS)
							{
								SCS->AddNode(SCSNode);
								SCSNode->SetParent(ParentSCSNode);
							}
							else
							{
								ParentSCSNode->AddChildNode(SCSNode);
							}
						}
						else if ((SceneComponent->GetAttachParent()->CreationMethod == EComponentCreationMethod::Native) && !bHarvesting)
						{
							// If we're attached to a component that will be native in the new blueprint
							SCS->AddNode(SCSNode);
							SCSNode->SetParent(SceneComponent->GetAttachParent());
						}
#endif
						else
						{
							// Otherwise check if we've already created the parents' new SCS node and attach to that or cache it off to do next pass
							USCS_Node** ParentSCSNode = InstanceComponentToNodeMap.Find(SceneComponent->GetAttachParent());
							if (ParentSCSNode)
							{
								(*ParentSCSNode)->AddChildNode(SCSNode);
							}
							else
							{
								SceneComponentsToAdd.Add(SceneComponent, SCSNode);
							}
						}
					}
				}
			}

			// Hook up the remaining components nodes that the parent's node was missing when it was processed
			for (auto ComponentIt = SceneComponentsToAdd.CreateConstIterator(); ComponentIt; ++ComponentIt)
			{
				// The AttachParent may or may not be BP-spawnable; if it's not, then we won't have created the parent node, so just add it as a child of the root node in that case.
				if (USCS_Node** ParentSCSNode = InstanceComponentToNodeMap.Find(ComponentIt.Key()->GetAttachParent()))
				{
					(*ParentSCSNode)->AddChildNode(ComponentIt.Value());
				}
				else
				{
					AddChildToSCSRootNodeLambda(ComponentIt.Value());
				}
			}

			return OutputNewRootNode;
		}

		static void CreateNodeFroActor(USimpleConstructionScript* InputSCS, const AActor* InputActor, TMap<USceneComponent*, USCS_Node*>& OutputNodeMap)
		{
			struct FResetSceneComponentAfterCopy
			{
				static void Reset(USceneComponent* Component)
				{
					Component->RelativeLocation = FVector::ZeroVector;
					Component->RelativeRotation = FRotator::ZeroRotator;

					// Clear out the attachment info after having copied the properties from the source actor
					Component->SetupAttachment(nullptr);
					//FDirectAttachChildrenAccessor::Get(Component).Empty();

					// Ensure the light mass information is cleaned up
					Component->InvalidateLightingCache();
				}
			};

			const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
			const bool bBakeBlueprintActor = PrefabToolSettings->bBakeBlueprintWhenGeneratingBlueprint;
			const bool bTryHavestComponents = PrefabToolSettings->bHarvestComponentsWhenGeneratingBlueprint;
			const TArray<TSubclassOf<AActor>>& IgnoreActorClasses = PrefabToolSettings->IgnoreActorClassesWhenGeneratingBlueprint;
			const TArray<FName>& IgnoreActorTags = PrefabToolSettings->IgnoreActorTagsWhenGeneratingBlueprint;

			TArray<AActor*> DirectChildActors;
			InputActor->GetAttachedActors(DirectChildActors);

			for (AActor* ChildActor : DirectChildActors)
			{
				USceneComponent* ChildActorRootComponent = ChildActor->GetRootComponent();
				USceneComponent* ChildActorParentComponent = ChildActor->GetAttachParentActor()->GetRootComponent();
				const FName ChildActorName = ChildActor->GetFName();

				USCS_Node* ChildNode = nullptr;

				bool bIsPrefabActor = ChildActor->IsA(APrefabActor::StaticClass());
				bool bIsBlueprintActor = ChildActor->GetClass()->ClassGeneratedBy != nullptr;
				bool bEmptyOrComponentWrapperActor = ChildActor->GetClass() == AActor::StaticClass() || ChildActor->GetClass()->HasMetaData(FBlueprintMetadata::MD_IgnoreCategoryKeywordsInSubclasses);
				bool bIgnoreByActorClass = IgnoreActorClasses.Contains(ChildActor->GetClass());
				bool bIgnoreByActorTag = ChildActor->Tags.ContainsByPredicate([=](const FName& Tag) {
					return IgnoreActorTags.Contains(Tag);
				});
				bool bHasAttachedChildern = ChildActor->GetRootComponent() && ChildActor->GetRootComponent()->GetAttachChildren().Num() > 0;
				bool bIgnoreButHasAttachedChildren = (bIgnoreByActorClass || bIgnoreByActorTag) && bHasAttachedChildern;

				TArray<USceneComponent*> BakedBlueprintRootComponents;

				// Create new SceneComponent for PrefabActor
				if (bIsPrefabActor || bIgnoreButHasAttachedChildren)
				{
					ChildNode = InputSCS->CreateNode(USceneComponent::StaticClass(), ChildActorName);
				}
				else if (bIgnoreByActorClass || bIgnoreByActorTag)
				{
					continue;
				}
				// Harvest components for EmptyActor or ComponentWrapperClass (use IgnoreCategoryKeywordsInSubclasses as there's currently no access to ComponentWrapperClass metadata)
				else if (bTryHavestComponents && !bIsBlueprintActor && bEmptyOrComponentWrapperActor)
				{
					TArray<UActorComponent*> Components;
					ChildActor->GetComponents(Components);

					// Exclude any components created by other components
					for (int32 Index = Components.Num() - 1; Index >= 0; --Index)
					{
						UActorComponent* ComponentToConsider = Components[Index];
						if (ComponentToConsider->IsEditorOnly())
						{
							Components.RemoveAt(Index, 1, /*bAllowShrinking=*/ false);
						}
					}

					ChildNode = Local::AddComponentsToBlueprint(InputSCS, Components, /*bHarvesting*/true, nullptr, /*bKeepMobility*/ true);
				}
				else if (bIsBlueprintActor && bBakeBlueprintActor)
				{
					ChildNode = InputSCS->CreateNode(USceneComponent::StaticClass(), ChildActorName);

					USceneComponent* BakedBlueprintRootComponent = Cast<USceneComponent>(ChildNode->ComponentTemplate);
					BakedBlueprintRootComponents.Add(BakedBlueprintRootComponent); 

					BakedBlueprintRootComponent->SetMobility(EComponentMobility::Static);

					TArray<UActorComponent*> Components;
					ChildActor->GetComponents(Components);

					// Exclude any components created by other components
					for (int32 Index = Components.Num() - 1; Index >= 0; --Index)
					{
						UActorComponent* ComponentToConsider = Components[Index];
						if (ComponentToConsider->IsEditorOnly())
						{
							Components.RemoveAt(Index, 1, /*bAllowShrinking=*/ false);
							continue;
						}
					}

					Local::AddComponentsToBlueprint(InputSCS, Components, /*bHarvesting*/true, ChildNode, /*bKeepMobility*/ true, /*bReplaceRootNode*/ false);
				}

				// Fallback to ChildActorComponent for all other actors
				if (ChildNode == nullptr)
				{
					ChildNode = InputSCS->CreateNode(UChildActorComponent::StaticClass(), ChildActorName);

					FComponentAssetBrokerage::AssignAssetToComponent(ChildNode->ComponentTemplate, ChildActor->GetClass());

					// Setup Child Actor Template
					if (AActor* ChildActorTemplate = Cast<UChildActorComponent>(ChildNode->ComponentTemplate)->GetChildActorTemplate())
					{
						if (!bIsBlueprintActor)
						{
							// Copy over Actor properties without instanced subobjects
							const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties);
							FCopyActorPropertyHelper::CopyActorProperties(ChildActor, ChildActorTemplate, CopyOptions);

							const bool bHasInstanceComponents = ChildActor->GetInstanceComponents().Num() > 0;
							if (bHasInstanceComponents)
							{
								Local::AddComponentsToBlueprint(InputSCS, ChildActor->GetInstanceComponents(), /*bHarvesting*/true, ChildNode, /*bKeepMobility*/ true);
							}
						}
						else
						{
							// Copy over all Actor properties include components
							UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
							Params.bDoDelta = false; // We need a deep copy of parameters here so the CDO values get copied as well
							FCopyActorPropertyHelper::CopyPropertiesForUnrelatedObjects(ChildActor, ChildActorTemplate, Params);
						}

						if (USceneComponent* DstSceneRoot = ChildActorTemplate->GetRootComponent())
						{
							FResetSceneComponentAfterCopy::Reset(DstSceneRoot);
						}

					} // if (AActor* ChildActorTemplate = Cast<UChildActorComponent>(ChildNode->ComponentTemplate)->GetChildActorTemplate())

				} // if (ChildNode == nullptr)

				// Set Mobility and Hidden in game
				if (USceneComponent* ChildComponent = Cast<USceneComponent>(ChildNode->ComponentTemplate))
				{
					bool bAlwaysStatic = BakedBlueprintRootComponents.Find(ChildComponent) != INDEX_NONE;
					if (bAlwaysStatic)
					{
						ChildComponent->SetMobility(EComponentMobility::Static);
					}
					else
					{
						ChildComponent->SetMobility(ChildActorRootComponent->Mobility);
					}
					ChildComponent->SetHiddenInGame(ChildActor->bHidden);
				}

				// Attach To Parent Node
				if (USCS_Node** ParentNodePtr = OutputNodeMap.Find(ChildActorParentComponent))
				{
					USCS_Node* ParentNode = *ParentNodePtr;
					ParentNode->AddChildNode(ChildNode);
				}
				else
				{
					InputSCS->GetRootNodes()[0]->AddChildNode(ChildNode);
				}

				// Setup Relative Transform
				if (USceneComponent* ChildComponent = Cast<USceneComponent>(ChildNode->ComponentTemplate))
				{
					bool bIsBakedBlueprintRootComponent = BakedBlueprintRootComponents.Find(ChildComponent) != INDEX_NONE;
					if (!bIsBakedBlueprintRootComponent)
					{
						const FTransform ChildRelativeTransform = ChildActorRootComponent->GetComponentToWorld().GetRelativeTransform(ChildActorParentComponent->GetComponentToWorld());
						ChildComponent->RelativeLocation = ChildRelativeTransform.GetLocation();
						ChildComponent->RelativeRotation = ChildRelativeTransform.GetRotation().Rotator();
						ChildComponent->RelativeScale3D = ChildRelativeTransform.GetScale3D();
					}
				}

				OutputNodeMap.Add(ChildActorRootComponent, ChildNode);

				CreateNodeFroActor(InputSCS, ChildActor, OutputNodeMap);
			}
		}

		static void SetAllComponentsMobility(USimpleConstructionScript* InputSCS, EComponentMobility::Type InputNewMobility)
		{
			const TArray<USCS_Node*>& AllNodes = InputSCS->GetAllNodes();
			for (USCS_Node* Node : AllNodes)
			{
				USceneComponent* AsSceneComponent = Node->ComponentTemplate != NULL ? Cast<USceneComponent>(Node->ComponentTemplate) : NULL;
				if (AsSceneComponent)
				{
					AsSceneComponent->SetMobility(InputNewMobility);
					if (UChildActorComponent* AsChildActorComponent = Cast<UChildActorComponent>(AsSceneComponent))
					{
						if (AActor* ChildActor = AsChildActorComponent->GetChildActorTemplate())
						{
							if (USceneComponent* ChildActorRootComponent = ChildActor->GetRootComponent())
							{
								ChildActorRootComponent->SetMobility(InputNewMobility);
							}
						}
					}
				}
			}
		}
	};

	// Create a common root
	USCS_Node* RootNode = SCS->GetDefaultSceneRootNode();;
	if (!RootNode)
	{
		RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
		SCS->AddNode(RootNode);
	}

	USceneComponent* RootComponent = Cast<USceneComponent>(RootNode->ComponentTemplate);
	check(RootComponent)

	// Set Mobility of RootComponent to Static for attaching
	RootComponent->SetMobility(EComponentMobility::Static);

	NodeMap.Add(PrefabActor->GetRootComponent(), RootNode);

	Local::CreateNodeFroActor(SCS, PrefabActor, NodeMap);

	// Restore Mobility of RootComponent
	const bool bForceMobilityToMovable = GetDefault<UPrefabToolSettings>()->bForceMobilityToMovableWhenGeneratingBlueprint;
	if (bForceMobilityToMovable)
	{
		Local::SetAllComponentsMobility(SCS, EComponentMobility::Movable);
	}
	else
	{
		RootComponent->SetMobility(PrefabActor->GetRootComponent()->Mobility);
	}
}


/////////////////////////////////////////////////////////////////
// FPrefabParser
//

bool FPrefabParser::ParseExportTextPath(const FString& InExportTextPath, FString* OutClassName, FString* OutObjectPath)
{
	if (FPackageName::ParseExportTextPath(InExportTextPath, OutClassName, OutObjectPath))
	{
		FString& OutObjectPathRef = *OutObjectPath;
		if (OutObjectPathRef.Len() > 1 && OutObjectPathRef.StartsWith(TEXT("\"")) && OutObjectPathRef.EndsWith(TEXT("\"")))
		{
			OutObjectPathRef = OutObjectPathRef.LeftChop(1).RightChop(1);
		}
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

#endif