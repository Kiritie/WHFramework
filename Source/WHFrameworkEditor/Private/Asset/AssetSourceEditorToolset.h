#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AssetSourceEditorToolset.generated.h"

	/** 检查主资产索引与导入来源；操作后由调用方显式保存资产。 */
UCLASS()
class UAssetSourceEditorToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** 检查或修复指定重定向器，拒绝白名单以外的引用包。 */
	UFUNCTION(meta = (AICallable))
	static FString FixupRedirectorPackages(const TArray<FString>& PackageNames, const TArray<FString>& ExpectedReferencers, bool bApply);

	/** 只读查询当前 Asset Manager 中的主资产路径。 */
	UFUNCTION(meta = (AICallable))
	static FString InspectPrimaryAssetPaths(const TArray<FString>& PrimaryIds);

	UFUNCTION(meta = (AICallable))
	static FString InspectAssetSource(UObject* Asset);

	/** 只读计算单块单层 Texture2D 的源图首级原始像素哈希。 */
	UFUNCTION(meta = (AICallable))
	static FString InspectTextureSourcePixels(UObject* Asset);

	UFUNCTION(meta = (AICallable))
	static FString RelocateAssetSource(UObject* Asset, const FString& ExpectedPackage,
		const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, bool bApply);

	/** 校验旧来源和新 PNG 后重导入贴图，不自动保存。 */
	UFUNCTION(meta = (AICallable))
	static FString ReimportTextureFromSource(UObject* Asset, const FString& ExpectedPackage,
		const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bApply);

	/** CSV 与简单曲线完全一致时更新来源；旧表缺少导入插值设置时仅在各行设置一致时补齐。 */
	UFUNCTION(meta = (AICallable))
	static FString RebindEquivalentCurveTableSource(UObject* Asset, const FString& ExpectedPackage,
		const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bApply);

	/** 仅在 CSV 与现有简单曲线完全一致时重导入；不自动保存。 */
	UFUNCTION(meta = (AICallable))
	static FString ReimportEquivalentCurveTable(UObject* Asset, const FString& ExpectedPackage,
		const FString& ExpectedRelativeSource, const FString& ExpectedMD5, bool bApply);

	/** 从已校验的 CSV 更新已有简单曲线键，并使曲线缓存失效；不自动保存。 */
	UFUNCTION(meta = (AICallable))
	static FString UpdateSimpleCurveTableFromSource(UObject* Asset, const FString& ExpectedPackage,
		const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bAllowNewRows, bool bApply);
};
