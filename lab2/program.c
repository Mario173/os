/*
 * Jednostavni primjer za generiranje prostih brojeva korištenjem
   "The GNU Multiple Precision Arithmetic Library" (GMP)
 */

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h> // sleep

#include "slucajni_prosti_broj.h"

uint64_t ms[10] = { 0 };
int ulaz = 0, izlaz = 0, brojac = 0; // za međuspremnik
int kraj = 1, Ulaz[15] = { 0 }, broj[15] = { 0 }; // za višedretvenost

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

uint64_t generiraj_dobar_broj(struct gmp_pomocno *p)
{
	int i = 0, MASKA = 0x03ff, x;
	uint64_t broj = 0, odabrano = 0;

	while((odabrano == 0) && (i < 10))
	{
		broj = daj_novi_slucajan_prosti_broj (p);
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

	return odabrano;
}

void udi_u_kriticni_odsjecak(int id)
{
	int max = -1, i;
	// uzimanje broja -> id je prevelik
	Ulaz[id] = 1;
	for(i = 0; i < 15; i++)
	{
		if(broj[i] > max)
			max = broj[i];
	}
	broj[id] = max + 1;
	Ulaz[id] = 0;
	// provjera i čekanje dretve s manjim brojem
	for(i = 0; i < 15; i++)
	{
		while(Ulaz[i])
			continue;
		while(broj[i] != 0 && (broj[i] < broj[id] || (broj[i] == broj[id] && i < id)))
			continue;
	}
}

void izadi_iz_kriticnog_odsjecka(int id)
{
	broj[id] = 0;
}

void *radna_dretva(void *id_p)
{
	struct gmp_pomocno p;
	time_t start, end;
	int broj_odabranih = 0, *id = id_p;

	// lokalni generator G
	// inicijaliziraj generator slucajnih prostih brojeva &G, id
	inicijaliziraj_generator (&p, *id);
	time(&start);

	do {
		// osiguraj da se ne generiraju vise od 3 broja u sekundi
		if(broj_odabranih < 3)
		{
			// x = generiraj_dobar_broj(&G)
			// taj G koristiti i u daj_novi_slucajan_prosti_broj (&G)
			uint64_t broj = generiraj_dobar_broj(&p);
			//stavi broj u međuspremnik -> uđi u kritični odsječak(id) -> Lamportov algoritam, zastita međuspremnika
			udi_u_kriticni_odsjecak(*id);
			// stavi u međuspremnik
			stavi_u_ms(broj);
			// ispisi stavio x
			printf("Dretva %d stavila broj %" PRIu64 " u meduspremnik.\n", *id, broj);
			// izadi iz kriticnog odsjecka
			izadi_iz_kriticnog_odsjecka(*id);
			broj_odabranih++;
		}
		time(&end);
		if(start != end)
		{
			//prošla je sekunda
			time(&start);
			broj_odabranih = 0;
		}
	} while(kraj);

	//obriši generator
	obrisi_generator (&p);

	return NULL;
}

void *neradna_dretva(void *id_p)
{
	int *id = id_p;
	do {
		// spavaj 3 sekunde
		sleep(3);

		// uđi u kritični odsječak
		udi_u_kriticni_odsjecak(*id);

		// y = uzmi broj iz međuspremnika
		uint64_t ispis = uzmi_iz_ms();
		// ispiši uzeo y
		printf("Dretva %d ispisala broj %" PRIu64 "\n", *id, ispis);

		// izađi iz kritičnog odsječka
		izadi_iz_kriticnog_odsjecka(*id);
	} while(kraj);

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, br1[10], br2[5];
	pthread_t thr_id[15];
	// u for petlji pokreni dretve s pthread_create
	for (i = 0; i < 10; i++)
	{
		br1[i] = i;
		if (pthread_create( &thr_id[i], NULL, radna_dretva, &br1[i]))
		{
			printf("Ne mogu stvoriti novu dretvu!\n");
			_exit(1);
		}
	}
	for(i = 0; i < 5; i++)
	{
		br2[i] = i + 10;
		if(pthread_create(&thr_id[i + 10], NULL, neradna_dretva, &br2[i]))
		{
			printf("Ne mogu stvoriti novu dretvu!\n");
			_exit(1);
		}
	}
	sleep(20);
	kraj = 0;
	for ( i = 0; i < 15; i++ )
		pthread_join ( thr_id[i], NULL );

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