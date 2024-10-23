/*     
sgp4coord.cpp

This file contains miscellaneous functions required for coordinate transformation.
Functions were originally written for Matlab as companion code for "Fundamentals of Astrodynamics 
and Applications" by David Vallado (2007). (w) 719-573-2600, email dvallado@agi.com

Ported to C++ by Grady Hillhouse with some modifications, July 2015.
*/
#include "sgp4coord.h"
#include "sgp4ext.h"

/*
teme2ecef

This function transforms a vector from a true equator mean equinox (TEME)
frame to an earth-centered, earth-fixed (ECEF) frame.

Author: David Vallado, 2007
Ported to C++ by Grady Hillhouse with some modifications, July 2015.

INPUTS          DESCRIPTION                     RANGE/UNITS
rteme           Position vector (TEME)          km
vteme           Velocity vector (TEME)          km/s
jdut1           Julian date                     days

OUTPUTS         DESCRIPTION                     RANGE/UNITS
recef           Position vector (ECEF)          km  
vecef           Velocity vector (ECEF)          km/s
*/

//void teme2ecef(double rteme[3], double vteme[3], double jdut1, double recef[3], double vecef[3])
void teme2ecef(double rteme[3], double jdut1, double recef[3])
{
    double gmst;
    double st[3][3];
    double rpef[3];
    //double vpef[3];
    double pm[3][3];
    //double omegaearth[3];
    
    //Get Greenwich mean sidereal time
    gmst = gstime(jdut1);
    
    //st is the pef - tod matrix
    st[0][0] = cos(gmst);
    st[0][1] = -sin(gmst);
    st[0][2] = 0.0;
    st[1][0] = sin(gmst);
    st[1][1] = cos(gmst);
    st[1][2] = 0.0;
    st[2][0] = 0.0;
    st[2][1] = 0.0;
    st[2][2] = 1.0;
    
    //Get pseudo earth fixed position vector by multiplying the inverse pef-tod matrix by rteme
    rpef[0] = st[0][0] * rteme[0] + st[1][0] * rteme[1] + st[2][0] * rteme[2];
    rpef[1] = st[0][1] * rteme[0] + st[1][1] * rteme[1] + st[2][1] * rteme[2];
    rpef[2] = st[0][2] * rteme[0] + st[1][2] * rteme[1] + st[2][2] * rteme[2];
    
    //Get polar motion vector
    polarm(jdut1, pm);
    
    //ECEF postion vector is the inverse of the polar motion vector multiplied by rpef
    recef[0] = pm[0][0] * rpef[0] + pm[1][0] * rpef[1] + pm[2][0] * rpef[2];
    recef[1] = pm[0][1] * rpef[0] + pm[1][1] * rpef[1] + pm[2][1] * rpef[2];
    recef[2] = pm[0][2] * rpef[0] + pm[1][2] * rpef[1] + pm[2][2] * rpef[2];
    
    //Earth's angular rotation vector (omega)
    //Note: I don't have a good source for LOD. Historically it has been on the order of 2 ms so I'm just using that as a constant. The effect is very small.
    //omegaearth[0] = 0.0;
    //omegaearth[1] = 0.0;
    //omegaearth[2] = 7.29211514670698e-05 * (1.0  - 0.0015563/86400.0);
    
    //Pseudo Earth Fixed velocity vector is st'*vteme - omegaearth X rpef
    //vpef[0] = st[0][0] * vteme[0] + st[1][0] * vteme[1] + st[2][0] * vteme[2] - (omegaearth[1]*rpef[2] - omegaearth[2]*rpef[1]);
    //vpef[1] = st[0][1] * vteme[0] + st[1][1] * vteme[1] + st[2][1] * vteme[2] - (omegaearth[2]*rpef[0] - omegaearth[0]*rpef[2]);
    //vpef[2] = st[0][2] * vteme[0] + st[1][2] * vteme[1] + st[2][2] * vteme[2] - (omegaearth[0]*rpef[1] - omegaearth[1]*rpef[0]);
    
    //ECEF velocty vector is the inverse of the polar motion vector multiplied by vpef
    //vecef[0] = pm[0][0] * vpef[0] + pm[1][0] * vpef[1] + pm[2][0] * vpef[2];
    //vecef[1] = pm[0][1] * vpef[0] + pm[1][1] * vpef[1] + pm[2][1] * vpef[2];
    //vecef[2] = pm[0][2] * vpef[0] + pm[1][2] * vpef[1] + pm[2][2] * vpef[2];
}

/*
polarm

This function calulates the transformation matrix that accounts for polar
motion. Polar motion coordinates are estimated using IERS Bulletin
rather than directly input for simplicity.

Author: David Vallado, 2007
Ported to C++ by Grady Hillhouse with some modifications, July 2015.

INPUTS          DESCRIPTION                     RANGE/UNITS
jdut1           Julian date                     days

OUTPUTS         DESCRIPTION
pm              Transformation matrix for ECEF - PEF
*/

void polarm(double jdut1, double pm[3][3])
{
    double MJD; //Julian Date - 2,400,000.5 days
    double A;
    double C;
    double xp; //Polar motion coefficient in radians
    double yp; //Polar motion coefficient in radians
    
    //Predict polar motion coefficients using IERS Bulletin - A (Vol. XXVIII No. 030)
    MJD = jdut1 - 2400000.5;
    A = 2 * pi * (MJD - 57226) / 365.25;
    C = 2 * pi * (MJD - 57226) / 435;
    
    xp = (0.1033 + 0.0494*cos(A) + 0.0482*sin(A) + 0.0297*cos(C) + 0.0307*sin(C)) * 4.84813681e-6;
    yp = (0.3498 + 0.0441*cos(A) - 0.0393*sin(A) + 0.0307*cos(C) - 0.0297*sin(C)) * 4.84813681e-6;
    
    pm[0][0] = cos(xp);
    pm[0][1] = 0.0;
    pm[0][2] = -sin(xp);
    pm[1][0] = sin(xp) * sin(yp);
    pm[1][1] = cos(yp);
    pm[1][2] = cos(xp) * sin(yp);
    pm[2][0] = sin(xp) * cos(yp);
    pm[2][1] = -sin(yp);
    pm[2][2] = cos(xp) * cos(yp);
}

/*
ijk2ll

This function calulates the latitude, longitude and altitude
given the ECEF position matrix.

Author: David Vallado, 2007
Ported to C++ by Grady Hillhouse with some modifications, July 2015.

INPUTS          DESCRIPTION                     RANGE/UNITS
r               Position matrix (ECEF)          km

OUTPUTS         DESCRIPTION
latlongh        Latitude, longitude, and altitude (rad, rad, and km)
*/

void ijk2ll(double r[3], double latlongh[3])
{
    double twopi = 2.0*pi;
    double small = 0.00000001;          //small value for tolerances
    double re = 6378.137;               //radius of earth in km
    double eesqrd = 0.006694385000;     //eccentricity of earth sqrd
    double magr, temp, rtasc;
    
    magr = mag(r);
    temp = sqrt(r[0]*r[0] + r[1]*r[1]);
 
    if(fabs(temp) < small)
    {
        rtasc = sgn(r[2]) * pi * 0.5;
    }
    else
    {
        rtasc = atan2(r[1], r[0]);
    }
    
    latlongh[1] = rtasc;
    
    if (fabs(latlongh[1]) >= pi)
    {
        if (latlongh[1] < 0.0)
        {
            latlongh[1] += twopi;
        }
        else
        {
            latlongh[1] -= twopi;
        }
    }
    
    latlongh[0] = asin(r[2] / magr);
    
    //Iterate to find geodetic latitude
    int i = 1;
    double olddelta = latlongh[0] + 10.0;
    double sintemp, c = 0;
    
    while ( (fabs(olddelta - latlongh[0]) >= small) && (i < 10) )
    {
        olddelta = latlongh[0];
        sintemp = sin(latlongh[0]);
        c = re / sqrt(1.0 - eesqrd*sintemp*sintemp);
        latlongh[0] = atan( (r[2] + c*eesqrd*sintemp) / temp );
        i++;
    }
    
    if (0.5*pi - fabs(latlongh[0]) > pi/180.0)
    {
        latlongh[2] = (temp/cos(latlongh[0])) - c;
    }
    else
    {
        latlongh[2] = r[2]/sin(latlongh[0]) - c*(1.0 - eesqrd);
    }
}

/*
site

This function finds the position and velocity vectors for a site. The
outputs are in the ECEF coordinate system. Note that the velocity vector
is zero because the coordinate system rotates with the earth.

Author: David Vallado, 2007
Ported to C++ by Grady Hillhouse with some modifications, July 2015.

INPUTS          DESCRIPTION                     RANGE/UNITS
latgd           Site geodetic latitude          -pi/2 to pi/2 in radians
lon             Longitude                       -2pi to 2pi in radians
alt             Site altitude                   km

OUTPUTS         DESCRIPTION
rs              Site position vector            km
vs              Site velocity vector            km/s
*/

//void site(double latgd, double lon, double alt, double rs[3], double vs[3])
void site(double latgd, double lon, double alt, double rs[3])
{
    double sinlat, re, eesqrd, cearth, rdel, rk;
    re = 6378.137;              //radius of earth in km
    eesqrd = 0.006694385000;    //eccentricity of earth sqrd
    
    //Find rdel and rk components of site vector
    sinlat = sin(latgd);
    cearth = re / sqrt( 1.0 - (eesqrd*sinlat*sinlat) );
    rdel = (cearth + alt) * cos(latgd);
    rk = ((1.0 - eesqrd) * cearth + alt ) * sinlat;
    
    //Find site position vector (ECEF)
    rs[0] = rdel * cos( lon );
    rs[1] = rdel * sin( lon );
    rs[2] = rk;
    
    //Velocity of site is zero because the coordinate system is rotating with the earth
    //vs[0] = 0.0;
    //vs[1] = 0.0;
    //vs[2] = 0.0;
}


/*
rv2azel

This function calculates the range, elevation, and azimuth (and their rates)
from the TEME vectors output by the SGP4 function.

Author: David Vallado, 2007
Ported to C++ by Grady Hillhouse with some modifications, July 2015.

INPUTS          DESCRIPTION                     RANGE/UNITS
ro              Sat. position vector (TEME)     km
vo              Sat. velocity vector (TEME)     km/s
latgd           Site geodetic latitude          -pi/2 to pi/2 in radians
lon             Site longitude                  -2pi to 2pi in radians
alt             Site altitude                   km
jdut1           Julian date                     days

OUTPUTS         DESCRIPTION
razel           Range, azimuth, and elevation matrix
razelrates      Range rate, azimuth rate, and elevation rate matrix
*/

//void rv2azel(double ro[3], double vo[3], double latgd, double lon, double alt, double jdut1, double razel[3], double razelrates[3])
void rv2azel(double ro[3], double latgd, double lon, double alt, double jdut1, double razel[3])
{
    //Locals
    double halfpi = pi * 0.5;
    double small  = 0.00000001;
    double temp;
    double rs[3];
    //double vs[3];
    double recef[3];
    //double vecef[3];
    double rhoecef[3];
    //double drhoecef[3];
    double tempvec[3];
    double rhosez[3];
    //double drhosez[3];
    double magrhosez;
    double rho, az, el;
    //double drho, daz, del;
    
    //Get site vector in ECEF coordinate system
    //site(latgd, lon, alt, rs, vs);
    site(latgd, lon, alt, rs);
    
    //Convert TEME vectors to ECEF coordinate system
    //teme2ecef(ro, vo, jdut1, recef, vecef);
    teme2ecef(ro, jdut1, recef);
    
    //Find ECEF range vectors
    for (int i = 0; i < 3; i++)
    {
        rhoecef[i] = recef[i] - rs[i];
        //drhoecef[i] = vecef[i];
    }
    rho = mag(rhoecef); //Range in km
    
    //Convert to SEZ (topocentric horizon coordinate system)
    rot3(rhoecef, lon, tempvec);
    rot2(tempvec, (halfpi-latgd), rhosez);
    
    //rot3(drhoecef, lon, tempvec);
    //rot2(tempvec, (halfpi-latgd), drhosez);
    
    //Calculate azimuth, and elevation
    temp = sqrt(rhosez[0]*rhosez[0] + rhosez[1]*rhosez[1]);
    if (temp < small)
    {
        el = sgn(rhosez[2]) * halfpi;
        //az = atan2(drhosez[1], -drhosez[0]);
        az = NAN;
    }
    else
    {
        magrhosez = mag(rhosez);
        el = asin(rhosez[2]/magrhosez);
        az = atan2(rhosez[1], -rhosez[0]);
    }
    
    //Calculate rates for range, azimuth, and elevation
    /**
    drho = dot(rhosez,drhosez) / rho;
    
    if(fabs(temp*temp) > small)
    {
        daz = (drhosez[0]*rhosez[1] - drhosez[1]*rhosez[0]) / (temp * temp);
    }
    else
    {
        daz = 0.0;
    }
    
    if(fabs(temp) > small)
    {
        del = (drhosez[2] - drho*sin(el)) / temp;
    }
    else
    {
        del = 0.0;
    }
    ***/
    //Move values to output vectors
    razel[0] = rho;             //Range (km)
    razel[1] = az;              //Azimuth (radians)
    razel[2] = el;              //Elevation (radians)
    
    //razelrates[0] = drho;       //Range rate (km/s)
    //razelrates[1] = daz;        //Azimuth rate (rad/s)
    //razelrates[2] = del;        //Elevation rate (rad/s)
}

void rot3(double invec[3], double xval, double outvec[3])
{
    double temp = invec[1];
    double c = cos(xval);
    double s = sin(xval);
    
    outvec[1] = c*invec[1] - s*invec[0];
    outvec[0] = c*invec[0] + s*temp;
    outvec[2] = invec[2];
}

void rot2(double invec[3], double xval, double outvec[3])
{
    double temp = invec[2];
    double c = cos(xval);
    double s = sin(xval);
    
    outvec[2] = c*invec[2] + s*invec[0];
    outvec[0] = c*invec[0] - s*temp;
    outvec[1] = invec[1];
}

/*
getJulianFromUnix

returns the Julian Date from Unix Time
*/

double getJulianFromUnix(double unixSecs)
{
   return ( unixSecs / 86400.0 ) + 2440587.5;
}


unsigned long getUnixFromJulian(double julian)
{
	return (unsigned long)((julian - 2440587.5)*86400.0 + 0.5);
}
