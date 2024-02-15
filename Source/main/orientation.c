
#include "orientation.h"

float declinationAngle = 0; // todo setup to web interface http://www.magnetic-declination.com/

OrientationSensors orientation_inited = Orientation_none;

void init_orientation()
{
    // HCM5883l
    hmc5883lInit(I2C0_DEV);
    if (hmc5883lTestConnection())
    {
        hmc5883lSetMode(0);
        ESP_LOGI("Orientation", "hmc5883lTestConnection OK");
        orientation_inited = Orientation_hmc5883l;
    }

    // end of list
    if (orientation_inited == Orientation_none)
    {
        ESP_LOGI("Orientation", "No compatible sensor found");
    }
}

float fix_heading(float heading)
{
    if (heading < 0)
        heading += 2 * M_PI;

    if (heading > 2 * M_PI)
        heading -= 2 * M_PI;
    return (heading + declinationAngle);
}

float get_heading()
{
    if (orientation_inited == Orientation_none)
        return 0;
    if (orientation_inited == Orientation_hmc5883l)
    {
        float heading_x = hmc5883lGetHeadingX();
        float heading_y = hmc5883lGetHeadingY();
        return (atan2(heading_y, heading_x) * 180) / M_PI + declinationAngle;
    }
    return 0;
}

float get_heading_degrees()
{
    return get_heading() * 180 / M_PI;
}

void reset_orientation_calibration()
{
    // todo
}

void calibrate_orientation(uint8_t sec)
{
    // todo, run a calibration for N sec to get min-max. should be around 10
}

void set_declination(float declination) { declinationAngle = declination; }
float get_declination() { return declinationAngle; }