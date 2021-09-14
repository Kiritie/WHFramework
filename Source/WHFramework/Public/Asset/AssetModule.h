// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "DataAsset/CharacterDataAsset.h"
#include "DataAsset/KnowledgeDataAsset.h"
#include "DataAsset/TeacherMediaDataAsset.h"
#include "Base/ModuleBase.h"
#include "Function/Sound/SynthesizeSpeech/SSSoundPreloadingIntreface.h"
#include "Character/Base/TD/TDCharacterBase.h"

#include "AssetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AAssetModule : public AModuleBase, public ISSSoundPreloadingIntreface
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AAssetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;
	
	/// 合成语言声音预加载
	virtual void PreloadingSynthesizeSpeechSounds_Implementation(USynthesizeSpeechDownloadManager* SynthesizeSpeechDownloadManager) override;

	//////////////////////////////////////////////////////////////////////////
	/// Knowledge
protected:
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	UKnowledgeDataAsset* KnowledgeDataAsset;

public:
	UFUNCTION(BlueprintPure, Category = "Knowledge")
	UKnowledgeDataAsset* GetKnowledgeDataAsset() const { return KnowledgeDataAsset; }

	UFUNCTION(BlueprintPure, Category = "Knowledge")
	FKnowledgeInfo GetKnowledgeInfoByName(const FString& InName) const;

	UFUNCTION(BlueprintPure, Category = "Knowledge")
	TSubclassOf<class ATeachingItem> GetTeachingItemClassByName(const FString& InName) const;

	//////////////////////////////////////////////////////////////////////////
	/// Character
protected:
	UPROPERTY(EditAnywhere, Category = "Character")
	UCharacterDataAsset* CharacterDataAsset;
public:
	UFUNCTION(BlueprintPure, Category = "Character")
	UCharacterDataAsset* GetCharacterDataAsset() const { return CharacterDataAsset;}

	UFUNCTION(BlueprintPure, Category = "Character")
	FCharacterInfo GetCharacterInfoByName(const FName InName) const;

	//////////////////////////////////////////////////////////////////////////
	/// TeacherMedia
protected:
	UPROPERTY(EditAnywhere, Category = "TeacherMedia")
	UTeacherMediaDataAsset* TeacherMediaDataAsset;
	
public:
	UFUNCTION(BlueprintPure, Category = "TeacherMedia")
	UTeacherMediaDataAsset* GetTeacherMediaDataAsset() const { return TeacherMediaDataAsset;}

	UFUNCTION(BlueprintPure, Category = "TeacherMedia")
	FTeacherMediaInfo GetTeacherMediaDataAssetByName(const FName InName) const;

	//////////////////////////////////////////////////////////////////////////
	/// Display
protected:
	UPROPERTY(EditAnywhere, Category = "Display")
	UDisplayDataAsset* DisplayDataAsset;

public:
	UFUNCTION(BlueprintPure, Category = "Display")
	UDisplayDataAsset* GetDisplayDataAsset() const { return DisplayDataAsset; }
};
