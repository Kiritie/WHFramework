// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "WHPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AWHPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AWHPlayerController();

	//////////////////////////////////////////////////////////////////////////
	/// Comp
protected:
	/// 音频模块网络组件
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Comp")
	class UAudioModuleNetworkComponent* AudioModuleNetworkComp;
public:
	UFUNCTION(BlueprintPure, Category = "Comp")
	UAudioModuleNetworkComponent* GetAudioModuleNetworkComp() const { return AudioModuleNetworkComp; }

protected:
	/// 角色模块网络组件
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Comp")
	class UCharacterModuleNetworkComponent* CharacterModuleNetworkComp;
public:
	UFUNCTION(BlueprintPure, Category = "Comp")
	UCharacterModuleNetworkComponent* GetCharacterModuleNetworkComp() const { return CharacterModuleNetworkComp; }

protected:
	/// 流程模块网络组件
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Comp")
	class UProcedureModuleNetworkComponent* ProcedureModuleNetworkComp;
public:
	UFUNCTION(BlueprintPure, Category = "Comp")
	UProcedureModuleNetworkComponent* GetProcedureModuleNetworkComp() const { return ProcedureModuleNetworkComp; }

protected:
	/// 网络模块网络组件
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Comp")
	class UNetworkModuleNetworkComponent* NetworkModuleNetworkComp;
public:
	UFUNCTION(BlueprintPure, Category = "Comp")
	UNetworkModuleNetworkComponent* GetNetworkModuleNetworkComp() const { return NetworkModuleNetworkComp; }

protected:
	/// 事件模块网络组件
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Comp")
	class UEventModuleNetworkComponent* EventModuleNetworkComp;
public:
	UFUNCTION(BlueprintPure, Category = "Comp")
	UEventModuleNetworkComponent* GetEventModuleNetworkComp() const { return EventModuleNetworkComp; }
}; 