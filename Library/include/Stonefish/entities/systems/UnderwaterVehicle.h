//
//  UnderwaterVehicle.h
//  Stonefish
//
//  Created by Patryk Cieslak on 17/05/17.
//  Copyright(c) 2017 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_UnderwaterVehicle__
#define __Stonefish_UnderwaterVehicle__

#include <entities/SystemEntity.h>
#include <entities/SolidEntity.h>
#include <entities/systems/Manipulator.h>
#include <actuators/Thruster.h>
#include <sensors/Pressure.h>
#include <sensors/DVL.h>
#include <sensors/FOG.h>
#include <sensors/IMU.h>
#include <sensors/GPS.h>
#include <sensors/Odometry.h>

//! A dynamical model of a fully equipped underwater vehicle
/*!
 * This class implements a dynamical model of an underwater vehicle, equipped with thrusters, navigation sensors and manipulators.  
 * The navigation sensors include GPS, DVL and IMU. There can be any number of manipulators attached to the vehicle.
 */
class UnderwaterVehicle : public SystemEntity
{
public:
    UnderwaterVehicle(std::string uniqueName, SolidEntity* bodySolid);
    virtual ~UnderwaterVehicle();
    
    //Underwater vehicle
    void AddThruster(Thruster* thruster, const btTransform& location); //Location in the body geometry frame
    Odometry* AddOdometry(const btTransform& location, btScalar updateFrequency = btScalar(-1));
    Pressure* AddPressureSensor(const btTransform& location, btScalar updateFrequency = btScalar(-1));
    DVL* AddDVL(const btTransform& location, btScalar beamSpreadAngle, btScalar updateFrequency = btScalar(-1)); 
    FOG* AddFOG(const btTransform& location, btScalar updateFrequency = btScalar(-1));
    IMU* AddIMU(const btTransform& location, btScalar updateFrequency = btScalar(-1));
    GPS* AddGPS(const btTransform& location, btScalar homeLatitude, btScalar homeLongitude, btScalar updateFrequency = btScalar(-1));
    void SetThrusterSetpoint(unsigned int index, btScalar s);
    btScalar GetThrusterSetpoint(unsigned int index);
    btScalar GetThrusterVelocity(unsigned int index);
    
    //System
    void AddToDynamicsWorld(btMultiBodyDynamicsWorld* world, const btTransform& worldTransform);
	void GetAABB(btVector3& min, btVector3& max);
    void UpdateAcceleration(btScalar dt);
    void UpdateSensors(btScalar dt);
    void UpdateControllers(btScalar dt);
    void UpdateActuators(btScalar dt);
    void ApplyGravity(const btVector3& g);
	void ApplyDamping();
    
    SystemType getSystemType();
    virtual btTransform getTransform() const;
	FeatherstoneEntity* getVehicleBody();
    
    virtual std::vector<Renderable> Render();
    
private:
    //Subsystems
	std::vector<Thruster*> thrusters; // + FINS?
    std::vector<Sensor*> sensors;
    
	//Vehicle body
    FeatherstoneEntity* vehicleBody;
	btScalar vehicleBodyMass;     //Mass of vehicle body (internal + external parts)
    btVector3 vehicleBodyInertia; //Inertia of vehicle body (internal + external parts)
	btTransform localTransform; //...of vehicle body (CoG)
    eigMatrix6x6 aMass;
	eigMatrix6x6 dCS;
	btVector3 CoB;
	
	//Motion
	btVector3 lastLinearVel;
	btVector3 lastAngularVel;
    btVector3 linearAcc;
    btVector3 angularAcc;
    
	//Rendering
    bool showInternals;
};

#endif