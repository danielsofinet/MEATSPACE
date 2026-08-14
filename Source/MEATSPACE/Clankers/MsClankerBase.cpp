#include "Clankers/MsClankerBase.h"

#include "Combat/MsHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AMsClankerBase::AMsClankerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(45.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	// Clankers must block the Visibility channel or weapon traces pass straight through them.
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(CollisionSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CollisionSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	HealthComponent = CreateDefaultSubobject<UMsHealthComponent>(TEXT("Health"));
}

void AMsClankerBase::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AMsClankerBase::HandleDeath);
	}
}

APawn* AMsClankerBase::FindTargetPlayer() const
{
	// Single target for now. When co-op lands this becomes "nearest living player".
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return nullptr;
	}

	if (AggroRange > 0.0f)
	{
		const float DistSq = FVector::DistSquared(Player->GetActorLocation(), GetActorLocation());
		if (DistSq > FMath::Square(AggroRange))
		{
			return nullptr;
		}
	}

	return Player;
}

FVector AMsClankerBase::ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer)
{
	if (!TargetPlayer)
	{
		return FVector::ZeroVector;
	}

	FVector ToPlayer = TargetPlayer->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;

	return ToPlayer.GetSafeNormal();
}

void AMsClankerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Steering is a server decision. Clients just receive the resulting movement.
	if (!HasAuthority())
	{
		return;
	}

	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	const APawn* TargetPlayer = FindTargetPlayer();

	FVector Flat = TargetPlayer
		? (TargetPlayer->GetActorLocation() - GetActorLocation())
		: FVector::ZeroVector;
	Flat.Z = 0.0f;
	DistanceToTarget = Flat.Size();

	TryContactDamage(const_cast<APawn*>(TargetPlayer));

	FVector DesiredDirection = ComputeMoveDirection(DeltaSeconds, TargetPlayer);

	// Back off once we are on top of the player so clankers crowd around rather than shove.
	if (TargetPlayer && DistanceToTarget < StopDistance)
	{
		DesiredDirection = FVector::ZeroVector;
	}

	FVector DesiredVelocity = DesiredDirection * MoveSpeed;

	// Ground-walkers never steer vertically - height is the ground trace's job.
	if (bSnapToGround)
	{
		DesiredVelocity.Z = 0.0f;
	}

	// Smooth toward the desired velocity instead of snapping, so turns read as momentum.
	CurrentVelocity = FMath::VInterpTo(CurrentVelocity, DesiredVelocity, DeltaSeconds, TurnResponsiveness);

	if (bSnapToGround)
	{
		CurrentVelocity.Z = 0.0f;
	}

	if (!CurrentVelocity.IsNearlyZero())
	{
		FHitResult Hit;
		AddActorWorldOffset(CurrentVelocity * DeltaSeconds, /*bSweep=*/true, &Hit);

		// Slide along whatever we hit rather than sticking to it.
		if (Hit.bBlockingHit)
		{
			const FVector Slide = FVector::VectorPlaneProject(CurrentVelocity, Hit.Normal);
			AddActorWorldOffset(Slide * DeltaSeconds, /*bSweep=*/true);
		}

		if (bFaceMovementDirection)
		{
			FVector Facing = CurrentVelocity;
			Facing.Z = 0.0f;
			if (!Facing.IsNearlyZero())
			{
				const FRotator Target = Facing.Rotation();
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, TurnResponsiveness));
			}
		}
	}

	// Ride the surface. Done after moving so slopes and steps are followed rather than fought.
	if (bSnapToGround)
	{
		SnapToGround();
	}
}

void AMsClankerBase::TryContactDamage(APawn* TargetPlayer)
{
	UWorld* World = GetWorld();
	if (!World || !TargetPlayer || ContactDamage <= 0.0f)
	{
		return;
	}

	if (DistanceToTarget > StopDistance + ContactReach)
	{
		return;
	}

	// Vertical check as well as horizontal, or a hovering clanker would claw at someone far
	// below it just because they line up on the ground plane.
	const float HeightDifference = FMath::Abs(TargetPlayer->GetActorLocation().Z - GetActorLocation().Z);
	if (HeightDifference > ContactHeightTolerance)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < LastContactTime + ContactInterval)
	{
		return;
	}
	LastContactTime = Now;

	// No instigator controller - clankers have no AIController, they steer themselves.
	UGameplayStatics::ApplyDamage(TargetPlayer, ContactDamage, nullptr, this, nullptr);
}

void AMsClankerBase::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Location = GetActorLocation();

	// Start above the actor so we still find the floor after walking up a small step.
	const FVector Start = Location + FVector(0.0f, 0.0f, GroundTraceDistance * 0.5f);
	const FVector End = Location - FVector(0.0f, 0.0f, GroundTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsClankerGround), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		FVector Snapped = Location;
		Snapped.Z = Hit.ImpactPoint.Z + GroundOffset;
		SetActorLocation(Snapped, /*bSweep=*/false);
	}
}

void AMsClankerBase::HandleDeath(AActor* DeadActor)
{
	// Placeholder death. Real version gets a death animation, ragdoll or explosion, plus
	// whatever drop/score hook the district progression ends up needing.
	SetActorEnableCollision(false);
	SetLifeSpan(0.15f);
}
