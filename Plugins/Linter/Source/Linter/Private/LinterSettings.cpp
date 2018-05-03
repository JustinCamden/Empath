// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "LinterSettings.h"
//#include "LevelSequence.h"
#include "AIController.h"
#include "MediaTexture.h"
#include "MediaPlayer.h"
#include "Resources/Version.h"
#if ENGINE_MINOR_VERSION < 18
#include "MediaSoundWave.h"
#include "Engine/DestructibleMesh.h"
#endif
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/MorphTarget.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimComposite.h"
#include "Animation/Rig.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "Camera/CameraAnim.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorTutorial.h"
#include "Engine/Font.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureLightProfile.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Engine/ObjectLibrary.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Matinee/MatineeAnimInterface.h"
#include "GameFramework/TouchInterface.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "Particles/ParticleSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Sound/AudioVolume.h"
#include "Sound/ReverbEffect.h"
#if ENGINE_MINOR_VERSION >= 18
#include "Sound/SoundWave.h"
#endif
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "VectorField/VectorField.h"
#include "VectorField/VectorFieldAnimated.h"
#include "VectorField/VectorFieldStatic.h"
#include "WidgetBlueprint.h"



ULinterSettings::ULinterSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDoAssetNamingConventionChecks(true)
	, bDoFolderNameChecks(true)
	, bDoFolderNamePascalCaseCheck(true)
	, bDoFolderNameSpaceCheck(true)
	, bDoFolderNameUnicodeCheck(true)
	, bUseTopLevelFolderForAssets(true)
	, bMapsInMapsFolder(true)
	, bDoFolderNameAssetsRedundantCheck(true)
	, bDoFolderNameAssetTypeRedundantCheck(true)
	, bBPCompiles(true)
	, bBPVariableChecks(true)
	, bBPVarPascalCase(true)
	, bBPVarBoolPrefix(true)
	, bBPVarBoolIsQuestion(true)
	, bBPVarAtomicInclusion(true)
	, bBPVarArrayPlurals(true)
	, bBPVarExposeOnSpawnEditable(true)
	, bBPVarEditableMustHaveTooltip(true)
	, bBPVarEditableCategories(true)
	, bBPVarSaveNoConfig(true)
	, bBPFunctionChecks(true)
	, bBPFunctionsShouldHaveReturnNodes(true)
	, FunctionNodeLimit(50)
	, bBPPublicFunctionsShouldHaveDescription(false)
	, bStaticMeshValidUVs(true)
	, bParticleSystemEmittersShouldBeNamed(true)
	, bLevelsShouldHaveNoErrorsOrWarnings(true)
	, DokuwikiExportNamespace(TEXT("api"))
{
#define ADD_SETTING(TYPE, PREFIX, SUFFIX) AssetNameSettings.Add(TYPE, FAssetTypeNameSetting(TEXT(PREFIX), TEXT(SUFFIX)));

	// Animations
	ADD_SETTING(UAimOffsetBlendSpace::StaticClass(), "AO_", "");
	ADD_SETTING(UAimOffsetBlendSpace1D::StaticClass(), "AO_", "");
	ADD_SETTING(UAnimBlueprint::StaticClass(), "ABP_", "");
	ADD_SETTING(UAnimComposite::StaticClass(), "AC_", "");
	ADD_SETTING(UAnimMontage::StaticClass(), "AM_", "");
	ADD_SETTING(UAnimSequence::StaticClass(), "A_", "");
	ADD_SETTING(UBlendSpace::StaticClass(), "BS_", "");
	ADD_SETTING(UBlendSpace1D::StaticClass(), "BS_", "");
	//ADD_SETTING(ULevelSequence, "LS_", "");
	ADD_SETTING(UMorphTarget::StaticClass(), "MT_", "");
	//ADD_SETTING(UPaperFlipbook, "PFB_", "");
	ADD_SETTING(URig::StaticClass(), "Rig_", "");
	ADD_SETTING(USkeletalMesh::StaticClass(), "SK_", "");
	ADD_SETTING(USkeleton::StaticClass(), "SKEL_", "");

	// Artificial Intelligence
	ADD_SETTING(AAIController::StaticClass(), "AIC_", "");
	ADD_SETTING(UBehaviorTree::StaticClass(), "BT_", "");
	ADD_SETTING(UBlackboardData::StaticClass(), "BB_", "");
	ADD_SETTING(UBTDecorator::StaticClass(), "BTDecorator_", "");
	ADD_SETTING(UBTService::StaticClass(), "BTService_", "");
	ADD_SETTING(UBTTaskNode::StaticClass(), "BTTask_", "");

	// Blueprints
	ADD_SETTING(UBlueprint::StaticClass(), "BP_", "");
	ADD_SETTING(UBlueprintFunctionLibrary::StaticClass(), "BPFL_", "");
	ADD_SETTING(UInterface::StaticClass(), "BPI_", "");
	ADD_SETTING(UEditorTutorial::StaticClass(), "TBP_", "");
	ADD_SETTING(UUserDefinedEnum::StaticClass(), "E", "");
	ADD_SETTING(UUserDefinedStruct::StaticClass(), "F", "");

	// Materials
	ADD_SETTING(UMaterial::StaticClass(), "M_", "");
	ADD_SETTING(UMaterialFunction::StaticClass(), "MF_", "");
	ADD_SETTING(UMaterialInstance::StaticClass(), "MI_", "");
	ADD_SETTING(UMaterialInstanceConstant::StaticClass(), "MI_", "");
	ADD_SETTING(UMaterialParameterCollection::StaticClass(), "MPC_", "");
	ADD_SETTING(USubsurfaceProfile::StaticClass(), "SP_", "");

	// Textures
	ADD_SETTING(UTexture2D::StaticClass(), "T_", "");
	ADD_SETTING(UTextureCube::StaticClass(), "TC_", "");
	ADD_SETTING(UMediaTexture::StaticClass(), "MT_", "");
	ADD_SETTING(UTextureRenderTarget2D::StaticClass(), "RT_", "");
	ADD_SETTING(UTextureRenderTargetCube::StaticClass(), "RTC_", "");
	ADD_SETTING(UTextureLightProfile::StaticClass(), "TLP_", "");

	// Miscellaneous
	ADD_SETTING(UVectorFieldAnimated::StaticClass(), "VFA_", "");
	ADD_SETTING(UCameraAnim::StaticClass(), "CA_", "");
	ADD_SETTING(UCurveLinearColor::StaticClass(), "Curve_", "");
	ADD_SETTING(UCurveTable::StaticClass(), "Curve_", "");
	//ADD_SETTING(UDataAsset::StaticClass(), "T_", "");
	ADD_SETTING(UDataTable::StaticClass(), "DT_", "");
	ADD_SETTING(UCurveFloat::StaticClass(), "Curve_", "");
	//ADD_SETTING(UFoliageType::StaticClass(), "FT_", "");
	ADD_SETTING(UForceFeedbackEffect::StaticClass(), "FFE_", "");
	//ADD_SETTING(ULandscapeGrassType::StaticClass(), "LG_", "");
	//ADD_SETTING(ULandscapeLayerInfoObject::StaticClass(), "LL_", "");
	ADD_SETTING(UMatineeAnimInterface::StaticClass(), "Matinee_", "");
	ADD_SETTING(UMediaPlayer::StaticClass(), "MP_", "");
	ADD_SETTING(UObjectLibrary::StaticClass(), "OL_", "");
	//ADD_SETTING(FRedirector::StaticClass(), "T_", "");
	//ADD_SETTING(UPaperSpriteSheet::StaticClass(), "T_", "");
	ADD_SETTING(UVectorFieldStatic::StaticClass(), "VF_", "");
	ADD_SETTING(UTouchInterface::StaticClass(), "TI_", "");
	ADD_SETTING(UCurveVector::StaticClass(), "Curve_", "");

	// Paper 2D

	// Physics
	ADD_SETTING(UPhysicalMaterial::StaticClass(), "PM_", "");
	ADD_SETTING(UPhysicsAsset::StaticClass(), "PHYS_", "");
#if ENGINE_MINOR_VERSION < 18
	ADD_SETTING(UDestructibleMesh::StaticClass(), "DM_", "");
#endif

	// Sounds
	ADD_SETTING(UDialogueVoice::StaticClass(), "DV_", "");
	ADD_SETTING(UDialogueWave::StaticClass(), "DW_", "");
#if ENGINE_MINOR_VERSION < 18
	ADD_SETTING(UMediaSoundWave::StaticClass(), "MSW_", "");
#endif
	ADD_SETTING(UReverbEffect::StaticClass(), "Reverb_", "");
	ADD_SETTING(USoundAttenuation::StaticClass(), "ATT_", "");
	ADD_SETTING(USoundClass::StaticClass(), "", "");
	ADD_SETTING(USoundConcurrency::StaticClass(), "", "_SC");
	ADD_SETTING(USoundCue::StaticClass(), "A_", "_Cue");
	ADD_SETTING(USoundMix::StaticClass(), "Mix_", "");
	ADD_SETTING(USoundWave::StaticClass(), "A_", "");

	// User Interface
	ADD_SETTING(UFont::StaticClass(), "Font_", "");
	ADD_SETTING(USlateBrushAsset::StaticClass(), "Brush_", "");
	ADD_SETTING(USlateWidgetStyleAsset::StaticClass(), "Style_", "");
	ADD_SETTING(UWidgetBlueprint::StaticClass(), "WBP_", "");

	// Effects
	ADD_SETTING(UParticleSystem::StaticClass(), "PS_", "");

#undef ADD_SETTING
	// Post Process Material Setting (Special Case Unfortunately)
	PostProcessMaterialSetting.Prefix = TEXT("PP_");
	// Macro Library Setting (Special Case Unfortunately)
	MacroLibrarySetting.Prefix = TEXT("BPML_");	
}