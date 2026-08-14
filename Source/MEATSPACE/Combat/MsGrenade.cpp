#include "Combat/MsGrenade.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMsGrenade::AMsGrenade()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(14.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Must not block weapon traces, or the grenade in flight would eat your own gunfire.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.28f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 0.0f;   // set by the thrower
	Movement->MaxSpeed = 0.0f;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.35f;
	Movement->Friction = 0.4f;
	Movement->ProjectileGravityScale = 1.6f;
	Movement->UpdatedComponent = Collision;
}

void AMsGrenade::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentHit.AddDynamic(this, &AMsGrenade::HandleHit);

	// The fuse is authoritative - clients just watch the grenade fly.
	if (HasAuthority() && !bExplodeOnImpact)
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &AMsGrenade::Explode, FMath::Max(FuseTime, 0.01f), false);
	}
}

void AMsGrenade::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExplodeOnImpact && HasAuthority())
	{
		Explode();
	}
}

void AMsGrenade::Explode()
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;

	const FVector Origin = GetActorLocation();

	TArray<AActor*> IgnoredActors;
	if (!bDamageThrower && GetInstigator())
	{
		// This weapon exists to relieve pressure. Blowing yourself up while surrounded would
		// invert that entirely.
		IgnoredActors.Add(GetInstigator());
	}

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		Damage,
		Damage * MinDamageFraction,
		Origin,
		InnerRadius,
		OuterRadius,
		/*DamageFalloff=*/1.0f,
		/*DamageTypeClass=*/nullptr,
		IgnoredActors,
		this,
		GetInstigatorController());

	MulticastExplodeFX(Origin);

	// Give the multicast a frame to leave before the actor goes.
	SetLifeSpan(0.1f);
}

void AMsGrenade::MulticastExplodeFX_Implementation(const FVector_NetQuantize& Location)
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugBlast)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin(Location);

	DrawDebugSphere(World, Origin, InnerRadius, 16, FColor(120, 200, 255), false, 0.5f, 0, 2.0f);
	DrawDebugSphere(World, Origin, OuterRadius, 20, FColor(60, 120, 220), false, 0.5f, 0, 1.5f);

	// Lightning arcs to whatever is in range - placeholder for a real electrical VFX, but it
	// already does the important job of showing WHO got caught in the blast.
	if (MaxVisualArcs > 0)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MsGrenadeArcs), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(this);

		World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(OuterRadius), Params);

		int32 ArcsDrawn = 0;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ArcsDrawn >= MaxVisualArcs)
			{
				break;
			}

			const AActor* HitActor = Overlap.GetActor();
			if (!HitActor || HitActor == this || HitActor == GetInstigator())
			{
				continue;
			}

			DrawDebugLine(World, Origin, HitActor->GetActorLocation(),
				FColor(160, 220, 255), false, 0.35f, 0, 2.5f);
			++ArcsDrawn;
		}
	}
#endif
}
