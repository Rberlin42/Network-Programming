#include <mpi.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>

int end_now = 0;

void sig_handler(int signo)
{
    if (signo == SIGUSR1) {
        end_now = 1;
    }
}

//return 1 if n is prime
int isPrime(unsigned int n){
    if(n < 2) return 0;

    unsigned int max = (unsigned int) sqrt(n);
    for (unsigned int i = 2; i <= max; i++)
        if(n % i == 0)
            return 0;

    return 1;
}

int main(int argc, char **argv)
{
    int count, id;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &count);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    int total_primes = 0;
    unsigned int progress = 10;
    unsigned int range = 10;

#ifdef SIGNAL
    time_t start, current;
    if(id == 0){
        time(&start);
    }
#endif

    if(id == 0)
        printf("%17s%21s\n", "N", "Primes");
    
    signal(SIGUSR1, sig_handler);
    
    while (!end_now) {
        int n_primes = 0;
        unsigned int i;

        //distribute numbers to calculate for this interval
        for(i = progress-10 + id; i < progress; i += count){
            //count prime
            n_primes += isPrime(i);

#ifdef SIGNAL
            //signal after 1 min
            if(id == 0){
                time(&current);
                if(difftime(current, start) > 60)
                    sig_handler(SIGUSR1);
            }
#endif
        }

        //check for a signal
        int recv;
        MPI_Allreduce(&end_now, &recv, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        end_now = recv;
        if(end_now && id == 0)
            printf("<Signal Received>\n");

        if(id == 0){
            int n;
            MPI_Reduce(&n_primes, &n, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
            total_primes += n;
        }
        else{
            MPI_Reduce(&n_primes, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        }

        //add up and print total for this interval
        if(id == 0 && (progress == range || end_now)){
            range *= 10;
            printf("%17d%20u\n", progress, total_primes);
        }

        //increment to next interval
        if(progress >= UINT_MAX-10)
            progress = UINT_MAX;
        else
            progress += 10;

    }
    
    MPI_Finalize();
    
    return 0;
}
