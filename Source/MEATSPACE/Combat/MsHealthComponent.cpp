#include "Combat/MsHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UMsHealthComponent::UMsHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMsHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	LastHealth = Health;

	// Only the server listens for damage - it is the only one allowed to change health.
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->OnTakeAnyDamage.AddDynamic(this, &UMsHealthComponent::HandleTakeAnyDamage);
		}
	}
}

void UMsHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMsHealthComponent, Health);
}

void UMsHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float DamageAmount,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return;
	}

	Health = FMath::Max(Health - DamageAmount, 0.0f);

	const float Delta = Health - LastHealth;
	LastHealth = Health;

	// OnRep does not fire on the server, so broadcast here as well.
	OnHealthChanged.Broadcast(Health, Delta);

	if (Health <= 0.0f)
	{
		OnDeath.Broadcast(GetOwner());
	}
}

void UMsHealthComponent::OnRep_Health()
{
	const float Delta = Health - LastHealth;
	LastHealth = Health;

	OnHealthChanged.Broadcast(Health, Delta);
}

void UMsHealthComponent::ResetHealth()
{
	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->HasAuthority())
		{
			return;
		}
	}

	Health = MaxHealth;
	LastHealth = Health;
	OnHealthChanged.Broadcast(Health, 0.0f);
}
