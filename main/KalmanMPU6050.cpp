#include "KalmanMPU6050.h"
#include "logdef.h"
#include "sensor.h"
#include "quaternion.h"
#include "vector_3d.h"
#include "sensor_processing_lib.h"
#include "vector.h"

#define DEBUG_INIT()
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_TS_PRINT(x)
#define DEBUG_TS_PRINTLN(x)

#define sqr(x) x *x
#define hypotenuse(x, y) sqrt(sqr(x) + sqr(y))

// MPU-6050
#define IMU_ADDR 0x68
#define IMU_ACCEL_XOUT_H 0x3B
#define IMU_REG 0x19
#define IMU_PWR_MGMT_1 0x6B

// Kalman Variables
Kalman IMU::kalmanX; // Create the Kalman instances
Kalman IMU::kalmanY;
Kalman IMU::kalmanZ;

// Private Variables
double IMU::gyroXAngle = 0;
double IMU::gyroYAngle = 0; // Angle calculate using the gyro only
double IMU::gyroZAngle = 0; // Angle calculate using the gyro only
uint32_t IMU::lastProcessed = 0;

float 	IMU::myrolly = 0;
float 	IMU::myrollz = 0;
float 	IMU::myaccroll = 0;
double  IMU::mypitch = 0;
double  IMU::filterPitch = 0;
double  IMU::filterRoll = 0;
double  IMU::filterYaw = 0;

uint64_t IMU::last_rts=0;
double IMU::accelX = 0.0;
double IMU::accelY = 0.0;
double IMU::accelZ = 0.0;
double IMU::gyroX = 0.0;
float  IMU::pitchfilter = 0.0;
float  IMU::rollfilter = 0.0;
double IMU::gyroY = 0.0;
double IMU::gyroZ = 0.0;
double IMU::kalXAngle = 0.0;
double IMU::kalYAngle = 0.0;
float IMU::filterAccRoll = 0.0;
float IMU::filterGyroRoll = 0.0;
Quaternion IMU::att_quat(0,0,0,0);
vector_ijk IMU::att_vector;
euler_angles IMU::euler;

vector_ijk gravity_vector( 0,0,-1 );

// Kalman Function Definition

void Kalman_Init(Kalman *kalPointer, double qang, double qbias, double rmeas )
{
	/* We will set the variables like so, these can also be tuned by the user */
	kalPointer->Q_angle = 0.01;
	kalPointer->Q_bias = 0.03;
	kalPointer->R_measure = 0.1;

	kalPointer->angle = 0; // Reset the angle
	kalPointer->bias = 0;  // Reset bias

	kalPointer->P[0][0] = 0; // Since we assume that the bias is 0 and we know the starting angle (use setAngle), the error covariance matrix is set like so - see: http://en.wikipedia.org/wiki/Kalman_filter#Example_application.2C_technical
	kalPointer->P[0][1] = 0;
	kalPointer->P[1][0] = 0;
	kalPointer->P[1][1] = 0;
}

// The angle should be in degrees and the rate should be in degrees per second and the delta time in seconds
double Kalman_GetAngle(Kalman *kalPointer,
		double newAngle, double newRate, double dt)
{
	// KasBot V2  -  Kalman filter module - http://www.x-firm.com/?page_id=145
	// Modified by Kristian Lauszus
	// See my blog post for more information: http://blog.tkjelectronics.dk/2012/09/a-practical-approach-to-kalman-filter-and-how-to-implement-it

	// Discrete Kalman filter time update equations - Time Update ("Predict")
	// Update xhat - Project the state ahead
	/* Step 1 */
	kalPointer->rate = newRate - kalPointer->bias;
	kalPointer->angle += dt * kalPointer->rate;

	// Update estimation error covariance - Project the error covariance ahead
	/* Step 2 */
	kalPointer->P[0][0] += dt * (dt * kalPointer->P[1][1] - kalPointer->P[0][1] - kalPointer->P[1][0] + kalPointer->Q_angle);
	kalPointer->P[0][1] -= dt * kalPointer->P[1][1];
	kalPointer->P[1][0] -= dt * kalPointer->P[1][1];
	kalPointer->P[1][1] += kalPointer->Q_bias * dt;

	// Discrete Kalman filter measurement update equations - Measurement Update ("Correct")
	// Calculate Kalman gain - Compute the Kalman gain
	/* Step 4 */
	kalPointer->S = kalPointer->P[0][0] + kalPointer->R_measure;
	/* Step 5 */
	kalPointer->K[0] = kalPointer->P[0][0] / kalPointer->S;
	kalPointer->K[1] = kalPointer->P[1][0] / kalPointer->S;

	// Calculate angle and bias - Update estimate with measurement zk (newAngle)
	/* Step 3 */
	kalPointer->y = newAngle - kalPointer->angle;
	/* Step 6 */
	kalPointer->angle += kalPointer->K[0] * kalPointer->y;
	kalPointer->bias += kalPointer->K[1] * kalPointer->y;

	// Calculate estimation error covariance - Update the error covariance
	/* Step 7 */
	kalPointer->P[0][0] -= kalPointer->K[0] * kalPointer->P[0][0];
	kalPointer->P[0][1] -= kalPointer->K[0] * kalPointer->P[0][1];
	kalPointer->P[1][0] -= kalPointer->K[1] * kalPointer->P[0][0];
	kalPointer->P[1][1] -= kalPointer->K[1] * kalPointer->P[0][1];

	return kalPointer->angle;
};

void IMU::init()
{
	Kalman_Init(&kalmanX);
	Kalman_Init(&kalmanY);
	Kalman_Init(&kalmanZ);

	MPU6050Read();
	// sleep( 0.1 );
	double roll=0;
	double pitch=0;
	// IMU::RollPitchFromAccel(&roll, &pitch);

	kalmanX.angle = roll; // Set starting angle
	kalmanY.angle = pitch;
	gyroXAngle = roll;
	gyroYAngle = pitch;

	lastProcessed = micros();
	att_quat = Quaternion(1.0,0.0,0.0,0.0);
	att_vector = vector_ijk(0.0,0.0,-1.0);
	euler = { 0,0,0 };
	ESP_LOGD(FNAME, "Finished IMU setup  gyroYAngle:%f ", gyroYAngle);
}


float fused_yaw = 0;

double IMU::getRollRad() {
	return filterRoll*DEG_TO_RAD;
}

double IMU::getPitchRad()  {
	return -filterPitch*DEG_TO_RAD;
}



void IMU::read()
{
	double dt=0;
	bool ret=false;
	MPU6050Read();
	uint64_t rts = esp_timer_get_time();
	if( last_rts == 0 )
		ret=true;
	dt = (double)(rts - last_rts)/1000000.0;
	last_rts = rts;
	if( ret )
		return;
	float ax=0;
	float ay=0;
	float az=0;

	if( getTAS() > 10 ){
		// This part is a deterministic and noise resistant approach for centrifugal force removal
		// 1: exstimate roll angle from Z axis omega plus airspeed
		double roll=0;
		double pitch=0;
		IMU::RollPitchFromAccel(&roll, &pitch);
		// Z/Y cross axis rotation in 3D space
		myrollz = R2D(atan( ( sqrt( D2R(gyroZ)*D2R(gyroZ) + D2R(gyroY)*D2R(gyroY)) * (getTAS()/3.6) ) / 9.81 ));
		// 2: estimate angle of bank from increased acceleration in Z axis
		float posacc=accelZ;
		// only positive G-force is to be considered, curve at negative G is not defined
		if( posacc < 1 )
			posacc = 1;
		float accroll = R2D(acos( 1 / posacc ));
		// estimate sign of acceleration angle from gyro
		float sign_accroll=accroll;
		if( myrollz < 0 )
			sign_accroll = -sign_accroll;
		roll = -(myrollz + sign_accroll)/2;  // positive or left turn comes with negative roll in right handed system

		// Calculate Pitch from Gyro and acceleration
		// PitchFromAccel(&pitch);
		ax=(UINT16_MAX/2)*sin(D2R(pitch));
		ay=(UINT16_MAX/2)*sin(D2R(roll)) * cos( D2R(pitch));
		az=(UINT16_MAX/2)*cos(D2R(roll)) * cos( D2R(pitch));
	}
	else{ // Case when on ground, get accelerations from sensor directly
		ax=accelX;
		az=accelZ;
		ay=accelY;
	}
	// to get pitch and roll independent of circling, image pitch and roll values into 3D vector
	att_vector = update_fused_vector(att_vector,ax, ay, az,D2R(gyroX),D2R(gyroY),D2R(gyroZ),dt);
	att_quat = quaternion_from_accelerometer(att_vector.a,att_vector.b,att_vector.c);
	euler = att_quat.to_euler_angles();
	// treat gimbal lock, limit to 80 deg
	if( euler.roll > 80.0 )
		euler.roll = 80.0;
	if( euler.pitch > 80.0 )
		euler.pitch = 80.0;
	if( euler.roll < -80.0 )
		euler.roll = -80.0;
	if( euler.pitch < -80.0 )
		euler.pitch = -80.0;

	float curh = 0;
	if( compass ){
		bool ok;
		float x=compass->rawX();
		float y=compass->rawY();
		float z=-compass->rawZ();
		gravity_vector = vector_ijk(att_vector.a,att_vector.b,att_vector.c);
		gravity_vector.normalize();
		curh = compass->cur_heading( &ok );
		if( ok ){
			float gyroYaw = getGyroYawDelta();
			// tuned to plus 7% what gave the best timing swing in response, 2% for compass is far enough
			// gyro and compass are time displaced, gyro comes immediate, compass a second later
			fused_yaw +=  Vector::angleDiffDeg( curh ,fused_yaw )*0.02 - gyroYaw * 1.07;
			filterYaw=Vector::normalizeDeg( fused_yaw );
			compass->setGyroHeading( filterYaw );
			// ESP_LOGI( FNAME,"cur magn head %.2f gyro yaw: %.4f fused: %.1f Gyro(%.3f/%.3f/%.3f)", curh, gyroYaw, gh, gyroX, gyroY, gyroZ  );
		}
	}
	if( ahrs_gyro_factor.get() > 0.1  ){
		filterRoll =  euler.roll;
		filterPitch =  euler.pitch;
	}
	else{
		double roll=0;
		double pitch=0;
		kalXAngle = Kalman_GetAngle(&kalmanX, roll, 0, dt);
		filterRoll = kalXAngle;
		kalYAngle = Kalman_GetAngle(&kalmanY, pitch, 0, dt);
		filterPitch += (kalYAngle - filterPitch) * 0.2;   // addittional low pass filter
	}
	ESP_LOGI( FNAME,"GV-Pitch=%.1f  GV-Roll=%.1f filterYaw: %.2f curh: %.2f GX:%.3f GY:%.3f GZ:%.3f AX:%.3f AY:%.3f AZ:%.3f  FP:%.1f FR:%.1f", euler.pitch, euler.roll, filterYaw, curh, gyroX,gyroY,gyroZ, accelX, accelY, accelZ, filterPitch, filterRoll  );
}

// IMU Function Definition

void IMU::MPU6050Read()
{
	accelX = accelG[2];
	accelY = accelG[1];
	accelZ = -accelG[0];
	// Gating ignores Gyro drift < 1 deg per second
	gyroX = abs(gyroDPS.z) < 1.0 ? 0.0 :  -(gyroDPS.z);
	gyroY = abs(gyroDPS.y) < 1.0 ? 0.0 :   (gyroDPS.y);
	gyroZ = abs(gyroDPS.x) < 1.0 ? 0.0 :   (gyroDPS.x);
}

void IMU::PitchFromAccel(double *pitch)
{
	*pitch = atan2(-accelX, accelZ) * RAD_TO_DEG;
}

void IMU::RollPitchFromAccel(double *roll, double *pitch)
{
	// Source: http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf eq. 25 and eq. 26
	// atan2 outputs the value of -π to π (radians) - see http://en.wikipedia.org/wiki/Atan2
	// It is then converted from radians to degrees

	*roll = atan((double)accelY / hypotenuse((double)accelX, (double)accelZ)) * RAD_TO_DEG;
	*pitch = atan2((double)accelX, (double)accelZ) * RAD_TO_DEG;

	// ESP_LOGI( FNAME,"Accelerometer Roll: %f  Pitch: %f  (y:%f x:%f)", *roll, *pitch, (double)accelY, (double)accelX );

}
