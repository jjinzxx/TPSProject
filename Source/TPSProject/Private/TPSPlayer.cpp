// Fill out your copyright notice in the Description page of Project Settings.


#include "TPSPlayer.h"

#include "AssetDefinitionAssetInfo.h"
#include "Bullet.h"
#include "EnemyFSM.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
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
	
	// 총 스켈레탈메시 컴포넌트 등록
	gunMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GunMeshComponent"));
	// 캐릭터 메시 컴포넌트(GetMesh())
	gunMeshComp->SetupAttachment(GetMesh());
	// LineTrace가 총에 막히지 않도록 충돌 해결
	gunMeshComp->SetCollisionEnabled((ECollisionEnabled::NoCollision));
	// 스켈레탈 메시 데이터 동적로드
	ConstructorHelpers::FObjectFinder<USkeletalMesh> TempGunMesh(TEXT("/Script/Engine.SkeletalMesh'/Game/Weapons/GrenadeLauncher/Meshes/SKM_GrenadeLauncher.SKM_GrenadeLauncher'"));
	if (TempGunMesh.Succeeded())
	{
		// 해당 경로의 스켈레탈 메시를 찾았다면, 메시 할당 + 임시 위치 보정
		gunMeshComp->SetSkeletalMesh(TempGunMesh.Object);
		gunMeshComp->SetRelativeLocation(FVector(-14.0f, 52.0f, 120.0f));
	}
	
	// 스나이퍼총 스태틱 메시 컴포넌트 등록
	sniperMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sniper StaticMeshComponent"));
	sniperMeshComp->SetupAttachment(GetMesh());
	// LineTrace가 총에 막히지 않도록 충돌 해결
	sniperMeshComp->SetCollisionEnabled((ECollisionEnabled::NoCollision));
	ConstructorHelpers::FObjectFinder<UStaticMesh> TempSniperGunMesh(TEXT("/Script/Engine.StaticMesh'/Game/Weapons/sniper/Meshes/sniper.sniper'"));
	if (TempSniperGunMesh.Succeeded())
	{
		sniperMeshComp->SetStaticMesh(TempSniperGunMesh.Object);
		sniperMeshComp->SetRelativeLocation(FVector(-14.0f, 52.0f, 120.0f));
		//sniperMeshComp->SetRelativeScale3D(FVector(0.8f));
	}
	
	// 시작 시 기본 무기로 스나이퍼건을(유탄총을 숨김)
	bUsingGrenadeGun = false;
	sniperMeshComp->SetVisibility(true);
	gunMeshComp->SetVisibility(false);
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
	
	// 스나이퍼 UI 위젯 인스턴스 생성(화면에 보이기 위해서는 AddToViewport() 호출 시 등장)
	sniperUI = CreateWidget(GetWorld(), sniperUIFactory);
	// 일반 조준 크로스헤어 UI 위젯 인스턴스 생성 -> AddToViewport()로 호출
	crosshairUI = CreateWidget(GetWorld(), crosshairUIFactory);
	if (crosshairUI)
	{
		crosshairUI->AddToViewport();
	}
}

// Called every frame
void ATPSPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 플레이어 이동 처리
	//GetControlRotarion() 함수는 좌우(YAW) 말고도 상하 (PITCH)까지 포함된 카메라 전체 회전
	// PITCH 움직임에 X축을 기울이는 성질이 있어서 카메라가 아래를 향하는 상태에서 W를 누르면
	// "앞으로 가는 입력" + "아래로 박는 입력"으로 섞임 -> 이 때 수평 속도 cos(PITCH)가 줄어든다.
	
	// 컨트롤러의 현재 회전 값들(YAW, PITCH, ROLL) 값을 가져옴
	FRotator controlRot = GetControlRotation();
	// PITCH 자체를 0으로 - 기우는 현상 차단
	controlRot.Pitch = 0.0f;
	// ROLL도 0으로 고정 - 기우는 현상 차단
	controlRot.Roll = 0.0f;
	
	direction = FRotationMatrix(controlRot).TransformVector(direction);
	
	// 언리얼엔진에서 제공하는 위 등속 운동을 구현한 함수
	AddMovementInput(direction);
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
		PlayerInput->BindAction(ia_Jump, ETriggerEvent::Started, this, &ATPSPlayer::InputJump);
		PlayerInput->BindAction(ia_Fire, ETriggerEvent::Started, this, &ATPSPlayer::InputFire);
		PlayerInput->BindAction(ia_GrenadeGun, ETriggerEvent::Started, this, &ATPSPlayer::ChangeToGrenadeGun);
		PlayerInput->BindAction(ia_SniperGun, ETriggerEvent::Started, this, &ATPSPlayer::ChangeToSniperGun);
		PlayerInput->BindAction(ia_SniperZoom, ETriggerEvent::Started, this, &ATPSPlayer::SniperZoom);
		// PlayerInput->BindAction(ia_SniperZoom, ETriggerEvent::Completed, this, &ATPSPlayer::SniperZoom); // 토글방식 사용 시 주석
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

// 점프 입력에 따른 콜백 함수 구현
void ATPSPlayer::InputJump(const FInputActionValue& inputValue)
{
	Jump(); // ACharacter 클래스가 제공하는 기본 점프 함수 호출
}

void ATPSPlayer::InputFire(const FInputActionValue& inputValue)
{
	if (bUsingGrenadeGun)
	{
		// 유탄총을 사용하는 경우 
		// 총 스켈레탈 메시에 FirePosition 이란 이름의 소켓의 월드 트랜스폼(위치/회전)을 가져옴
		//FTransform firePosition = gunMeshComp->GetSocketTransform(TEXT("FirePosition"));
		FVector spawnLocation = gunMeshComp->GetSocketLocation(TEXT("FirePosition"));
		FRotator spawnRotation = cameraComp->GetComponentRotation();
		
		
		// 위 위치/회전으로 BulletFactory가 BP_Bullet 인스턴스를 월드에 스폰
		//GetWorld()->SpawnActor<ABullet>(bulletFactory, firePosition);
		GetWorld()->SpawnActor<ABullet>(bulletFactory, spawnLocation, spawnRotation);
	}
	else
	{
		// 스나이퍼를 사용하는 경우
		// 라인의 시작/종료 위치 설정 - 카메라부터 카메라 정명 50m 까지
		FVector startPosition = cameraComp->GetComponentLocation();
		FVector endPosition = startPosition + cameraComp->GetForwardVector() * 5000.0f; // 50m
		
		// 충돌 결과 저장, 자기 자신은 충돌 검사에서 제외
		FHitResult hitResult;
		FCollisionQueryParams params;
		params.AddIgnoredActor(this);
	
		// LineTraceSingleByChannal(결과 그릇, 시작 위치, 종료 위치, 트레이스 채널, 충돌 옵션)
		// ECC_Visibility 채널로 라인트레이스를 실행하고 -> 처음 부딪힌 액터 하나만 검출
		bool bHit = GetWorld()->LineTraceSingleByChannel(hitResult, startPosition, endPosition, ECC_Visibility, params);
		
		// [Debug] LineTrace 경로 시각화 - 빨강(미충돌) / 초록(충돌)
		DrawDebugLine(GetWorld(), startPosition, endPosition, bHit ? FColor::Green : FColor::Red, false, 1.f, 0 , .2f);
		if (bHit)
		{
			// [Debug] 충돌 정보 
			UE_LOG(LogTemp, Warning, TEXT("HIT: Actor=%s, Component=%s, Distance=%.1f, ImpactPoint=%s"),
				hitResult.GetActor() ? *hitResult.GetActor()->GetName() : TEXT("None"),
				hitResult.GetComponent() ? *hitResult.GetComponent()->GetName() : TEXT("None"),
				hitResult.Distance,
				*hitResult.ImpactPoint.ToString()
			);
			
			// [Debug] 타격 위치 시각화
			DrawDebugSphere(GetWorld(), hitResult.ImpactPoint, 20.f, 12, FColor::Yellow, false, 2.f);
			
			// 타격 위치에 Niagara 이펙트 스폰
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), bulletEffectFactory, hitResult.ImpactPoint);
			
			// 적 피격 처리 - FSM 컴포넌트의 OnDamageProcess() 호출
			auto enemy = hitResult.GetActor()->GetDefaultSubobjectByName(TEXT("FSM"));
			if (enemy)
			{
				auto enemyFSM = Cast<UEnemyFSM>(enemy);
				enemyFSM->OnDamageProcess();
			}
			
			// 타격 물체에 물리 엔진 적용
			UPrimitiveComponent* hitComp = hitResult.GetComponent();
			if (hitComp && hitComp->IsSimulatingPhysics())
			{
				// 1. 조준 방향 - 시작점에서 종료점 방향
				FVector dir = (endPosition - startPosition).GetSafeNormal();
				// 2. 날려보낼 힘 (F = MA 방향 * 질량 * 가속도)
				FVector force = dir * hitComp->GetMass() * 20000.f;
				// 3. 타격된 지점에 힘을 적용
				// AddForce(F) - 무게 중심에 힘을 적용 -> 회전없이 평행이동
				// AddForceAtlocation(F, pos) - 특정 위치에 힘-> 회전(토크)가 발생한다. ex: 모서리 맞으면 빙글빙글 돌면서 뒤로 밀림
				hitComp->AddForceAtLocation(force, hitResult.ImpactPoint);
			}
		}
	
	}
	
	
}

void ATPSPlayer::ChangeToGrenadeGun(const struct FInputActionValue& inputValue)
{
	// 사용 중 플래그를 유탄총으로 변경
	bUsingGrenadeGun = true;
	// 스나이퍼 숨기고 / 유탄총 보이게
	sniperMeshComp->SetVisibility(false);
	gunMeshComp->SetVisibility(true);
}
void ATPSPlayer::ChangeToSniperGun(const struct FInputActionValue& inputValue)
{
	// 사용 중 플래그를 스나이퍼총으로 변경
	bUsingGrenadeGun = false;
	// 스나이퍼 보이고 / 유탄총 숨기기
	sniperMeshComp->SetVisibility(true);
	gunMeshComp->SetVisibility(false);
}

void ATPSPlayer::SniperZoom()
{
	// 스나이퍼 총이 아닐 때는 동작하지 않음
	if (bUsingGrenadeGun)
	{
		return;
	}
	
	if (sniperUI == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("sniperUI가 생성되지 않았습니다! 조준을 할 수 없습니다."));
		return; 
	}
	
	// 키 누름 - 줌 모드에 진입
	if (bSniperZoom == false)
	{
		bSniperZoom = true;
		sniperUI->AddToViewport(); // 조준경 UI 화면에 나타남
		cameraComp->SetFieldOfView(45.f); // FOV 시야각을 좁혀서 줌인 효과
		if (crosshairUI)
		{
			crosshairUI->RemoveFromParent();
		}
	}
	// 키 해제 - 줌 모드 해제
	else
	{
		bSniperZoom = false;
		sniperUI->RemoveFromParent(); // 조준경 UI 화면에 나타남
		cameraComp->SetFieldOfView(90.f); // FOV 시야각을 좁혀서 줌인 효과
		if (crosshairUI)
		{
			crosshairUI->AddToViewport();
		}
	}
}
