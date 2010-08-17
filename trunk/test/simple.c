#include <dpthread-wrapper.h>

int func(int a, int b)
{
	int ret = a + b; 
	return ret; 
}

void *worker(void *v)
{
	int x = (int)v; 
	int sum = 0; 
	switch ( x ) { 
	case 0: 
		break;
	case 1:
		det_disable_logical_clock(); 
		sum = func(1,2); 
		det_enable_logical_clock(); 
		break; 
	case 2:
		det_disable_logical_clock();
		det_enable_logical_clock(); 
		break; 
	case 3:
		det_increase_logical_clock(7); 
		break; 

	}

	printf("sum = %d\n", sum); 
}

main(int argc, char *argv[])
{
	int cnt = atoi(argv[1]); 
	int tid; 
	det_init(argc, argv); 

	pthread_create(&tid, NULL, worker, cnt); 
	pthread_join(tid, NULL); 

	det_print_stat();
}
