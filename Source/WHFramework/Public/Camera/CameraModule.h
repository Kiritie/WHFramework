#pragma once

#include "CameraModuleTypes.h"
#include "Debug/DebuggerInterface.h"
#include "Main/Base/ModuleBase.h"
#include "CameraModule.generated.h"

class ACameraManagerBase;
class ACameraRigBase;
class ACameraShotAnchor;
class UCameraModeBase;

UCLASS()
class WHFRAMEWORK_API UCameraModule : public UModuleBase, public IDebuggerInterface
{
	GENERATED_BODY()

	GENERATED_MODULE(UCameraModule)

public:
	UCameraModule();

	~UCameraModule();

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

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Debugger
protected:
	virtual void OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC) override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera Rig
protected:
	UPROPERTY(EditAnywhere, Category = "Rig")
	TSubclassOf<ACameraRigBase> DefaultRigClass;

	UPROPERTY(EditAnywhere, Category = "Mode")
	TSubclassOf<UCameraModeBase> DefaultModeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FCameraConfig CameraConfig;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACameraManagerBase>> CameraManagers;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACameraShotAnchor>> ShotAnchors;

	UPROPERTY(VisibleAnywhere, Category = "Settings")
	FCameraUserSettings UserSettings;

public:
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	ACameraManagerBase* GetCameraManager(int32 InPlayerIndex = 0) const;

	void RegisterCameraManager(ACameraManagerBase* InManager);

	void UnRegisterCameraManager(ACameraManagerBase* InManager);

	void RegisterShotAnchor(ACameraShotAnchor* InAnchor);

	void UnregisterShotAnchor(ACameraShotAnchor* InAnchor);

	const TArray<TWeakObjectPtr<ACameraShotAnchor>>& GetShotAnchors() const { return ShotAnchors; }

	TSubclassOf<ACameraRigBase> GetDefaultRigClass() const { return DefaultRigClass; }

	TSubclassOf<UCameraModeBase> GetDefaultModeClass() const { return DefaultModeClass; }

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	const FCameraConfig& GetCameraConfig() const { return CameraConfig; }

	const FCameraUserSettings& GetUserSettings() const { return UserSettings; }

	void ApplyUserSettings(const FCameraUserSettings& InSettings);
};
