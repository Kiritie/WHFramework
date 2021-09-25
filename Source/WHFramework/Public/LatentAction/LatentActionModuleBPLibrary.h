// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LatentActionModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LatentActionModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ULatentActionModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static ALatentActionModule* LatentActionModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "LatentFunction")
	static ALatentActionModule* GetLatentActionModule(const UObject* InWorldContext);

	/**
	* MoveActorTo
	* 将插值对象移动到期望位置,优先使用锚点位置
	* @param WorldContextObject
	* @param Actor							需要插值的对象
	* @param InTargetPoint					如果存在将应用为插值目标位置
	* @param InTargetTransform				插值目标位置
	* @param bUseRotator					是否应用目标旋转
	* @param bUseScale						是否应用目标缩放
	* @param bEaseIn						是否淡入
	* @param bEaseOut						是否淡出
	* @param BlendExp						淡入淡出的阈值
	* @param ApplicationTime				插值所需应用到的时长
	* @param bForceShortestRotationPath		是否应用最短旋转路径
	* @param MoveAction
	* @param LatentInfo
	* @return void
	*/
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject", BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath", UnsafeDuringActorConstruction = "true"), Category="LatentFunction")
	static void MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* InTargetPoint, FTransform InTargetTransform, bool bUseRotator, bool bUseScale, float ApplicationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject",BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath",UnsafeDuringActorConstruction = "true"), Category="LatentFunction")
	static void RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator InRotator, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject",BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath",UnsafeDuringActorConstruction = "true"), Category="LatentFunction")
	static void ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector InScale3D, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);
	
	/**
	* DelayPlus
	* 可以手动停止的延时，用法同Delay
	* @param WorldContextObject
	* @param Duration						延时时长
	* @param DelayPlusAction
	* @param LatentInfo
	* @return void
	*/
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject", Duration="0.2", ExpandEnumAsExecs= "DelayPlusAction", Keywords="sleep", UnsafeDuringActorConstruction = "true"), Category="LatentFunction")
	static void	DelayPlus(UObject* WorldContextObject, float Duration, TEnumAsByte<EDelayPlusAction::Type> DelayPlusAction, FLatentActionInfo LatentInfo);
};