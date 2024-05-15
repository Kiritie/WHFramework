// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/Base/ModuleBase.h"

#include "NetworkModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UNetworkModule : public UModuleBase
{
	GENERATED_BODY()
				
	GENERATED_MODULE(UNetworkModule)

public:	
	// ParamSets default values for this actor's properties
	UNetworkModule();

	~UNetworkModule();

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

public:
	virtual FString GetModuleDebugMessage() override;

protected:
	UPROPERTY(EditAnywhere)
	bool bLocalMode;

	UPROPERTY(EditAnywhere)
	FString ServerURL;

	UPROPERTY(EditAnywhere)
	int32 ServerPort;
public:
	UFUNCTION(BlueprintPure)
	bool IsLocalMode() const { return bLocalMode; }

	UFUNCTION(BlueprintCallable)
	void SetLocalMode(bool bInLocalMode) { this->bLocalMode = bInLocalMode; }

	UFUNCTION(BlueprintPure)
	FString GetServerURL() const;

	UFUNCTION(BlueprintCallable)
	void SetServerURL(const FString& InServerURL) { this->ServerURL = InServerURL; }

	UFUNCTION(BlueprintPure)
	int32 GetServerPort() const { return ServerPort; }

	UFUNCTION(BlueprintCallable)
	void SetServerPort(int32 InServerPort) { this->ServerPort = InServerPort; }

	UFUNCTION(BlueprintPure)
	FString GetServerLocalURL() const;

	UFUNCTION(BlueprintPure)
	FString GetServerLocalIP() const;

	UFUNCTION(BlueprintPure)
	int32 GetServerLocalPort() const;

public:
	UFUNCTION(BlueprintCallable)
	bool ConnectServer(const FString& InServerURL = TEXT(""), int32 InServerPort = -1, const FString& InOptions = FString(TEXT("")));

	UFUNCTION(BlueprintCallable)
	bool DisconnectServer(const FString& InLevelName);
};
