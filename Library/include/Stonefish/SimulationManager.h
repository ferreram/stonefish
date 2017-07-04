//
//  SimulationManager.h
//  Stonefish
//
//  Created by Patryk Cieslak on 11/28/12.
//  Copyright (c) 2012-2017 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_SimulationManager__
#define __Stonefish_SimulationManager__

//Engine core
#include "ResearchDynamicsWorld.h"
#include "ResearchConstraintSolver.h"
#include "UnitSystem.h"
#include "NameManager.h"
#include "MaterialManager.h"

//Entities
#include "Entity.h"
#include "SolidEntity.h"
#include "FeatherstoneEntity.h"
#include "Ocean.h"
#include "SystemEntity.h"
#include "OpenGLTrackball.h"

//Dynamic elements
#include "Joint.h"
#include "Sensor.h"
#include "Contact.h"
#include "Actuator.h"
#include "Controller.h"

//Debugging and logging
#include "OpenGLDebugDrawer.h"
#include "Console.h"

typedef enum {DANTZIG, PROJ_GAUSS_SIEDEL, LEMKE} SolverType;
typedef enum {STANDARD, INCLUSIVE, EXCLUSIVE} CollisionFilteringType;
typedef enum {TERRESTIAL, MARINE, CUSTOM} SimulationType;

/*! 
    @class SimulationManager
    An abstract class managing all of the simulated entities, solver settings and equipped with custom callbacks.
 */
class SimulationManager
{
    friend class OpenGLPipeline;
    
public:
    SimulationManager(SimulationType t, UnitSystems unitSystem, btScalar stepsPerSecond, SolverType st = DANTZIG, CollisionFilteringType cft = STANDARD);
	virtual ~SimulationManager(void);
    
    //physics
    virtual void BuildScenario() = 0;
    bool SolveICProblem();
    void DestroyScenario();
    void RestartScenario();
    bool StartSimulation();
    void ResumeSimulation();
    void AdvanceSimulation(uint64_t timeInMicroseconds);
    void StopSimulation();
	
    void EnableOcean(Fluid* f = NULL);
	void AddEntity(Entity* ent);
    void AddSolidEntity(SolidEntity* ent, const btTransform& worldTransform);
    void AddSystemEntity(SystemEntity* ent, const btTransform& worldTransform);
    void AddJoint(Joint* jnt);
    void AddActuator(Actuator* act);
    void AddSensor(Sensor* sens);
    void AddController(Controller* cntrl);
    Contact* AddContact(Entity* entA, Entity* entB, size_type contactHistoryLength = 1);
    
	Entity* PickEntity(int x, int y);
    bool CheckContact(Entity* entA, Entity* entB);

    double getPhysicsTimeInMiliseconds();
    void setStepsPerSecond(btScalar steps);
    void setICSolverParams(bool useGravity, btScalar timeStep = btScalar(0.001), unsigned int maxIterations = 100000, btScalar maxTime = BT_LARGE_FLOAT, btScalar linearTolerance = btScalar(1e-6), btScalar angularTolerance = btScalar(1e-6));
    btScalar getStepsPerSecond();
    void getWorldAABB(btVector3& min, btVector3& max);
    CollisionFilteringType getCollisionFilter();
    SolverType getSolverType();
    
    Entity* getEntity(unsigned int index);
    Entity* getEntity(std::string name);
    Joint* getJoint(unsigned int index);
    Joint* getJoint(std::string name);
    Contact* getContact(unsigned int index);
    Contact* getContact(Entity* entA, Entity* entB);
    Actuator* getActuator(unsigned int index);
    Actuator* getActuator(std::string name);
    Sensor* getSensor(unsigned int index);
    Sensor* getSensor(std::string name);
    Controller* getController(unsigned int index);
    Controller* getController(std::string name);
    
    void setGravity(btScalar gravityConstant);
    btVector3 getGravity();
    ResearchDynamicsWorld* getDynamicsWorld();
    btScalar getSimulationTime();
    MaterialManager* getMaterialManager();
    bool isZAxisUp();
    
    bool drawCameraDummies;
    bool drawLightDummies;
    
protected:
    static void SolveICTickCallback(btDynamicsWorld* world, btScalar timeStep);
    static void SimulationTickCallback(btDynamicsWorld* world, btScalar timeStep);
    static void SimulationPostTickCallback(btDynamicsWorld* world, btScalar timeStep);
    static bool CustomMaterialCombinerCallback(btManifoldPoint& cp,	const btCollisionObjectWrapper* colObj0Wrap,int partId0,int index0,const btCollisionObjectWrapper* colObj1Wrap,int partId1,int index1);
    
    ResearchDynamicsWorld* dynamicsWorld;
    ResearchConstraintSolver* dwSolver;
    btCollisionDispatcher* dwDispatcher;
    btBroadphaseInterface* dwBroadphase;
    btDefaultCollisionConfiguration* dwCollisionConfig;
    
    MaterialManager* materialManager;
    
private:
    void InitializeSolver();
    void InitializeScenario();
    
    btScalar sps;
    btScalar simulationTime;
    uint64_t currentTime;
    uint64_t physicTime;
    uint64_t ssus;
    SolverType solver;
    CollisionFilteringType collisionFilter;
    bool icProblemSolved;
    bool icUseGravity;
    btScalar icTimeStep;
    unsigned int icMaxIter;
    btScalar icMaxTime;
    btScalar icLinTolerance;
    btScalar icAngTolerance;
    unsigned int mlcpFallbacks;
    
    std::vector<Entity*> entities;
    std::vector<Joint*> joints;
    std::vector<Sensor*> sensors;
    std::vector<Actuator*> actuators;
    std::vector<Controller*> controllers;
    std::vector<Contact*> contacts;
    Ocean* ocean;
    btScalar g;
    bool zUp;
    SimulationType simType;

    //graphics
    OpenGLTrackball* trackball;
    OpenGLDebugDrawer* debugDrawer;
};

#endif
