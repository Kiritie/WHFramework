#pragma once

#include "CoreMinimal.h"

// 事件
DEFINE_LOG_CATEGORY_STATIC(WHAsset, Log, All);

// 流程
DEFINE_LOG_CATEGORY_STATIC(WHProcedure, Log, All);

// 事件
DEFINE_LOG_CATEGORY_STATIC(WHEvent, Log, All);

// 网络
DEFINE_LOG_CATEGORY_STATIC(WHNetwork, Log, All);

// Web请求
DEFINE_LOG_CATEGORY_STATIC(WHWebRequest, Log, All);

// 打印
#define WH_LOG(CategoryName, Verbosity, Format, ...) \
{ \
	UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
}

// 断言实现
#define WH_ENSURE_IMPL(Capture, InExpression, InFormat, ...) \
(LIKELY(!!(InExpression)) || (([Capture] () ->bool \
{ \
	static bool bExecuted = false; \
	if ((!bExecuted)) \
	{ \
		bExecuted = true; \
		FString Expr = #InExpression;\
		FString File = __FILE__;\
		int32 Line = __LINE__;\
		UE_LOG(LogTemp, Error, TEXT("IVREAL UE4 CRASH"));\
		UE_LOG(LogTemp, Error, TEXT("Expression : %s, %s, %d"), *Expr, *File, Line);\
		UE_LOG(LogTemp, Error, InFormat, ##__VA_ARGS__); \
	} \
	return false; \
})()))

// 用这两个断言, 代替多数仅需要在编辑器中进行判断的断言
#if WITH_EDITOR
	#define ensureEditor(InExpression) ensureAlways(InExpression)
	#define ensureEditorMsgf( InExpression, InFormat, ... ) ensureAlwaysMsgf( InExpression, InFormat, ##__VA_ARGS__)
#else
	#define ensureEditor(InExpression) WH_ENSURE_IMPL( , InExpression, TEXT(""))
	#define ensureEditorMsgf( InExpression, InFormat, ... ) WH_ENSURE_IMPL(&, InExpression, InFormat, ##__VA_ARGS__)
#endif

/*
 * 输出调试信息到游戏窗口
 * @param Message 消息内容
 * @param DisplayColor 显示颜色
 * @param Duration 程持续时间
 * @param bNewerOnTop 更新在顶部
 */
FORCEINLINE void WHDebug(const FString& Message, FColor DisplayColor = FColor::Cyan, float Duration = 1.5f, bool bNewerOnTop = true)
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, DisplayColor, Message, bNewerOnTop);
	}
}
