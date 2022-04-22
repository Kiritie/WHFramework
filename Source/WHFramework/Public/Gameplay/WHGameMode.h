// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "WHGameMode.generated.h"

/**
 * 游戏模式基类
 */
UCLASS()
class WHFRAMEWORK_API AWHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWHGameMode();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	UFUNCTION(BlueprintNativeEvent)
	void Initialize();

	//////////////////////////////////////////////////////////////////////////
	/// Inherits
public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void InitGameState() override;
	
	virtual APlayerController* SpawnPlayerController(ENetRole InRemoteRole, const FString& Options) override;

public:
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void Logout(AController* Exiting) override;

protected:
	virtual void BeginPlay() override;
};
