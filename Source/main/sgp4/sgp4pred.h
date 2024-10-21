/*
This file contains miscellaneous functions required for predicting satellite overpasses and the main class for easy acces to all the functions.
Most functions are based on the columns written by Dr. T.S. Kelso on the celestrak website (https://celestrak.com/columns/)

Written by Hopperpop
*/

#ifndef _sgp4pred_
#define _sgp4pred_

#include "sgp4ext.h"
#include "sgp4unit.h"
#include "sgp4io.h"
#include "sgp4coord.h"
#include <stdint.h>

enum visibletype
{
  daylight,
  eclipsed,
  lighted
};

enum shadowtransit
{
	none,
	enter,
	leave
};

struct passinfo
{
  double jdstart;
  double jdstop;
  double jdmax;
  double jdtransit;

  double maxelevation;
  double minelevation;  //elevation at start and end
  double transitelevation;

  double azstart;
  double azmax;
  double azstop;
  double aztransit;

  visibletype visstart;
  visibletype visstop;
  visibletype vismax;
  visibletype vistransit;

  visibletype sight;
  shadowtransit transit;

};



class Sgp4 {
    char opsmode;
    gravconsttype  whichconst;
    double ro[3];
    double vo[3];
    double razel[3];
    double offset; //Min elevation for overpass prediction in radials
    double sunoffset;  //Min elevation sun for daylight in radials
    double jdC;    //Current used julian date
    double jdCp;    //Current used julian date for prediction

    double sgp4wrap( double jdCe);  //returns the elevation for a given julian date
	double visiblewrap(double jdCe);  //returns angle between sun surface and earth surface

  public:
    char satName[25];   ///satellite name
	char line1[80];     //tle line 1
	char line2[80];     //tle line 2

    double revpday;  ///revolutions per day
    elsetrec satrec;
    double siteLat, siteLon, siteAlt, siteLatRad, siteLonRad;
    double satLat, satLon, satAlt, satAz, satEl, satDist,satJd;
    double sunAz, sunEl;
	int16_t satVis;

    Sgp4();
    bool init(const char naam[], char longstr1[130], char longstr2[130]);  //initialize parameters from 2 line elements
    void site(double lat, double lon, double alt);  //initialize site latitude[degrees],longitude[degrees],altitude[meters]
    void setsunrise(double degrees);   //change the elevation that the sun needs to make it daylight

    void findsat(double jdI);     //find satellite position from julian date
    void findsat(unsigned long);  //find satellite position from unix time

    bool nextpass( passinfo* passdata, int itterations); // calculate next overpass data, returns true if succesfull
	bool nextpass(passinfo* passdata, int itterations, bool direc); //direc = false for forward search, true for backwards search
    bool nextpass(passinfo* passdata, int itterations, bool direc, double minimumElevation); //minimumElevation = minimum elevation above the horizon (in degrees)
    bool initpredpoint( double juliandate , double startelevation); //initialize prediction algorithm, starting from a juliandate and predict passes aboven startelevation
    bool initpredpoint( unsigned long unix, double startelevation); // from unix time

    int16_t visible();  //check if satellite is visible
	int16_t visible(bool& notdark, double& deltaphi);

	double getpredpoint();
	void setpredpoint(double jdCe);
} ;



#endif
