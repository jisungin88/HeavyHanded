#include "UI/DetectionGaugeWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Data/JobDataInfo.h"
#include "GameFramework/Pawn.h"

FString UDetectionGaugeWidget::GetGaugeBindingDebugInfo() const
{
	const FVector2D LayoutSize = GetCachedGeometry().GetLocalSize();
	const FVector2D SightSize = IsValid(SightGaugeBar) ? SightGaugeBar->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FVector2D HearingSize = IsValid(HearingGaugeBar) ? HearingGaugeBar->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FVector2D ImageSize = IsValid(AggroTargetImage) ? AggroTargetImage->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FString ImageInfo = FString::Printf(TEXT(" | [대상 이미지] %s 연결=%s 표시=%s 배치=%.0fx%.0f 투명도=%.2f 색알파=%.2f"), *AggroTargetDebugInfo, IsValid(AggroTargetImage) ? TEXT("예") : TEXT("아니오"), IsValid(AggroTargetImage) && AggroTargetImage->IsVisible() ? TEXT("켜짐") : TEXT("숨김"), ImageSize.X, ImageSize.Y, IsValid(AggroTargetImage) ? AggroTargetImage->GetRenderOpacity() : 0.f, IsValid(AggroTargetImage) ? AggroTargetImage->GetColorAndOpacity().A : 0.f);
	return FString::Printf(TEXT("시야바=%s 청각바=%s 위젯표시=%s 요청크기=%.0fx%.0f 실제배치=%.0fx%.0f 투명도=%.2f 배율=(%.2f,%.2f) 시야바배치=%.0fx%.0f 청각바배치=%.0fx%.0f"), IsValid(SightGaugeBar) ? TEXT("연결") : TEXT("없음(이름 확인)"), IsValid(HearingGaugeBar) ? TEXT("연결") : TEXT("없음(이름 확인)"), IsVisible() ? TEXT("켜짐") : TEXT("숨김"), GetDesiredSize().X, GetDesiredSize().Y, LayoutSize.X, LayoutSize.Y, GetRenderOpacity(), GetRenderTransform().Scale.X, GetRenderTransform().Scale.Y, SightSize.X, SightSize.Y, HearingSize.X, HearingSize.Y) + ImageInfo;
}

void UDetectionGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetAggroTarget(nullptr);

	// 소유자(AGuardCharacter)가 첫 게이지 갱신을 호출하기 전까지, 위젯이
	// 디자이너에서 설정한 기본 Visibility(보통 Visible)로 잠깐 노출되는 플리커를 막는다.
	// bHideWhenEmpty가 꺼져 있으면(=항상 표시 의도) 여기서 숨기면 안 된다 - 그 경우
	// 첫 갱신 전에도 디자이너의 표시 설정을 유지한다.
	if (bHideWhenEmpty)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UDetectionGaugeWidget::SetGaugePercent(float InPercent0to100)
{
	SetPerceptionGaugePercents(InPercent0to100, 0.f, true, false);
}

void UDetectionGaugeWidget::SetAggroTarget(AActor* TargetActor)
{
	AggroTargetDebugInfo = FString::Printf(TEXT("대상=%s 클래스=%s 테이블=%s 행타입=%s 등록=%d"), *GetNameSafe(TargetActor), IsValid(TargetActor) ? *GetNameSafe(TargetActor->GetClass()) : TEXT("없음"), *GetNameSafe(JobDataTable), IsValid(JobDataTable) ? *GetNameSafe(JobDataTable->GetRowStruct()) : TEXT("없음"), TargetPortraitRows.Num());
	if (!IsValid(AggroTargetImage))
	{
		AggroTargetDebugInfo += TEXT(" 결과=AggroTargetImage 연결 실패");
		return;
	}

	UTexture2D* Icon = nullptr;
	FString Result = !IsValid(TargetActor) ? TEXT("대상 없음") : !IsValid(JobDataTable) ? TEXT("테이블 미지정") : TEXT("행 타입 불일치(FJobInfo 필요)");
	if (IsValid(TargetActor) && IsValid(JobDataTable) && JobDataTable->GetRowStruct() == FJobInfo::StaticStruct())
	{
		Result = TEXT("일치하는 BP 클래스 없음");
		UClass* MatchedClass = nullptr;
		FName MatchedRow = NAME_None;
		for (const auto& Entry : TargetPortraitRows)
		{
			UClass* TargetClass = Entry.Key.Get();
			if (IsValid(TargetClass) && TargetActor->IsA(TargetClass) && (!IsValid(MatchedClass) || TargetClass->IsChildOf(MatchedClass)))
			{
				MatchedClass = TargetClass;
				MatchedRow = Entry.Value;
			}
		}
		AggroTargetDebugInfo += FString::Printf(TEXT(" 매칭=%s 행=%s"), *GetNameSafe(MatchedClass), *MatchedRow.ToString());
		if (IsValid(MatchedClass) && MatchedRow.IsNone())
		{
			Result = TEXT("매칭 클래스의 행 이름 미지정");
		}
		if (!MatchedRow.IsNone())
		{
			const FJobInfo* JobRow = JobDataTable->FindRow<FJobInfo>(MatchedRow, TEXT("AggroTargetPortrait"), false);
			Icon = JobRow ? JobRow->Portrait.Get() : nullptr;
			Result = !JobRow ? TEXT("해당 행 없음") : !IsValid(Icon) ? TEXT("Portrait 미지정") : TEXT("Portrait 적용");
		}
	}
	AggroTargetDebugInfo += FString::Printf(TEXT(" Portrait=%s 결과=%s"), *GetNameSafe(Icon), *Result);

	// 없는 대상이나 미등록 이미지는 슬롯 크기를 유지하면서 비운다.
	AggroTargetImage->SetBrushFromTexture(IsValid(Icon) ? Icon : nullptr, false);
	AggroTargetImage->SetVisibility(IsValid(Icon) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UDetectionGaugeWidget::SetPerceptionGaugePercents(float SightPercent, float HearingPercent, bool bSightEnabled, bool bHearingEnabled)
{
	const float Sight01 = bSightEnabled ? FMath::Clamp(SightPercent / 100.f, 0.f, 1.f) : 0.f;
	const float Hearing01 = bHearingEnabled ? FMath::Clamp(HearingPercent / 100.f, 0.f, 1.f) : 0.f;
	if (IsValid(SightGaugeBar))
	{
		SightGaugeBar->SetPercent(Sight01);
		SightGaugeBar->SetVisibility(bSightEnabled && (!bHideWhenEmpty || Sight01 > 0.f) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IsValid(HearingGaugeBar))
	{
		HearingGaugeBar->SetPercent(Hearing01);
		HearingGaugeBar->SetVisibility(bHearingEnabled && (!bHideWhenEmpty || Hearing01 > 0.f) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// Hidden으로 슬롯 간격을 유지해 한쪽 게이지가 사라져도 다른 쪽의 위치가 변하지 않는다.
	const bool bShouldShow = (bSightEnabled || bHearingEnabled) && (!bHideWhenEmpty || Sight01 > 0.f || Hearing01 > 0.f);
	SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
