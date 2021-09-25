// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SpawnPool.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API USpawnPool : public UObject
{
	GENERATED_BODY()

public:
	USpawnPool();

private:
	/// 限制大小
	UPROPERTY(VisibleAnywhere)
	int32 Limit;
	/// Actor类型
	UPROPERTY(VisibleAnywhere)
	TSubclassOf<AActor> Type;
	/// Actor数量
	UPROPERTY(VisibleAnywhere)
	int32 Count;
	/// Actor列表
	TQueue<AActor*> Queue;

public:
	/**
	* 初始化
	* @param InLimit 限制大小
	* @param InType 类型
	*/
	void Initialize(int32 InLimit, TSubclassOf<AActor> InType);
	/**
	* 生成Actor
	*/
	AActor* Spawn();
	/**
	* 回收Actor
	*/
	void Despawn(AActor* InActor);
	/**
	* 清理Actor
	*/
	void Clear();

public:
	int32 GetLimit() const;

	int32 GetCount() const;
};
