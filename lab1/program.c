/*
 * Jednostavni primjer za generiranje prostih brojeva korištenjem
   "The GNU Multiple Precision Arithmetic Library" (GMP)
 */

#include <stdio.h>
#include <time.h>

#include "slucajni_prosti_broj.h"

uint64_t ms[10] = { 0 };
int ulaz = 0, izlaz = 0, brojac = 0;

void stavi_u_ms(uint64_t broj)
{
	if(brojac < 10)
	{
		ms[ulaz] = broj;
		ulaz = (ulaz + 1) % 10;
		brojac++;
	}
}

uint64_t uzmi_iz_ms()
{
	if(brojac > 0)
	{
		uint64_t broj = ms[izlaz];
		ms[izlaz] = 0;
		izlaz = (izlaz + 1) % 10;
		brojac--;
		return broj;
	}
	return 0;
}

int prost(int x)
{
	for(int i = 2; i * i <= x; i++)
	{
		if(x % i == 0)
			return 0;
	}
	return 1;
}

uint64_t generiraj_dobar_broj(int b)
{
	int i = 0, MASKA = 0x03ff, x;
	uint64_t broj = 0, odabrano = 0;
	struct gmp_pomocno p;

	inicijaliziraj_generator (&p, b);

	//pogledaj fju daj_novi
	while((odabrano == 0) && (i < 10))
	{
		broj = daj_novi_slucajan_prosti_broj (&p);
		//printf("Novi: %" PRIu64 "\n", broj);
		for(int j = 0; j < 54; j++)
		{
			x = (broj >> j) & MASKA;
			//ako je x prost, onda je odabrano = broj
			if(prost(x))
			{
				odabrano = broj;
				break;
			}
		}
		i += 1;
	}
	if(i == 10)
		odabrano = broj;

	obrisi_generator (&p);

	return odabrano;
}

int main(int argc, char *argv[])
{
	time_t start, end;
	int broj_ispisa = 0, broj_odabranih = 0;

	time(&start);

	while(broj_ispisa < 10)
	{
		//generiramo max 3 broja u sekundi
		if(broj_odabranih < 3)
		{
			uint64_t broj = generiraj_dobar_broj(broj_odabranih * (broj_ispisa + 10));
			//stavi broj u međuspremnik
			stavi_u_ms(broj);
			broj_odabranih++;
		}
		time(&end);
		if(start != end)
		{
			//prošla je sekunda -> ispis
			//uzmi broj iz međuspremnika i ispiši ga
			uint64_t ispis = uzmi_iz_ms();
			printf("%" PRIu64 "\n", ispis);
			broj_ispisa++;
			time(&start);
			broj_odabranih = 0;
		}
	}
	return 0;
}

/*
  prevođenje:
  - ručno: gcc program.c slucajni_prosti_broj.c -lgmp -lm -o program
  - preko Makefile-a: make
  pokretanje:
  - ./program
  - ili: make pokreni
  nepotrebne datoteke (.o, .d, program) NE stavljati u repozitorij
  - obrisati ih ručno ili s make obrisi
*/