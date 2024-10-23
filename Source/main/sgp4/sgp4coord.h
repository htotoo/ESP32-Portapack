#ifndef _sgp4coord_
#define _sgp4coord_

/*     
sgp4coord.h

This file contains miscellaneous functions required for coordinate transformation.
Functions were originally written for Matlab as companion code for "Fundamentals of Astrodynamics 
and Applications" by David Vallado (2007). (w) 719-573-2600, email dvallado@agi.com

Ported to C++ by Grady Hillhouse with some modifications, July 2015.
*/

#include <math.h>
#include <string.h>

//void teme2ecef(double rteme[3], double vteme[3], double jdut1, double recef[3], double vecef[3]);
void teme2ecef(double rteme[3], double jdut1, double recef[3]);

void polarm(double jdut1, double pm[3][3]);

void ijk2ll(double r[3], double latlongh[3]);

//void site(double latgd, double lon, double alt, double rs[3], double vs[3]);
void site(double latgd, double lon, double alt, double rs[3]);

//void rv2azel(double ro[3], double vo[3], double latgd, double lon, double alt, double jdut1, double razel[3], double razelrates[3]);
void rv2azel(double ro[3], double latgd, double lon, double alt, double jdut1, double razel[3]);

void rot3(double invec[3], double xval, double outvec[3]);

void rot2(double invec[3], double xval, double outvec[3]);

double getJulianFromUnix(double unixSecs);

unsigned long getUnixFromJulian(double julian);

#endif
