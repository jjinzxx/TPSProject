// Fill out your copyright notice in the Description page of Project Settings.


#include "TPSPlayer.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"


// Sets default values
ATPSPlayer::ATPSPlayer()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// TPS 카메라를 SpringArm 컴포넌트에 부착
	springArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	springArmComp->SetupAttachment(RootComponent); // 계층 구조상 캡슐 컴포넌트가 ROOT이며 스프링암을 자식으로 배치
	springArmComp->SetRelativeLocation(FVector(0.0f, 70.0f, 90.0f));
	springArmComp->TargetArmLength = 400.0f; // 가까운 3인칭을 원하면 이걸 조절(원하는대로)
	
	// 카메라
	cameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	cameraComp->SetupAttachment(springArmComp);
	
	// (참고) C++에서 BP의 옵션을 직접 수정하는 경우
	// 아래처럼 해당 옵션 변수들을 직접 코드로 제어 가능
	// springArmComp->bUsePawnControlRotation = true;
	// cameraComp->bUsePawnControlRotation = false;
	// bUseControllerRotationYaw = true;
		
}

// Called when the game starts or when spawned
void ATPSPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	// Enhanced Input 시스템이 IMC_TPS를 사용하도록 설정
	auto pc = Cast<APlayerController>(Controller);
	if (pc)
	{
		auto subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(pc->GetLocalPlayer());
		if (subsystem)
		{
			subsystem->AddMappingContext(imc_TPS, 0);
		}
	}
}

// Called every frame
void ATPSPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 플레이어 이동 처리
	// P 결과 위치 = P0 초기 위치 + v 속도 * t 시간
	direction = FTransform(GetControlRotation()).TransformFVector4(direction);
	FVector P0 = GetActorLocation();
	FVector vt = direction * walkSpeed * DeltaTime;
	FVector P = P0 + vt;
	SetActorLocation(P);
	direction = FVector::ZeroVector;
}

// Called to bind functionality to input
void ATPSPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto PlayerInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	if (PlayerInput)
	{
		PlayerInput->BindAction(ia_LookUp, ETriggerEvent::Triggered, this, &ATPSPlayer::LookUp);
		PlayerInput->BindAction(ia_Turn, ETriggerEvent::Triggered, this, &ATPSPlayer::Turn);
		PlayerInput->BindAction(ia_Move, ETriggerEvent::Triggered, this, &ATPSPlayer::Move);
	}
}

// 좌우 회전 입력에 따른 콜백 함수
void ATPSPlayer::Turn(const FInputActionValue& inputValue)
{
	float value = inputValue.Get<float>();
	AddControllerYawInput(value); // YAW(Z 축) 회전
}

// 상하 회전 입력에 따른 콜백 함수
void ATPSPlayer::LookUp(const FInputActionValue& inputValue)
{
	float value = inputValue.Get<float>();
	AddControllerPitchInput(value); // PITCH(Y 축) 회전
}

// 전후좌우 이동 입력에 따른 콜백 함수 구현
void ATPSPlayer::Move(const FInputActionValue& inputValue)
{
	FVector2D value = inputValue.Get<FVector2D>();

	direction.X = value.X;
	direction.Y = value.Y;
}