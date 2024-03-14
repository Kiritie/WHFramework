// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "VideoModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "VideoModule.generated.h"

class AMediaPlayerBase;
class UGameUserSettings;
UCLASS()
class WHFRAMEWORK_API UVideoModule : public UModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UVideoModule)

public:	
	// ParamSets default values for this actor's properties
	UVideoModule();

	~UVideoModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual void Load_Implementation() override;

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// MediaPlayer
protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "MediaPlayer")
	TArray<AMediaPlayerBase*> MediaPlayers;

private:
	UPROPERTY(Transient)
	TMap<FName, AMediaPlayerBase*> MediaPlayerMap;

public:
	UFUNCTION(BlueprintCallable)
	void AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure)
	AMediaPlayerBase* GetMediaPlayerByName(const FName InName) const;
	
	//////////////////////////////////////////////////////////////////////////
	/// Movie
public:
	UFUNCTION(BlueprintCallable)
	void PlayMovieForMediaPlayer(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	void PlayMovieForMediaPlayerWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void PauseMovieForMediaPlayer(const FName InName, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void SeekMovieForMediaPlayer(const FName InName, const FTimespan& InTimespan, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	void StopMovieForMediaPlayer(const FName InName, bool bSkip, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// VideoSetting
protected:
	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality GlobalVideoQuality;

public:
	UFUNCTION(BlueprintCallable)
	virtual void ApplyVideoQualitySettings();
	
	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetGlobalVideoQuality() const { return GlobalVideoQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalVideoQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintCallable)
	virtual void SetViewDistanceQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetViewDistanceQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetShadowQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetShadowQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalIlluminationQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetGlobalIlluminationQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetReflectionQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetReflectionQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetAntiAliasingQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetAntiAliasingQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetTextureQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetTextureQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetVisualEffectQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetVisualEffectQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetPostProcessingQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetPostProcessingQuality() const;
	
	UFUNCTION(BlueprintCallable)
	virtual void SetFoliageQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetFoliageQuality() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetShadingQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetShadingQuality() const;

protected:
	UFUNCTION(BlueprintPure)
	virtual UGameUserSettings* GetGameUserSettings() const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
