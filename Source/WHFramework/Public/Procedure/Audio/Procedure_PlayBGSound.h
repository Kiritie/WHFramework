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
// class WHFRAMEWORK_API UProcedure_PlaySingleSound : public UProcedureBase
// {
// 	GENERATED_BODY()
//
// public:
// 	UProcedure_PlaySingleSound();
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
// 	virtual void ServerOnEnter(UProcedureBase* InLastProcedure) override;
//
// 	virtual void ServerOnLeave() override;
// };
