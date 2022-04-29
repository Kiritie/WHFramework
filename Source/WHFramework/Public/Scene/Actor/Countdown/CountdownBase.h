// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "CountdownBase.generated.h"

UCLASS()
class WHFRAMEWORK_API ACountdownBase : public AActor, public ISceneActorInterface
{
	GENERATED_BODY()
	
public:	
	ACountdownBase();

	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaSeconds) override;

	// 服务器设置倒计时时长
	void ServerSetTotalCountdownTime(float InCountdownTime);

	UFUNCTION()
	void OnRep_TotalCountdownTime();

	UFUNCTION()
	void OnRep_ServerCountdownTime();

	// 更新倒计时的显示样式, 在倒计时时间有变化的时候, 调用
	// 常见时间节点 : 接收到服务器的总共倒计时时间, 开始倒计时
	//              当前客户端与服务器时间差太大, 强制同步
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateCountdownDisplayStyle();

	// 当前倒计时时间
	UPROPERTY(BlueprintReadWrite)
	float CountdownTime;
	// 服务器当前的倒计时, 用于处理客户端和服务器时间差过大
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ServerCountdownTime)
	int32 ServerCountdownTime;
	// 总共倒计时时长
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, ReplicatedUsing = OnRep_TotalCountdownTime)
	float TotalCountdownTime;
};
