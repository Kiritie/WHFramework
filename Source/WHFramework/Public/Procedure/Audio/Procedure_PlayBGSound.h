// // Fill out your copyright notice in the Description page of Project Settings.
//
// #pragma once
//
// #include "CoreMinimal.h"
// #include "Procedure/Base/ProcedureBase.h"
// #include "Procedure_PlaySingleSound.generated.h"
//
// /**
//  * 
//  */
// UCLASS()
// class WHFRAMEWORK_API AProcedure_PlaySingleSound : public AProcedureBase
// {
// 	GENERATED_BODY()
//
// public:
// 	AProcedure_PlaySingleSound();
//
// public:
// 	/// 声音
// 	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
// 	class USoundBase* Sound;
// 	/// 音量
// 	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
// 	float Volume;
// 	/// 点位
// 	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Audio")
// 	class ATargetPoint* Point;
//
// public:
// 	virtual void ServerOnEnter_Implementation(AProcedureBase* InLastProcedure) override;
//
// 	virtual void ServerOnLeave_Implementation(AProcedureBase* InNextProcedure) override;
// };
