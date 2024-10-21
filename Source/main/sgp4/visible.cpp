#include "visible.h"
#include <stdint.h>

#define sunradius 695500   //km
#define earthradius 6378.137   //km
#define au 149597871   //km

/*
This file contains miscellaneous functions required for calculating visible satellites.
Some functions were originally written for Matlab as companion code for "Fundamentals of Astrodynamics
and Applications" by David Vallado (2007). (w) 719-573-2600, email dvallado@agi.com

Ported to C++ by Hopperpop with some modifications, November 2015.
*/

/*
% ------------------------------------------------------------------------------
%
%                           function sun
%
%  this function calculates the geocentric equatorial position vector
%    the sun given the julian date.  this is the low precision formula and
%    is valid for years from 1950 to 2050.  accuaracy of apparent coordinates
%    is 0.01  degrees.  notice many of the calculations are performed in
%    degrees, and are not changed until later.  this is due to the fact that
%    the almanac uses degrees exclusively in their formulations.
%
%  author        : david vallado                  719-573-2600   27 may 2002
%
%  revisions
%    vallado     - fix mean lon of sun                            7 mat 2004
%
%  inputs          description                    range / units
%    jdC          - julian date                    days from 4713 bc
%
%  outputs       :
%    rsun        - ijk position vector of the sun km

%  locals        :
%    meanlong    - mean longitude
%    meananomaly - mean anomaly
%    eclplong    - ecliptic longitude
%    obliquity   - mean obliquity of the ecliptic
%    tut1        - julian centuries of ut1 from
%                  jan 1, 2000 12h
%    ttdb        - julian centuries of tdb from
%                  jan 1, 2000 12h
%    hr          - hours                          0 .. 24              10
%    min         - minutes                        0 .. 59              15
%    sec         - seconds                        0.0  .. 59.99          30.00
%    temp        - temporary variable
%    deg         - degrees
%
%  coupling      :
%    none.
%
%  references    :
%    vallado       2007, 281, alg 29, ex 5-1
%
% [rsun,rtasc,decl] = sun ( jdC );
% ------------------------------------------------------------------------------
*/

void sun ( double jd, double rsun[3] ){

        double twopi      =     2.0*pi;
        double deg2rad    =     pi/180.0;

        // -------------------------  implementation   -----------------
        // -------------------  initialize values   --------------------
        double tut1 = ( jd - 2451545.0  )/ 36525.0;

        double meanlong = 280.460  + 36000.77 * tut1;
        meanlong = floatmod( meanlong, 360.0  );  //deg

        double meananomaly = 357.5277233  + 35999.05034 * tut1;
        meananomaly = floatmod( meananomaly*deg2rad,twopi );  //rad
        if ( meananomaly < 0.0  ){
            meananomaly= twopi + meananomaly;
        }

        double eclplong= meanlong + 1.914666471 * sin(meananomaly)+ 0.019994643 * sin(2.0 *meananomaly); //deg
        eclplong= floatmod( eclplong, 360.0  );  //deg

        double obliquity= 23.439291  - 0.0130042 * tut1;  //deg

        eclplong = eclplong *deg2rad;
        obliquity = obliquity *deg2rad;

        // --------- find magnitude of sun vector, )   components ------
        double magr= 1.000140612  - 0.016708617 *cos( meananomaly ) - 0.000139589 *cos( 2.0 *meananomaly );   // in au's

        rsun[0]= magr*cos( eclplong )*au;
        rsun[1]= magr*cos(obliquity)*sin(eclplong)*au;
        rsun[2]= magr*sin(obliquity)*sin(eclplong)*au;

}

//returns angle between sun surface and earth surface, from the viewpoint of the satellite
double Sgp4::visiblewrap(double jdCe) {
	double rsun[3];   //vector between earth and sun
	double razell[3];

	sun(jdC, rsun);  //calculate sun poistion vector
	rv2azel(rsun, siteLatRad, siteLonRad, siteAlt, jdC, razell);  //calc sun satEl

	double rsunsat[3]; //vector between sat and sun
	double rearth[3];
	double magsunsat, magearth;
	double phiearth, phisun, phi;
	double rnomearth, rnomsun;

	sgp4wrap(jdCe);

	rearth[0] = -ro[0];
	rearth[1] = -ro[1];
	rearth[2] = -ro[2];

	rsunsat[0] = rsun[0] + rearth[0];
	rsunsat[1] = rsun[1] + rearth[1];
	rsunsat[2] = rsun[2] + rearth[2];

	magsunsat = mag(rsunsat);
	magearth = mag(rearth);

	phiearth = asin(earthradius / magearth);
	phisun = asin(sunradius / magsunsat);

	phi = acos(dot(rearth, rsunsat) / magsunsat / magearth);

	return phi - phisun - phiearth;   ///grens op bijschaduw
	//return -phiearth + phisun + phi;  ///grens op echte schaduw

}


//calculate if satellite is visible
int16_t Sgp4::visible(bool& notdark, double& deltaphi){

    double rsun[3];   //vector between earth and sun
    double razell[3];

    sun(jdC, rsun);  //calculate sun poistion vector
    rv2azel(rsun, siteLatRad, siteLonRad, siteAlt, jdC, razell);  //calc sun satEl

    sunEl = razell[2] * 180 / pi;
      sunAz = razell[1] * 180 / pi;
  	  notdark = (razell[2] > sunoffset); //sun aboven -6°  => not dark enough

    double rsunsat[3]; //vector between sat and sun
    double rearth[3];
    double magsunsat, magearth;
    double phiearth, phisun, phi;
	double rnomearth, rnomsun;

    rearth[0] = -ro[0];
    rearth[1] = -ro[1];
    rearth[2] = -ro[2];

    rsunsat[0] = rsun[0] + rearth[0];
    rsunsat[1] = rsun[1] + rearth[1];
    rsunsat[2] = rsun[2] + rearth[2];

    magsunsat = mag(rsunsat);
    magearth = mag(rearth);

    phiearth = asin(earthradius/magearth);
    phisun = asin(sunradius/magsunsat);

    phi = acos(dot(rearth,rsunsat)/magsunsat/magearth);
	deltaphi = phi - phisun - phiearth;

    if (phiearth > phisun && phi < phiearth - phisun){   //umbral eclipse
        return 0;
    }

    if (phiearth < phisun && phi < phisun - phiearth){  //partial eclipse
      return (int16_t)(( 1 - phiearth*phiearth/(phisun*phisun) )*1000);  //approach to the surface coverage with a square sun and earth
    }

    if (fabs(phiearth-phisun) < phi && phi < phiearth + phisun){  //penumbral eclipse
        if (phiearth > phisun){
             return (int16_t)(( (phisun+phi-phiearth)/(phisun*2.0) )*1000);  //approach to the surface coverage with a square sun and earth
        }else{
             return (int16_t)(( 1 - phiearth*(phisun - phi + phiearth)/(2.0*phisun*phisun) )*1000);  //approach to the surface coverage with a square sun and earth
        }
    }

    return 1000;  //no eclipse => visible

}

int16_t Sgp4::visible() {
	bool notdark;
	double deltaphi;
	int16_t viss = visible(notdark,deltaphi);
	if (notdark) {
		return -1;
	}
	else {
		return viss;
	}
}
