#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageVignetteWidget.generated.h"

class UImage;
class UTexture2D;
class UWidgetAnimation;

/**
 * 피격 화면 이펙트 (기획서 4장 다운 조건 / 8장 UI).
 *
 * [체력바를 만들지 않는다] 이 게임에는 연속적인 체력이 없다. 다운 조건이
 *   "경비견 3회 누적 / 무장 경비 접촉 즉시 / 낙하" 라 중간값이 존재하지 않는다.
 *   그래서 바 대신 맞은 순간 화면에 핏자국을 던진다.
 *
 * [핏자국은 사라지고 카운트는 남는다] 핏자국은 HitFadeSeconds 뒤에 걷힌다.
 *   시야를 계속 가리면 잠입 게임에서 플레이가 불가능해지기 때문이다.
 *   반면 몇 대 맞았는지는 다운 판정에 쓰이므로 그대로 유지된다 —
 *   즉 화면은 "방금 맞았다" 를 말하고, 카운트는 "얼마나 위험한가" 를 말한다.
 *   마지막 단계(= 다운)만 예외로 남는다. bKeepFinalHitVisible 참조.
 *
 *   [따라오는 것] 핏자국이 걷히고 나면 지금 1스택인지 2스택인지 화면에 남지 않는다.
 *   기획서 8장의 피격 카운트 표시(●●○)가 그래서 필요해진다. 아직 미착수.
 *
 * [왜 UMG 인가] 포스트프로세스가 아니라 위젯이다.
 *   ① Overlay_NoiseDir(소음 방향)과 같은 화면 가장자리를 쓰므로 레이어 순서를
 *      한 곳에서 관리해야 한다. ② 카메라에 붙지 않아 다운 후 관전 카메라로
 *      넘어가도 화면에 딸려가지 않는다. ③ 페이드 곡선을 위젯 애니메이션으로
 *      디자이너가 직접 만진다.
 *   3차 피격의 블러 · 채도 하락은 UMG 가 3D 화면을 건드릴 수 없으므로
 *   포스트프로세스로 따로 얹는다. 그건 이 클래스 밖이다.
 *
 * [트리거는 아직 없다] 경비의 접촉 판정도, State.Downed 를 부여하는 쪽도 아직 없다.
 *   그래서 지금은 hh.UI.Damage 치트로만 켠다. 나중에 GameplayCue 나
 *   HitCount 어트리뷰트 구독이 SetHitCount() 를 부르게 되면 이 클래스는 안 바뀐다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UDamageVignetteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 피격 횟수를 설정한다. 카운트는 여기서만 바뀐다.
	 *
	 * 늘어난 만큼 핏자국이 새로 뜨고, 각자 제 시간이 되면 알아서 걷힌다.
	 * 0 이면 전부 지우고 타이머도 취소한다(부활 · 리셋).
	 * 배치된 이미지 수를 넘는 값은 잘린다 — 무장 경비의 즉사도 최대치일 뿐이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Damage")
	void SetHitCount(int32 NewCount);

	/** 한 대 맞았다. 경비견 접촉이 부르게 될 자리다 */
	UFUNCTION(BlueprintCallable, Category = "UI|Damage")
	void AddHit() { SetHitCount(HitCount + 1); }

	/** 최대치로 올린다. 무장 경비 접촉처럼 카운트를 거치지 않는 즉시 다운용 */
	UFUNCTION(BlueprintCallable, Category = "UI|Damage")
	void ApplyMaxHit() { SetHitCount(GetMaxHitCount()); }

	/** 전부 지운다. 동료가 복구를 끝냈을 때 */
	UFUNCTION(BlueprintCallable, Category = "UI|Damage")
	void ClearHits() { SetHitCount(0); }

	UFUNCTION(BlueprintPure, Category = "UI|Damage")
	int32 GetHitCount() const { return HitCount; }

	/** WBP 가 실제로 배치한 핏자국 이미지 수. 3개를 다 두지 않아도 동작한다 */
	UFUNCTION(BlueprintPure, Category = "UI|Damage")
	int32 GetMaxHitCount() const;

	/**
	 * 0~1 강도. 카운트를 최대치로 나눈 값이다.
	 *
	 * 카메라 셰이크 Scale · 포스트프로세스 블렌드처럼 이 위젯 밖의 연출도
	 * 같은 값 하나를 받아 쓰라고 열어 둔다 — 단계마다 에셋을 나누지 않기 위해서다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Damage")
	float GetIntensity() const;

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// 전부 Optional 이다. 하나만 두고 시작해도 되고, 나중에 늘려도 C++ 은 안 바뀐다.
	// 앵커는 풀스크린, Visibility 는 반드시 Hit Test Invisible 로 둘 것 —
	// 화면 전체를 덮는 이미지가 클릭을 먹으면 결과 화면 버튼이 눌리지 않는다.

	/** 1차 피격 핏자국 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Blood_1;

	/** 2차 피격 핏자국 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Blood_2;

	/** 3차 피격 핏자국. 다운 트리거 단계 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Blood_3;

	// ── 위젯 애니메이션 ──
	//
	// 없으면 알파를 즉시 켜고 끈다. 연출이 없을 뿐 기능은 그대로 돈다.
	// 걷힐 때는 같은 애니메이션을 거꾸로 재생한다 — 사라지는 연출을 따로
	// 만들면 등장과 짝이 안 맞을 때 티가 난다.

	/** 1차 등장. 없으면 즉시 표시 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Splat_1;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Splat_2;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Splat_3;

	/** 부활 · 리셋으로 한꺼번에 사라지는 연출. 없으면 즉시 숨긴다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Damage", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ClearAll;

	/**
	 * 뽑아 쓸 핏자국 마스크 (/Game/BloodScreen/ScreenDamage_Blood/Textures).
	 *
	 * [50장을 다 넣지 말 것] 배열에 담긴 것은 전부 메모리에 올라온다.
	 *   원본 팩이 13MB 다. 모양이 서로 다른 5~10장이면 반복이 눈에 띄지 않는다.
	 *   가장자리에 몰린 것과 화면 가운데를 덮는 것을 섞어 둘 것 —
	 *   3차에서 가운데를 덮는 것이 나와야 "위험하다" 가 읽힌다.
	 *
	 * 비어 있으면 핏자국 없이 카운트만 동작한다(로그로 알린다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Vignette")
	TArray<TObjectPtr<UTexture2D>> DamageTextures;

	/**
	 * 핏자국 하나가 화면에 남아 있는 시간(초). 0 이면 걷히지 않는다.
	 *
	 * [기획서 근거가 없는 값이다] 기획서 8장에는 소음 피드백만 있고 피격 잔상
	 *   지속 시간이라는 항목이 없다. 플레이테스트로 정할 값이다.
	 *   너무 짧으면 맞았는지 모르고, 너무 길면 시야를 가려 잠입이 불가능해진다.
	 *
	 * 타이머는 핏자국마다 따로 돈다 — 연달아 맞으면 걷히는 시점도 따로다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Vignette", meta = (ClampMin = "0.0", Units = "s"))
	float HitFadeSeconds = 10.f;

	/**
	 * 마지막 단계(= 다운)의 핏자국은 걷지 않는다.
	 *
	 * 다운된 순간에 화면이 깨끗해지면 "일어난 것" 처럼 읽힌다. 그 자리는
	 * 동료 복구를 기다리는 구간이라 화면이 계속 위험해 보여야 맞는다.
	 * 지우는 것은 부활(ClearHits) 뿐이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Vignette")
	bool bKeepFinalHitVisible = true;

	/**
	 * 걷힐 때의 재생 배속. 등장 애니메이션을 거꾸로 돌릴 때 곱한다.
	 *
	 * [1 이 아닌 이유] 피는 순간적으로 튀고 천천히 마른다. 등장과 소멸을 같은
	 *   속도로 두면 사라질 때 "지워진다" 처럼 보인다. 0.3 이면 0.3초짜리 등장이
	 *   1초에 걸쳐 걷힌다. 사라지는 애니메이션을 따로 만들지 않는 이유는
	 *   둘이 어긋났을 때 한쪽만 고쳐져 티가 나기 때문이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Vignette", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float FadeSpeedScale = 0.3f;

	// ── BP 연출 훅 ──
	//
	// 표시는 C++ 이 이미 끝냈다. 여기는 사운드 · 카메라 셰이크 자리다.
	// 훅은 클라에서도 돌기 때문에 게임 상태를 바꾸지 말 것.

	/**
	 * 피격이 화면에 적용됐다.
	 *
	 * @param NewCount  누적 피격 횟수
	 * @param Intensity 0~1 강도. StartCameraShake 의 Scale 로 그대로 넘기면 된다
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Damage")
	void OnHitApplied(int32 NewCount, float Intensity);

	/** 핏자국 하나가 시간이 지나 걷혔다. 카운트는 그대로다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Damage")
	void OnHitFaded(int32 SlotIndex);

	/** 전부 지워졌다(부활 · 리셋). 카운트도 0 이다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Damage")
	void OnCleared();

private:
	/** 인덱스(0부터)에 해당하는 핏자국 이미지. 없으면 null */
	UImage* GetSlotImage(int32 Index) const;

	/** 인덱스에 해당하는 등장 애니메이션. 없으면 null */
	UWidgetAnimation* GetSlotAnim(int32 Index) const;

	/** 슬롯 하나를 켠다. 무작위 마스크를 꽂고 애니를 재생한 뒤 걷힐 시각을 예약한다 */
	void ShowSlot(int32 Index);

	/** 슬롯 하나를 걷는다. 카운트는 건드리지 않는다 */
	void FadeSlot(int32 Index);

	/** 예약된 걷힘을 전부 취소한다 */
	void CancelFadeTimers();

	/** 배열에서 마스크 하나를 고른다. 직전에 쓴 것은 되도록 피한다 */
	UTexture2D* PickTexture();

	/** 슬롯 최대 개수. 이미지 · 애니메이션 · 타이머 배열의 크기다 */
	static constexpr int32 MaxSlots = 3;

	/** 현재 누적 피격 횟수. 화면에 보이는 핏자국 수와 다를 수 있다 */
	int32 HitCount = 0;

	/** 직전에 뽑은 마스크의 인덱스. 같은 그림이 연달아 나오는 것을 막는다 */
	int32 LastTextureIndex = INDEX_NONE;

	/** 슬롯별 걷힘 예약. 연달아 맞으면 각자 따로 돈다 */
	FTimerHandle FadeHandles[MaxSlots];
};
