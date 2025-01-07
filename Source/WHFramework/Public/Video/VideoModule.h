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
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

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
public:
	UFUNCTION(BlueprintCallable)
	virtual void ApplyVideoSettings();

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EWindowModeN WindowMode;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EWindowResolution WindowResolution;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	bool bEnableVSync;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	bool bEnableDynamicResolution;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality GlobalVideoQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality ViewDistanceQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality ShadowQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality GlobalIlluminationQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality ReflectionQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality AntiAliasingQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality TextureQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality VisualEffectQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality PostProcessingQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality FoliageQuality;

	UPROPERTY(EditAnywhere, Category = "VideoSetting")
	EVideoQuality ShadingQuality;

public:
	UFUNCTION(BlueprintPure)
	EWindowModeN GetWindowMode() const { return WindowMode; }

	UFUNCTION(BlueprintCallable)
	void SetWindowMode(EWindowModeN InMode, bool bApply = false);

	UFUNCTION(BlueprintPure)
	EWindowResolution GetWindowResolution() const { return WindowResolution; }

	UFUNCTION(BlueprintCallable)
	void SetWindowResolution(EWindowResolution InResolution, bool bApply = false);

	UFUNCTION(BlueprintPure)
	FIntPoint GetDesktopResolution() const;

	UFUNCTION(BlueprintPure)
	bool IsEnableVSync() const { return bEnableVSync; }

	UFUNCTION(BlueprintCallable)
	void SetEnableVSync(bool bInValue, bool bApply = false);

	UFUNCTION(BlueprintPure)
	bool IsEnableDynamicResolution() const { return bEnableDynamicResolution; }

	UFUNCTION(BlueprintCallable)
	void SetEnableDynamicResolution(bool bInValue, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetGlobalVideoQuality() const { return GlobalVideoQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalVideoQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetViewDistanceQuality() const { return ViewDistanceQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetViewDistanceQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetShadowQuality() const { return ShadowQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetShadowQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetGlobalIlluminationQuality() const { return GlobalIlluminationQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalIlluminationQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetReflectionQuality() const { return ReflectionQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetReflectionQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetAntiAliasingQuality() const { return AntiAliasingQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetAntiAliasingQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetTextureQuality() const { return TextureQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetTextureQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetVisualEffectQuality() const { return VisualEffectQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetVisualEffectQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetPostProcessingQuality() const { return PostProcessingQuality; }
	
	UFUNCTION(BlueprintCallable)
	virtual void SetPostProcessingQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetFoliageQuality() const { return FoliageQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetFoliageQuality(EVideoQuality InQuality, bool bApply = false);

	UFUNCTION(BlueprintPure)
	virtual EVideoQuality GetShadingQuality() const { return ShadingQuality; }

	UFUNCTION(BlueprintCallable)
	virtual void SetShadingQuality(EVideoQuality InQuality, bool bApply = false);

protected:
	UFUNCTION(BlueprintPure)
	virtual UGameUserSettings* GetGameUserSettings() const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
