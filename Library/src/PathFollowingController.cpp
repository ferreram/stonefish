//
//  PathFollowingController.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 22/06/2014.
//  Copyright (c) 2014-2017 Patryk Cieslak. All rights reserved.
//

#include "PathFollowingController.h"
#include "PathGenerator2D.h"

PathFollowingController::PathFollowingController(std::string uniqueName, PathGenerator* pathGenerator, Trajectory* positionSensor, btScalar frequency) : Controller(uniqueName, frequency)
{
    inputPath = pathGenerator;
    measuredTraj = positionSensor;
    
    if(pathGenerator->is3D())
        error.resize(7);
    else
        error.resize(4);
}

PathFollowingController::~PathFollowingController()
{
    delete inputPath;
    measuredTraj = NULL; //will be deleted with other sensors
    outputControllers.clear(); //will be deleted with other controllers
    error.clear();
}

PathGenerator* PathFollowingController::getPath()
{
    return inputPath;
}

ControllerType PathFollowingController::getType()
{
    return CONTROLLER_PATHFOLLOWING;
}

void PathFollowingController::RenderPath()
{
    if(inputPath->isRenderable())
    {
        inputPath->Render();
        
        btVector3 point;
        btVector3 tangent;
        inputPath->PointAtTime(inputPath->getTime(), point, tangent);
        std::vector<glm::vec3> vertices;
		vertices.push_back(glm::vec3(point.getX(), point.getY(), point.getZ()));
		OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::POINTS, vertices, CONTACT_COLOR);
	}
}

void PathFollowingController::Reset()
{
}

void PathFollowingController::Tick(btScalar dt)
{
    //Update position on path
    btVector3 desiredPoint;
    btVector3 desiredTangent;
    inputPath->MoveOnPath(VelocityOnPath() * dt, desiredPoint, desiredTangent);
    //printf("%1.5f\t%1.5f\n", desiredPoint.x(), desiredPoint.y());
    
    
    if(inputPath->getTime() >= btScalar(1.0)) //End of path reached
        PathEnd();
    else
    {
        if(inputPath->is3D()) //3D path following
        {
            
        }
        else //2D path following
        {
            PathGenerator2D* inputPath2D = (PathGenerator2D*)inputPath;
            
            //Get last measured position and attitude
            std::vector<btScalar> measured = measuredTraj->getLastSample().getData();
            btVector3 actualPoint = btVector3(measured[0], measured[1], measured[2]);
            
            //Calculate errors
            switch(inputPath2D->getPlane())
            {
                case PLANE_XY:
                {
                    btScalar desiredOrientation = btAtan2(desiredTangent.y(), desiredTangent.x());
                    btScalar actualOrientation = measured[5]; //Yaw
                    btVector3 positionDifference = desiredPoint - actualPoint;
                    btVector3 normalToPath = btVector3(-desiredTangent.y(), desiredTangent.x(), 0.0);
                    
                    error[0] = positionDifference.x(); //difference in x
                    error[1] = positionDifference.y(); //difference in y
                    error[2] = positionDifference.dot(normalToPath); //distance to path
                    
                    error[3] = desiredOrientation - actualOrientation;
                    error[3] = error[3] >= M_PI ? -(2*M_PI-error[3]) : (error[3] < -M_PI ? 2*M_PI + error[3] : error[3]); // error in [-pi,pi)
                }
                    break;
                    
                case PLANE_XZ:
                {
                    btScalar desiredOrientation = btAtan2(desiredTangent.z(), desiredTangent.x());
                    btScalar actualOrientation = measured[4]; //Pitch
                    btVector3 positionDifference = desiredPoint - actualPoint;
                    btVector3 normalToPath = btVector3(-desiredTangent.z(), 0.0, desiredTangent.x());
                    
                    error[0] = positionDifference.x(); //difference in x
                    error[1] = positionDifference.z(); //difference in z
                    error[2] = positionDifference.dot(normalToPath); //distance to path
                    error[3] = desiredOrientation - actualOrientation;
                }
                    break;
                    
                case PLANE_YZ:
                {
                    btScalar desiredOrientation = btAtan2(desiredTangent.z(), desiredTangent.y());
                    btScalar actualOrientation = measured[3]; //Roll
                    btVector3 positionDifference = desiredPoint - actualPoint;
                    btVector3 normalToPath = btVector3(0.0, -desiredTangent.z(), desiredTangent.y());
                    
                    error[0] = positionDifference.y(); //difference in y
                    error[1] = positionDifference.z(); //difference in z
                    error[2] = positionDifference.dot(normalToPath); //distance to path
                    error[3] = desiredOrientation - actualOrientation;
                }
                    break;
            }
        }
        
        //Run specific path following algorithm
        ControlTick(dt);
    }
}
