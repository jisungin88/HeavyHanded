// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

USTRUCT(BlueprintType)
struct FAbilityInputBinding
{
	GENERATED_BODY()

public:
	// 에디터에서 지정할 입력 액션 (예: IA_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TObjectPtr<class UInputAction> InputAction;

	// 그 입력에 대응할 어빌리티 클래스 (예: GAB_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TSubclassOf<class UGameplayAbility> AbilityClass;
};

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;

UCLASS()
class HEAVYHANDED_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

	// IAbilitySystemInterface 필수 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// --- 카메라 및 시점 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// --- Enhanced Input Actions ---
	// 에디터에서 블루프린트로 생성한 Input Action 에셋을 할당할 수 있습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Turn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LookUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditAnywhere, Category = "Input") 
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// --- 입력 처리 함수 (Enhanced Input 방식) ---
	// 2차원 축 입력 (WASD 이동 등)
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	// 마우스 회전 입력 등
	void Turn(const FInputActionValue& Value);
	void LookUp(const FInputActionValue& Value);


protected:
	// 클라이언트가 입력했을 때 서버로 요청을 보내는 함수
	UFUNCTION(Server, Reliable)
	void Server_ApplyGameplayEffect(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 2. 언리얼 빌드 시스템이 찾는 내부 구현체 함수 (_Implementation 필수 붙이기)
	//void Server_ApplyGameplayEffect_Implementation(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 기존 입력 함수 수정
	virtual void StartCrouch(const FInputActionValue& Value);
	virtual void StopCrouch(const FInputActionValue& Value);
	virtual void StartSprint(const FInputActionValue& Value);
	virtual void StopSprint(const FInputActionValue& Value);

protected:
	// MovementSpeed 속성이 변할 때 호출될 콜백 함수
	virtual void OnMovementSpeedChanged(const struct FOnAttributeChangeData& Data);

	// ASC 초기화 시 속성 바인딩을 수행할 함수
	void BindAttributeDelegates();

	// 입력에 따라 적용할 달리기/앉기 Gameplay Effect 클래스 (블루프린트에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> SprintGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> CrouchGameplayEffectClass;

protected:
	void ApplyGameplayEffectToSelf(TSubclassOf<class UGameplayEffect> EffectClass);
	void RemoveGameplayEffectFromSelf(TSubclassOf<class UGameplayEffect> EffectClass);

protected:
	// ★ [통합됨] 에디터 디테일 패널에서 입력과 스킬을 1:1로 매핑하는 리스트 (DefaultAbilities 삭제됨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<FAbilityInputBinding> AbilityInputBindings;

	// 인프라 입력 감지 시 실행될 콜백 함수
	//void AbilityInputPressed(TSubclassOf<class UGameplayAbility> AbilityClass);
	void AbilityInputPressed(int32 InputID);
	void AbilityInputReleased(int32 InputID);

protected:
	// 현재 손에 들고 있는 액터.
	// 부착 자체는 액터 리플리케이션이 옮겨주지만, 물리/콜리전 토글은 복제되지
	// 않는 로컬 호출이라 OnRep 에서 클라이언트도 같은 상태를 만들어줘야 한다.
	UPROPERTY(ReplicatedUsing = OnRep_HeldActor, VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> HeldActor = nullptr;

	UFUNCTION()
	virtual void OnRep_HeldActor(AActor* PreviousHeldActor);

public:
	// HeldActor를 반환하는 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetHeldActor() const { return HeldActor; }

	// HeldActor를 설정하는 함수. 서버 권한에서만 동작하며 물리/콜리전
	// 게이팅까지 함께 처리한다. 부착/분리는 호출하는 쪽 책임이다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetHeldActor(AActor* NewHeldActor);

	// 운반 중에는 물리와 콜리전을 끄고, 놓을 때 되돌린다.
	// 서버 로직과 OnRep 이 같은 경로를 쓰도록 여기 한 곳에만 둔다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ApplyCarryPhysicsState(AActor* Target, bool bCarried);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
