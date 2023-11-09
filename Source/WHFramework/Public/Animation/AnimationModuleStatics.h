// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "AnimationModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAnimationModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* MoveActorTo
	* 将插值对象移动到期望位置,优先使用锚点位置
	* @param WorldContextObject
	* @param Actor							需要插值的对象
	* @param TargetPoint					如果存在将应用为插值目标位置
	* @param TargetTransform				插值目标位置
	* @param bUseRotator					是否应用目标旋转
	* @param bUseScale						是否应用目标缩放
	* @param bEaseIn						是否淡入
	* @param bEaseOut						是否淡出
	* @param BlendExp						淡入淡出的阈值
	* @param DurationTime				插值所需应用到的时长
	* @param bForceShortestRotationPath		是否应用最短旋转路径
	* @param MoveAction
	* @param LatentInfo
	* @return void
	*/
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject", BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath", UnsafeDuringActorConstruction = "true"), Category="AnimationModuleStatics")
	static void MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* TargetPoint, FTransform TargetTransform, bool bUseRotator, bool bUseScale, float DurationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject",BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath",UnsafeDuringActorConstruction = "true"), Category="AnimationModuleStatics")
	static void RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator Rotator, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject",BlendExp= "1.0", ApplicationTime= "1.0", ExpandEnumAsExecs= "MoveAction", AdvancedDisplay = "bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath",UnsafeDuringActorConstruction = "true"), Category="AnimationModuleStatics")
	static void ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector Scale, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo);
	
	/**
	* CancelableDelay
	* 可以手动停止的延时，用法同Delay
	* @param WorldContextObject
	* @param Duration						延时时长
	* @param CancelableDelayAction
	* @param LatentInfo
	* @return void
	*/
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", WorldContext= "WorldContextObject", Duration="0.2", ExpandEnumAsExecs= "CancelableDelayAction", Keywords="sleep", UnsafeDuringActorConstruction = "true"), Category="AnimationModuleStatics")
	static void	CancelableDelay(UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo);
};