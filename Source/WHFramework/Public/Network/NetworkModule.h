// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/Base/ModuleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "NetworkModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ANetworkModule : public AModuleBase
{
	GENERATED_BODY()
				
	GENERATED_MODULE(ANetworkModule)

public:	
	// ParamSets default values for this actor's properties
	ANetworkModule();

	~ANetworkModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

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
