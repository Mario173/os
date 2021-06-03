/*
 * Jednostavni primjer za generiranje prostih brojeva korištenjem
   "The GNU Multiple Precision Arithmetic Library" (GMP)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h> // memset
#include <unistd.h> // sleep

#include "slucajni_prosti_broj.h"

#define velicinaMs 10
#define brRadnih 10
#define brNeradnih 5

pthread_mutex_t monitor;
pthread_cond_t red_prazni, red_puni, nema_memorije;
uint64_t ms[velicinaMs] = { 0 };
int ulaz = 0, izlaz = 0, brojac = 0; // za međuspremnik
int kraj = 1, Ulaz[brRadnih + brNeradnih] = { 0 }, broj[brRadnih + brNeradnih] = { 0 }; // za višedretvenost
int velicina_spremnika = 50;
char* spremnik;

void stavi_u_ms(uint64_t broj)
{
	if(brojac < velicinaMs)
	{
		ms[ulaz] = broj;
		ulaz = (ulaz + 1) % velicinaMs;
		brojac++;
	}
}

uint64_t uzmi_iz_ms()
{
	if(brojac > 0)
	{
		uint64_t broj = ms[izlaz];
		ms[izlaz] = 0;
		izlaz = (izlaz + 1) % velicinaMs;
		brojac--;
		return broj;
	}
	return 0;
}

int prost(int x)
{
	// Eratostenovo sito
	if(x <= 1) {
		return 0;
	}
	int prosti[x];
	memset(prosti, 1, x * sizeof(int));
	for(int i = 2; i * i <= x; i++) {
		if(prosti[i] == 1) {
			for(int j = i * i; j <= x; j++) {
				if(j == x) {
					return 0;
				}
				prosti[j] = 0; // nisu prosti
			}
		}
	}
	return prosti[x];
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
			pthread_mutex_lock(&monitor);
			while(brojac == velicinaMs) {
				pthread_cond_wait(&red_prazni, &monitor);
			}
			// stavi u međuspremnik
			stavi_u_ms(broj);
			// ispisi stavio x
			printf("Dretva %d stavila broj %" PRIu64 " u meduspremnik.\n", *id, broj);
			// monitor
			broj_odabranih++;
			pthread_cond_signal(&red_puni);
			pthread_mutex_unlock(&monitor);
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

int zauzmi(int size, int id) {
	int poc_min = -1, min = INT_MAX, curr = 0, curr_poc = 0;
	for(int i = 0; i < velicina_spremnika; i++) {
		if(spremnik[i] == '-') {
			if(curr == 0) {
				curr_poc = i;
			}
			curr++;
		} else {
			if(curr >= size && curr < min) {
				min = curr;
				poc_min = curr_poc;
			}
			curr = 0;
		}

		if(i == velicina_spremnika - 1) {
			if(curr >= size && curr < min) {
				min = curr;
				poc_min = curr_poc;
			}
		}
	}

	if(poc_min != -1) {
		char c = id -10 + '0';
		for(int i = 0; i < size; i++) {
			spremnik[i + poc_min] = c;
		}
	}

	return poc_min;
}

void oslobodi(int pocetak, int id) {
	int i = pocetak;
	char c = id - 10 + '0';
	while(i < velicina_spremnika && spremnik[i] == c) {
		spremnik[i] = '-';
		i++;
	}
}

void *neradna_dretva(void *id_p)
{
	int *id = id_p;
	do {
		// spavaj 3 sekunde
		sleep(3);
		pthread_mutex_lock(&monitor);
		while(brojac == 0) {
			pthread_cond_wait(&red_puni, &monitor);
		}
		// y = uzmi broj iz međuspremnika
		uint64_t ispis = uzmi_iz_ms();
		// ispiši uzeo y
		printf("Dretva %d ispisala broj %" PRIu64 "\n", *id, ispis);

		pthread_cond_signal(&red_prazni);

		// najbolji odgovarajući
		int x, ostatak = ispis % 20;
		do {
			x = zauzmi(ostatak, *id);
			if(x == -1) {
				pthread_cond_wait(&nema_memorije, &monitor);
			}
		} while(x == -1);

		pthread_mutex_unlock(&monitor);

		sleep(ostatak % 5);

		pthread_mutex_lock(&monitor);

		printf("Dretva %d potrosila broj %" PRIu64 "\n", *id, ispis);
		printf("Stanje spremnika: %s\n", spremnik);
		oslobodi(x, *id);
		pthread_cond_broadcast(&nema_memorije);

		pthread_mutex_unlock(&monitor);
	} while(kraj);

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, br1[brRadnih], br2[brNeradnih];
	pthread_t thr_id[brRadnih + brNeradnih];

	spremnik = (char*)malloc(velicina_spremnika * sizeof(char));

	for(i = 0; i < velicina_spremnika; i++) {
		spremnik[i] = '-';
	}

	pthread_mutex_init(&monitor, NULL);
	pthread_cond_init(&red_prazni, NULL);
	pthread_cond_init(&red_puni, NULL);
	pthread_cond_init(&nema_memorije, NULL);
	// u for petlji pokreni dretve s pthread_create
	for (i = 0; i < brRadnih; i++)
	{
		br1[i] = i;
		if (pthread_create( &thr_id[i], NULL, radna_dretva, &br1[i]))
		{
			printf("Ne mogu stvoriti novu dretvu!\n");
			_exit(1);
		}
	}
	for(i = 0; i < brNeradnih; i++)
	{
		br2[i] = i + brRadnih;
		if(pthread_create(&thr_id[i + brRadnih], NULL, neradna_dretva, &br2[i]))
		{
			printf("Ne mogu stvoriti novu dretvu!\n");
			_exit(1);
		}
	}
	sleep(20);
	kraj = 0;
	brojac = 10000;
	for( i = 0; i < brRadnih; i++ ) {
		pthread_cond_signal(&red_prazni);
		pthread_mutex_unlock(&monitor);
	}
	pthread_cond_broadcast(&nema_memorije);
	for( i = 0; i < brNeradnih; i++ ) {
		pthread_cond_signal(&red_puni);
		pthread_mutex_unlock(&monitor);
	}
	for ( i = 0; i < brRadnih + brNeradnih; i++ )
		pthread_join ( thr_id[i], NULL );

	pthread_mutex_destroy(&monitor);
	pthread_cond_destroy(&red_prazni);
	pthread_cond_destroy(&red_puni);
	pthread_cond_destroy(&nema_memorije);
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