#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DetectionGaugeWidget.generated.h"

class UProgressBar;
class UImage;
class UDataTable;
class APawn;

// 경비 머리 위에 붙는 시야·청각 통합 게이지 위젯. 값을 스스로 찾아오지 않고
// SetPerceptionGaugePercents()로 외부(AGuardCharacter)가 전달한 값을 표시한다.
// "이 게이지가 누구 것인지"는 이 위젯을 소유한 WidgetComponent가 어느 경비 캐릭터에
// 붙어 있는지로 이미 결정되기 때문에, 위젯 스스로 월드를 순회해 대상을 찾을 필요가 없다.
//
// SightGaugeBar(시야), HearingGaugeBar(청각)는 파생 WBP에 같은 이름의 ProgressBar를 배치해야
// 자동으로 연결된다(BindWidgetOptional이라 없어도 크래시하지 않고 그냥 갱신만 안 된다).
UCLASS()
class HEAVYHANDED_API UDetectionGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 기존 시야 전용 호출과의 호환용. 통합 위젯에는 SetPerceptionGaugePercents를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Perception")
	void SetGaugePercent(float InPercent0to100);

	// 두 값 모두 0~100. 감각 활성화 여부와 두 게이지 값을 함께 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Perception")
	void SetPerceptionGaugePercents(float SightPercent, float HearingPercent, bool bSightEnabled, bool bHearingEnabled);

	FString GetGaugeBindingDebugInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Guard|Aggro")
	void SetAggroTarget(AActor* TargetActor);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SightGaugeBar;

	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HearingGaugeBar;

	UPROPERTY(BlueprintReadOnly, Category = "Guard|Aggro", meta = (BindWidgetOptional))
	TObjectPtr<UImage> AggroTargetImage;

	// FJobInfo 행의 Portrait를 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Aggro", meta = (RequiredAssetDataTags = "RowStructure=/Script/HeavyHanded.JobInfo"))
	TObjectPtr<UDataTable> JobDataTable;

	// 캐릭터 BP 클래스와 직업 데이터 테이블의 행 이름을 연결한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Aggro")
	TMap<TSubclassOf<APawn>, FName> TargetPortraitRows;

	// 활성화된 두 게이지가 모두 0일 때 전체 위젯을 숨길지.
	UPROPERTY(EditAnywhere, Category = "Guard|Perception")
	bool bHideWhenEmpty = true;

private:
	// 기존 통합 게이지 화면·로그 진단에 함께 출력한다.
	FString AggroTargetDebugInfo;
};
