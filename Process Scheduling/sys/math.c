/*
 * expdev.c
 *
 *  Created on: Jan 10, 2013
 *      Author: hlee
 */

#include <math.h>

double log(double x){
	double e = 2.71828; // constant e
	int partA = 0;
	double remainder = x;
	if(x < 2.0) {
		return taylor_log(x);
	} else {
		do{
			remainder /= e;
			partA ++;
		}while(remainder >= 2.0);
	}
	double partB = taylor_log(remainder);
	return partA + partB;
}


// Using taylor serices for natural logarithmic function
// Note: x > 0 && x <= 2
double taylor_log(double input){
	double x = input -1;
	double res = x;
	int i;
	for(i=2;i<=20;i++){
		if(i%2){
			res += pow(x, i) / (double)i;
		}else{
			res -= pow(x, i) / (double)i;
		}
	}
	return res;
}

double pow(double x, int y){
	int i;
	double res = 1.0;
	for(i = 0; i < y; i++ ){
		res = res * x;
	}
	return res;
}

double expdev(double lamda) {

	double dummy;

	do
		dummy = (double) rand() / RAND_MAX;
	while (dummy == 0.0);

	return -log(dummy) / lamda;
}
