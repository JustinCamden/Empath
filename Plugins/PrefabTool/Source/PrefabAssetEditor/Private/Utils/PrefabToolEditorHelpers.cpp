// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "PrefabToolEditorHelpers.h"
#include "PrefabToolHelpers.h"

#include "BlueprintEditorUtils.h"
#include "Engine/InheritableComponentHandler.h"
#include "K2Node_AddComponent.h"
#include "BusyCursor.h"
#include "Layers/ILayers.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "LevelUtils.h"
#include "ScopedTransaction.h"
#include "BSPOps.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "ActorEditorUtils.h"
#include "Logging/MessageLog.h"
#include "Engine/SimpleConstructionScript.h"

DEFINE_LOG_CATEGORY(LogPrefabTool);

#define LOCTEXT_NAMESPACE "PrefabToolEditorHelpers"

namespace PrefabToolEditorHelpers
{
	void AddActorComponentsToMap(const AActor* KeyActor, const AActor* ValueActor, TMap<UObject*, UObject*>& InMap)
	{
		TArray<UActorComponent*> KeyComponents;
		KeyActor->GetComponents(KeyComponents);

		TArray<UActorComponent*> ValueComponents;;
		ValueActor->GetComponents(ValueComponents);

		int32 MatchedCount = FMath::Min(KeyComponents.Num(), ValueComponents.Num());
		for (int32 Index = 0; Index < MatchedCount; ++Index)
		{
			InMap.Add(KeyComponents[Index], ValueComponents[Index]);
		}
	}
	
	namespace EAdvancedCopyOptions
	{
		enum Type
		{
			Default = 0,
			CanCopyTransientProperties = 1 << 0,
			CanCopyComponentContainer = 1 << 1,
			CanCopyComponent = 1 << 2,
		};
	}

	struct FAdvancedCopyOptions
	{
		FAdvancedCopyOptions(const EAdvancedCopyOptions::Type InFlags) : Flags(InFlags) {}
		EAdvancedCopyOptions::Type Flags;
	};

	namespace CopyActorPropertiesLocal
	{
		template<class AllocatorType = FDefaultAllocator>
		UActorComponent* FindMatchingComponentInstance(UActorComponent* SourceComponent, AActor* TargetActor, const TArray<UActorComponent*, AllocatorType>& TargetComponents, int32& StartIndex)
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
			}

			return TargetComponent;
		}
	}

	// @EditorUtilities
	int32 CopyActorProperties(AActor* SourceActor, AActor* TargetActor, const EditorUtilities::FCopyOptions& Options, const FAdvancedCopyOptions& AdvancedOptions, TMap<UObject*, UObject*>* OutMatchingComponentMap)
	{
		
		check(SourceActor != nullptr && TargetActor != nullptr);

		const bool bIsPreviewing = (Options.Flags & EditorUtilities::ECopyOptions::PreviewOnly) != 0;

		int32 CopiedPropertyCount = 0;

		// The actor's classes should be compatible, right?
		UClass* ActorClass = SourceActor->GetClass();
		check(TargetActor->GetClass()->IsChildOf(ActorClass));

		// Get archetype instances for propagation (if requested)
		TArray<AActor*> ArchetypeInstances;
		if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
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

		const bool bCanCopyTransient = !!(AdvancedOptions.Flags & EAdvancedCopyOptions::CanCopyTransientProperties);
		const bool bCanCopyComponetContainer = !!(AdvancedOptions.Flags & EAdvancedCopyOptions::CanCopyComponentContainer);
		const bool bCanCopyComponent = !!(AdvancedOptions.Flags & EAdvancedCopyOptions::CanCopyComponent);

		// Copy non-component properties from the old actor to the new actor
		// @todo sequencer: Most of this block of code was borrowed (pasted) from UEditorEngine::ConvertActors().  If we end up being able to share these code bodies, that would be nice!
		TSet<UObject*> ModifiedObjects;
		for (UProperty* Property = ActorClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			const bool bIsTransient = !!(Property->PropertyFlags & CPF_Transient);
			const bool bIsComponentContainer = !!(Property->PropertyFlags & CPF_ContainsInstancedReference);
			const bool bIsComponentProp = !!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference));
			const bool bIsBlueprintReadonly = !!(Options.Flags & EditorUtilities::ECopyOptions::FilterBlueprintReadOnly) && !!(Property->PropertyFlags & CPF_BlueprintReadOnly);
			const bool bIsIdentical = Property->Identical_InContainer(SourceActor, TargetActor);

			if (bIsTransient && !bIsIdentical)
			{
				PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("Non-Identical Transient Properyt found: %s"), *Property->GetName());
			}

			if (bIsComponentContainer && !bIsIdentical)
			{
				PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("Non-Identical Component Container Properyt found: %s"), *Property->GetName());
			}

			if (bIsComponentProp && !bIsIdentical)
			{
				PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("Non-Identical Component Properyt found: %s"), *Property->GetName());
			}
			if ((bCanCopyTransient || !bIsTransient) && !bIsIdentical && (bCanCopyComponetContainer || !bIsComponentContainer) && (bCanCopyComponent || !bIsComponentProp) && !bIsBlueprintReadonly)
			{
				const bool bIsSafeToCopy = !(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
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

						if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
						{
							TargetActor->PreEditChange(Property);
						}

						// Determine which archetype instances match the current property value of the target actor (before it gets changed). We only want to propagate the change to those instances.
						TArray<UObject*> ArchetypeInstancesToChange;
						if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
						{
							for (AActor* ArchetypeInstance : ArchetypeInstances)
							{
								if (ArchetypeInstance != nullptr && Property->Identical_InContainer(ArchetypeInstance, TargetActor))
								{
									ArchetypeInstancesToChange.Add(ArchetypeInstance);
								}
							}
						}

						EditorUtilities::CopySingleProperty(SourceActor, TargetActor, Property);

						if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
						{
							FPropertyChangedEvent PropertyChangedEvent(Property);
							TargetActor->PostEditChangeProperty(PropertyChangedEvent);
						}

						if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
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

									EditorUtilities::CopySingleProperty(TargetActor, ArchetypeInstance, Property);
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
			UActorComponent* TargetComponent = CopyActorPropertiesLocal::FindMatchingComponentInstance(SourceComponent, TargetActor, TargetComponents, TargetComponentIndex);

			if (TargetComponent != nullptr)
			{
				if (OutMatchingComponentMap != nullptr)
				{
					OutMatchingComponentMap->Add(SourceComponent, TargetComponent);
				}

				UClass* ComponentClass = SourceComponent->GetClass();
				check(ComponentClass == TargetComponent->GetClass());

				// Build a list of matching component archetype instances for propagation (if requested)
				TArray<UActorComponent*> ComponentArchetypeInstances;
				if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
				{
					for (AActor* ArchetypeInstance : ArchetypeInstances)
					{
						if (ArchetypeInstance != nullptr)
						{
							UActorComponent* ComponentArchetypeInstance = EditorUtilities::FindMatchingComponentInstance(TargetComponent, ArchetypeInstance);
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
					const bool bIsComponentContainer = !!(Property->PropertyFlags & CPF_ContainsInstancedReference);
					const bool bIsComponentProp = !bIsComponentContainer && !!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference));
					const bool bIsTransform =
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);

					if (bIsTransient && !bIsIdentical)
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("  Non-Identical Transient Properyt found: %s"), *Property->GetName());
					}

					if (bIsComponentContainer && !bIsIdentical)
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("  Non-Identical Component Container Properyt found: %s"), *Property->GetName());
					}

					if (bIsComponentProp && !bIsIdentical)
					{
						PREFABTOOL_LOG(/*UE_LOG(LogTemp,*/ Display, TEXT("  Non-Identical Component Properyt found: %s"), *Property->GetName());
					}

					if (!bIsIdentical && (bCanCopyTransient || !bIsTransient) && (bCanCopyComponetContainer || !bIsComponentContainer) && (bCanCopyComponent || !bIsComponentProp) 
						&& !SourceUCSModifiedProperties.Contains(Property)
						&& (!bIsTransform || SourceComponent != SourceActor->GetRootComponent() || (!SourceActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !TargetActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))))
					{
						const bool bIsSafeToCopy = !(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
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

								if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
								{
									// @todo simulate: Should we be calling this on the component instead?
									TargetActor->PreEditChange(Property);
								}

								// Determine which component archetype instances match the current property value of the target component (before it gets changed). We only want to propagate the change to those instances.
								TArray<UActorComponent*> ComponentArchetypeInstancesToChange;
								if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
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

								EditorUtilities::CopySingleProperty(SourceComponent, TargetComponent, Property);

								if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
								{
									FPropertyChangedEvent PropertyChangedEvent(Property);
									TargetActor->PostEditChangeProperty(PropertyChangedEvent);
								}

								if (Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances)
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

											EditorUtilities::CopySingleProperty(TargetComponent, ComponentArchetypeInstance, Property);
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

				for (UActorComponent* ModifiedComponentInstance : ComponentInstancesToReregister)
				{
					ModifiedComponentInstance->RegisterComponent();
				}
			}
		}

		if (!bIsPreviewing && CopiedPropertyCount > 0 && TargetActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && TargetActor->GetClass()->HasAllClassFlags(CLASS_CompiledFromBlueprint))
		{
			FBlueprintEditorUtils::PostEditChangeBlueprintActors(CastChecked<UBlueprint>(TargetActor->GetClass()->ClassGeneratedBy));
		}

		// If one of the changed properties was part of the actor's transformation, then we'll call PostEditMove too.
		if (!bIsPreviewing && bTransformChanged)
		{
			if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditMove)
			{
				const bool bFinishedMove = true;
				TargetActor->PostEditMove(bFinishedMove);
			}
		}

		return CopiedPropertyCount;
	}
}

#undef LOCTEXT_NAMESPACE
